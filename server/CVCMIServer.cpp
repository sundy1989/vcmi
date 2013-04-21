#include "StdInc.h"

#include <boost/asio.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>

#include "../lib/filesystem/CResourceLoader.h"
#include "../lib/mapping/CCampaignHandler.h"
#include "../lib/CThreadHelper.h"
#include "../lib/Connection.h"
#include "../lib/CModHandler.h"
#include "../lib/CArtHandler.h"
#include "../lib/CDefObjInfoHandler.h"
#include "../lib/CGeneralTextHandler.h"
#include "../lib/CHeroHandler.h"
#include "../lib/CTownHandler.h"
#include "../lib/CBuildingHandler.h"
#include "../lib/CSpellHandler.h"
#include "../lib/CCreatureHandler.h"
#include "zlib.h"
#include "CVCMIServer.h"
#include "../lib/StartInfo.h"
#include "../lib/mapping/CMap.h"
#include "../lib/Interprocess.h"
#include "../lib/VCMI_Lib.h"
#include "../lib/VCMIDirs.h"
#include "CGameHandler.h"
#include "../lib/mapping/CMapInfo.h"
#include "../lib/CObjectHandler.h"
#include "../lib/GameConstants.h"
#include "../lib/logging/CBasicLogConfigurator.h"
#include "../lib/CConfigHandler.h"
#include "../lib/ScopeGuard.h"

#include "../lib/UnlockGuard.h"

std::string NAME_AFFIX = "server";
std::string NAME = GameConstants::VCMI_VERSION + std::string(" (") + NAME_AFFIX + ')'; //application name
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;
namespace intpr = boost::interprocess;
bool end2 = false;
int port = 3030;

/*
 * CVCMIServer.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

static void vaccept(tcp::acceptor *ac, tcp::socket *s, boost::system::error_code *error)
{
	ac->accept(*s,*error);
}



CPregameServer::CPregameServer(CConnection *Host, TAcceptor *Acceptor /*= NULL*/)
	: host(Host), listeningThreads(0), acceptor(Acceptor), upcomingConnection(NULL),
	  curmap(NULL), curStartInfo(NULL), state(RUNNING)
{
	initConnection(host);
}

void CPregameServer::handleConnection(CConnection *cpc)
{
	setThreadName("CPregameServer::handleConnection");
	try
	{
		while(!cpc->receivedStop)
		{
			CPackForSelectionScreen *cpfs = NULL;
			*cpc >> cpfs;

            logNetwork->infoStream() << "Got package to announce " << typeid(*cpfs).name() << " from " << *cpc;

			boost::unique_lock<boost::recursive_mutex> queueLock(mx);
			bool quitting = dynamic_cast<QuitMenuWithoutStarting*>(cpfs), 
				startingGame = dynamic_cast<StartWithCurrentSettings*>(cpfs);
			if(quitting || startingGame) //host leaves main menu or wants to start game -> we end
			{
				cpc->receivedStop = true;
				if(!cpc->sendStop)
					sendPack(cpc, *cpfs);

				if(cpc == host)
					toAnnounce.push_back(cpfs);
			}
			else
				toAnnounce.push_back(cpfs);

			if(startingGame)
			{
				//wait for sending thread to announce start
				auto unlock = vstd::makeUnlockGuard(mx);
				while(state == RUNNING) boost::this_thread::sleep(boost::posix_time::milliseconds(50));
			}
		}
	} 
	catch (const std::exception& e)
	{
		boost::unique_lock<boost::recursive_mutex> queueLock(mx);
        logNetwork->errorStream() << *cpc << " dies... \nWhat happened: " << e.what();
	}

	boost::unique_lock<boost::recursive_mutex> queueLock(mx);
	if(state != ENDING_AND_STARTING_GAME)
	{
		connections -= cpc;

		//notify other players about leaving
		PlayerLeft *pl = new PlayerLeft();
		pl->playerID = cpc->connectionID;
		announceTxt(cpc->name + " left the game");
		toAnnounce.push_back(pl);

		if(!connections.size())
		{
            logNetwork->errorStream() << "Last connection lost, server will close itself...";
			boost::this_thread::sleep(boost::posix_time::seconds(2)); //we should never be hasty when networking
			state = ENDING_WITHOUT_START;
		}
	}

    logNetwork->infoStream() << "Thread listening for " << *cpc << " ended";
	listeningThreads--;
	vstd::clear_pointer(cpc->handler);
}

void CPregameServer::run()
{
	startListeningThread(host);
	start_async_accept();

	while(state == RUNNING)
	{
		{
			boost::unique_lock<boost::recursive_mutex> myLock(mx);
			while(toAnnounce.size())
			{
				processPack(toAnnounce.front());
				toAnnounce.pop_front();
			}

// 			//we end sending thread if we ordered all our connections to stop
// 			ending = true;
// 			BOOST_FOREACH(CPregameConnection *pc, connections)
// 				if(!pc->sendStop)
// 					ending = false;

			if(state != RUNNING)
			{
                logNetwork->infoStream() << "Stopping listening for connections...";
				acceptor->close();
			}

			if(acceptor)
			{
				acceptor->get_io_service().reset();
				acceptor->get_io_service().poll();
			}
		} //frees lock

		boost::this_thread::sleep(boost::posix_time::milliseconds(50));
	}

    logNetwork->infoStream() << "Thread handling connections ended";

	if(state == ENDING_AND_STARTING_GAME)
	{
        logNetwork->infoStream() << "Waiting for listening thread to finish...";
		while(listeningThreads) boost::this_thread::sleep(boost::posix_time::milliseconds(50));
        logNetwork->infoStream() << "Preparing new game";
	}
}

CPregameServer::~CPregameServer()
{
	delete acceptor;
	delete upcomingConnection;

	BOOST_FOREACH(CPackForSelectionScreen *pack, toAnnounce)
		delete pack;

	toAnnounce.clear();

	//TODO pregameconnections
}

void CPregameServer::connectionAccepted(const boost::system::error_code& ec)
{
	if(ec)
	{
        logNetwork->infoStream() << "Something wrong during accepting: " << ec.message();
		return;
	}

    logNetwork->infoStream() << "We got a new connection! :)";
	CConnection *pc = new CConnection(upcomingConnection, NAME);
	initConnection(pc);
	upcomingConnection = NULL;

	*pc << (ui8)pc->connectionID << curmap;

	startListeningThread(pc);

	announceTxt(pc->name + " joins the game");
	PlayerJoined *pj = new PlayerJoined();
	pj->playerName = pc->name;
	pj->connectionID = pc->connectionID;
	toAnnounce.push_back(pj);

	start_async_accept();
}

void CPregameServer::start_async_accept()
{
	assert(!upcomingConnection);
	assert(acceptor);

	upcomingConnection = new TSocket(acceptor->get_io_service());
	acceptor->async_accept(*upcomingConnection, boost::bind(&CPregameServer::connectionAccepted, this, boost::asio::placeholders::error));
}

void CPregameServer::announceTxt(const std::string &txt, const std::string &playerName /*= "system"*/)
{
    logNetwork->infoStream() << playerName << " says: " << txt;
	ChatMessage cm;
	cm.playerName = playerName;
	cm.message = txt;

	boost::unique_lock<boost::recursive_mutex> queueLock(mx);
	toAnnounce.push_front(new ChatMessage(cm));
}

void CPregameServer::announcePack(const CPackForSelectionScreen &pack)
{
	BOOST_FOREACH(CConnection *pc, connections)
		sendPack(pc, pack);
}

void CPregameServer::sendPack(CConnection * pc, const CPackForSelectionScreen & pack)
{
	if(!pc->sendStop)
	{
        logNetwork->infoStream() << "\tSending pack of type " << typeid(pack).name() << " to " << *pc;
		*pc << &pack;
	}

	if(dynamic_cast<const QuitMenuWithoutStarting*>(&pack))
	{
		pc->sendStop = true;
	}
	else if(dynamic_cast<const StartWithCurrentSettings*>(&pack))
	{
		pc->sendStop = true;
	}
}

void CPregameServer::processPack(CPackForSelectionScreen * pack)
{
	if(dynamic_cast<CPregamePackToHost*>(pack))
	{
		sendPack(host, *pack);
	}
	else if(SelectMap *sm = dynamic_cast<SelectMap*>(pack))
	{
		vstd::clear_pointer(curmap);
		curmap = sm->mapInfo;
		sm->free = false;
		announcePack(*pack);
	}
	else if(UpdateStartOptions *uso = dynamic_cast<UpdateStartOptions*>(pack))
	{
		vstd::clear_pointer(curStartInfo);
		curStartInfo = uso->options;
		uso->free = false;
		announcePack(*pack);
	}
	else if(dynamic_cast<const StartWithCurrentSettings*>(pack))
	{
		state = ENDING_AND_STARTING_GAME;
		announcePack(*pack);
	}
	else
		announcePack(*pack);

	delete pack;
}

void CPregameServer::initConnection(CConnection *c)
{
	*c >> c->name;
	connections.insert(c);
    logNetwork->infoStream() << "Pregame connection with player " << c->name << " established!";
}

void CPregameServer::startListeningThread(CConnection * pc)
{	
	listeningThreads++;
	pc->enterPregameConnectionMode();
	pc->handler = new boost::thread(&CPregameServer::handleConnection, this, pc);
}

CVCMIServer::CVCMIServer()
: io(new boost::asio::io_service()), acceptor(new TAcceptor(*io, tcp::endpoint(tcp::v4(), port))), firstConnection(NULL)
{
    logNetwork->debugStream() << "CVCMIServer created!";
}
CVCMIServer::~CVCMIServer()
{
	//delete io;
	//delete acceptor;
	/delete firstConnection;
}

CGameHandler * CVCMIServer::initGhFromHostingConnection(CConnection &c)
{
	CGameHandler *gh = new CGameHandler();
	StartInfo si;
	c >> si; //get start options

	if(!si.createRandomMap)
	{
		bool mapFound = CResourceHandler::get()->existsResource(ResourceID(si.mapname, EResType::MAP));

		//TODO some checking for campaigns
		if(!mapFound && si.mode == StartInfo::NEW_GAME)
		{
			c << ui8(1); //WRONG!
			return nullptr;
		}
	}

	c << ui8(0); //OK!

	gh->init(&si);
	gh->conns.insert(&c);

	return gh;
}

void CVCMIServer::newGame()
{
	CConnection &c = *firstConnection;
	ui8 clients;
	c >> clients; //how many clients should be connected 
	assert(clients == 1); //multi goes now by newPregame, TODO: custom lobbies

	CGameHandler *gh = initGhFromHostingConnection(c);

	auto onExit = vstd::makeScopeGuard([&]()
	{
		vstd::clear_pointer(gh);
	});

	gh->run(false);
}

void CVCMIServer::newPregame()
{
	CPregameServer *cps = new CPregameServer(firstConnection, acceptor);
	cps->run();
	if(cps->state == CPregameServer::ENDING_WITHOUT_START)
	{
		delete cps;
		return;
	}

	if(cps->state == CPregameServer::ENDING_AND_STARTING_GAME)
	{
		CGameHandler gh;
		gh.conns = cps->connections;
		gh.init(cps->curStartInfo);

		BOOST_FOREACH(CConnection *c, gh.conns)
			c->addStdVecItems(gh.gs);

		gh.run(false);
	}
}

void CVCMIServer::start()
{
	ServerReady *sr = NULL;
	intpr::mapped_region *mr;
	try
	{
		intpr::shared_memory_object smo(intpr::open_only,"vcmi_memory",intpr::read_write);
		smo.truncate(sizeof(ServerReady));
		mr = new intpr::mapped_region(smo,intpr::read_write);
		sr = reinterpret_cast<ServerReady*>(mr->get_address());
	}
	catch(...)
	{
		intpr::shared_memory_object smo(intpr::create_only,"vcmi_memory",intpr::read_write);
		smo.truncate(sizeof(ServerReady));
		mr = new intpr::mapped_region(smo,intpr::read_write);
		sr = new(mr->get_address())ServerReady();
	}

	boost::system::error_code error;
    logNetwork->infoStream()<<"Listening for connections at port " << acceptor->local_endpoint().port();
	tcp::socket * s = new tcp::socket(acceptor->get_io_service());
	boost::thread acc(boost::bind(vaccept,acceptor,s,&error));
	sr->setToTrueAndNotify();
	delete mr;

	acc.join();
	if (error)
	{
        logNetwork->warnStream()<<"Got connection but there is an error " << error;
		return;
	}
    logNetwork->infoStream()<<"We've accepted someone... ";
	firstConnection = new CConnection(s,NAME);
    logNetwork->infoStream()<<"Got connection!";
	while(!end2)
	{
		ui8 mode;
		*firstConnection >> mode;
		switch (mode)
		{
		case 0:
			firstConnection->close();
			exit(0);
			break;
		case 1:
			firstConnection->close();
			return;
			break;
		case 2:
			newGame();
			break;
		case 3:
			loadGame();
			break;
		case 4:
			newPregame();
			break;
		}
	}
}

void CVCMIServer::loadGame()
{
	CConnection &c = *firstConnection;
	std::string fname;
	CGameHandler gh;
	boost::system::error_code error;
	ui8 clients;

	c >> clients >> fname; //how many clients should be connected - TODO: support more than one

// 	{
// 		char sig[8];
// 		CMapHeader dum;
// 		StartInfo *si;
// 
// 		CLoadFile lf(CResourceHandler::get()->getResourceName(ResourceID(fname, EResType::LIB_SAVEGAME)));
// 		lf >> sig >> dum >> si;
// 		logNetwork->infoStream() <<"Reading save signature";
// 
// 		lf >> *VLC;
// 		logNetwork->infoStream() <<"Reading handlers";
// 
// 		lf >> (gh.gs);
// 		c.addStdVecItems(gh.gs);
// 		logNetwork->infoStream() <<"Reading gamestate";
// 	}

	{
		CLoadFile lf(CResourceHandler::get()->getResourceName(ResourceID(fname, EResType::SERVER_SAVEGAME)));
		gh.loadCommonState(lf);
		lf >> gh;
	}

	c << ui8(0);

	CConnection* cc; //tcp::socket * ss;
	for(int i=0; i<clients; i++)
	{
		if(!i) 
		{
			cc = &c;
		}
		else
		{
			tcp::socket * s = new tcp::socket(acceptor->get_io_service());
			acceptor->accept(*s,error);
			if(error) //retry
			{
                logNetwork->warnStream()<<"Cannot establish connection - retrying...";
				i--;
				continue;
			}
			cc = new CConnection(s,NAME);
			cc->addStdVecItems(gh.gs);
		}	
		gh.conns.insert(cc);
	}

	gh.run(true);
}

int main(int argc, char** argv)
{
	console = new CConsoleHandler;
	CBasicLogConfigurator logConfig(VCMIDirs::get().localPath() + "/VCMI_Server_log.txt", console);
    logConfig.configureDefault();
	if(argc > 1)
	{
        port = std::stoi(argv[1]);
    }

	preinitDLL(console);
    settings.init();
    logConfig.configure();

    logNetwork->infoStream() << "Port " << port << " will be used.";
	loadDLLClasses();
	srand ( (ui32)time(NULL) );
	try
	{
		io_service io_service;
		CVCMIServer server;

		try
		{
			while(!end2)
			{
				server.start();
			}
			io_service.run();
		}
		catch(boost::system::system_error &e) //for boost errors just log, not crash - probably client shut down connection
		{
            logNetwork->errorStream() << e.what();
			end2 = true;
		}HANDLE_EXCEPTION
	}
	catch(boost::system::system_error &e)
	{
        logNetwork->errorStream() << e.what();
		//catch any startup errors (e.g. can't access port) errors
		//and return non-zero status so client can detect error
		throw;
	}
	//delete VLC; //can't be re-enabled due to access to already freed memory in bonus system
	CResourceHandler::clear();

  return 0;
}
