#include "../stdafx.h"
#include "CPreGame.h"
#include <ctime>
#include <boost/filesystem.hpp>   // includes all needed Boost.Filesystem declarations
#include <boost/algorithm/string.hpp>
#include <zlib.h>
#include "../timeHandler.h"
#include <sstream>
#include "SDL_Extensions.h"
#include "CGameInfo.h"
#include "CCursorHandler.h"
#include "../hch/CDefHandler.h"
#include "../hch/CDefObjInfoHandler.h"
#include "../hch/CGeneralTextHandler.h"
#include "../hch/CLodHandler.h"
#include "../hch/CTownHandler.h"
#include "../hch/CHeroHandler.h"
#include "../hch/CObjectHandler.h"
#include "../hch/CCampaignHandler.h"
#include "../hch/CCreatureHandler.h"
#include "../hch/CMusicHandler.h"
#include "../hch/CVideoHandler.h"
#include <cmath>
#include "Graphics.h"
//#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <cstdlib>
#include "../lib/Connection.h"
#include "../lib/VCMIDirs.h"
#include "../lib/map.h"
#include "AdventureMapButton.h"
#include "GUIClasses.h"
#include "CPlayerInterface.h"
#include "../CCallback.h"
#include <boost/lexical_cast.hpp>
#include <cstdlib>
#include "CMessage.h"
#include "../hch/CSpellHandler.h" /*for campaign bonuses*/
#include "../hch/CArtHandler.h" /*for campaign bonuses*/
#include "../hch/CBuildingHandler.h" /*for campaign bonuses*/
#include "CBitmapHandler.h"

/*
 * CPreGame.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
namespace fs = boost::filesystem;
using boost::bind;
using boost::ref;

void updateScreenLT(int maxW, int maxH);

#if _MSC_VER >= 1600
	#define bind boost::bind
	#define ref boost::ref
#endif

void startGame(StartInfo * options);

CGPreGame * CGP;
static const CMapInfo *curMap;
static StartInfo *curOpts;
static int playerColor; //if more than one player - applies to the first
static std::vector<std::string> playerNames; // serial id of name <-> player name

static std::string selectedName; //set when game is started/loaded

static void do_quit()
{
	SDL_Event event;
	event.quit.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

static CMapInfo *mapInfoFromGame()
{
	CMapInfo *ret = new CMapInfo();
	CMapHeader *headerCopy = new CMapHeader(*LOCPLINT->cb->getMapHeader()); //will be deleted by CMapInfo d-tor
	ret->setHeader(headerCopy);
	return ret;
}

static void setPlayersFromGame()
{
	playerColor = LOCPLINT->playerID;
}

static void clearInfo()
{
	delNull(curMap);
	delNull(curOpts);
	playerColor = -1;
	playerNames.clear();
}

void CMapInfo::countPlayers()
{
	actualHumanPlayers = playerAmnt = humenPlayers = 0;
	for(int i=0;i<PLAYER_LIMIT;i++)
	{
		if(mapHeader->players[i].canHumanPlay)
		{
			playerAmnt++;
			humenPlayers++;
		}
		else if(mapHeader->players[i].canComputerPlay)
		{
			playerAmnt++;
		}
	}

	if(scenarioOpts)
		for (std::map<int, PlayerSettings>::const_iterator i = scenarioOpts->playerInfos.begin(); i != scenarioOpts->playerInfos.end(); i++)
			if(i->second.human)
				actualHumanPlayers++;
}

//CMapInfo::CMapInfo(const std::string &fname, const unsigned char *map )
//: mapHeader(NULL), campaignHeader(NULL)
//{
//	init(fname, map);
//}

CMapInfo::CMapInfo(bool map)
: mapHeader(NULL), campaignHeader(NULL), scenarioOpts(NULL)
{
}

void CMapInfo::mapInit(const std::string &fname, const unsigned char *map )
{
	filename = fname;
	int i = 0;
	mapHeader = new CMapHeader();
	mapHeader->version = CMapHeader::invalid;

	try
	{
		mapHeader->initFromMemory(map, i);
		countPlayers();
	}
	catch (const std::string &e)
	{
		tlog1 << "\t\tWarning: evil map file: " << fname << ": " << e << std::endl; 
		delete mapHeader;
		mapHeader = NULL;
	}
}

CMapInfo::~CMapInfo()
{
	delete mapHeader;
	delete campaignHeader;
}

void CMapInfo::campaignInit()
{
	campaignHeader = new CCampaignHeader( CCampaignHandler::getHeader(filename, lodCmpgn) );
}

void CMapInfo::setHeader(CMapHeader *header)
{
	mapHeader = header;
}

CMenuScreen::CMenuScreen( EState which )
{
	OBJ_CONSTRUCTION;
	bgAd = NULL;

	switch(which)
	{
	case mainMenu:
		{
			buttons[0] = new AdventureMapButton("", CGI->generaltexth->zelp[3].second, bind(&CMenuScreen::moveTo, this, ref(CGP->scrs[newGame])), 540, 10, "ZMENUNG.DEF", SDLK_n);
			buttons[1] = new AdventureMapButton("", CGI->generaltexth->zelp[4].second, bind(&CMenuScreen::moveTo, this, ref(CGP->scrs[loadGame])), 532, 132, "ZMENULG.DEF", SDLK_l);
			buttons[2] = new AdventureMapButton("", CGI->generaltexth->zelp[5].second, 0, 524, 251, "ZMENUHS.DEF", SDLK_h);
			buttons[3] = new AdventureMapButton("", CGI->generaltexth->zelp[6].second, 0 /*cb*/, 557, 359, "ZMENUCR.DEF", SDLK_c);
			boost::function<void()> confWindow = bind(CInfoWindow::showYesNoDialog, ref(CGI->generaltexth->allTexts[69]), (const std::vector<SComponent*>*)0, do_quit, 0, false, 1);
			buttons[4] = new AdventureMapButton("", CGI->generaltexth->zelp[7].second, confWindow, 586, 468, "ZMENUQT.DEF", SDLK_ESCAPE);
		}
		break;
	case newGame:
		{
			bgAd = new CPicture(BitmapHandler::loadBitmap("ZNEWGAM.bmp"), 114, 312, true);
			buttons[0] = new AdventureMapButton("", CGI->generaltexth->zelp[10].second, bind(&CGPreGame::openSel, CGP, newGame, false), 545, 4, "ZTSINGL.DEF", SDLK_s);
			buttons[1] = new AdventureMapButton("", CGI->generaltexth->zelp[11].second, &pushIntT<CMultiMode>, 568, 120, "ZTMULTI.DEF", SDLK_m);
			buttons[2] = new AdventureMapButton("", CGI->generaltexth->zelp[12].second, bind(&CMenuScreen::moveTo, this, ref(CGP->scrs[campaignMain])), 541, 233, "ZTCAMPN.DEF", SDLK_c);
			buttons[3] = new AdventureMapButton("", CGI->generaltexth->zelp[13].second, 0 /*cb*/, 545, 358, "ZTTUTOR.DEF", SDLK_t);
			buttons[4] = new AdventureMapButton("", CGI->generaltexth->zelp[14].second, bind(&CMenuScreen::moveTo, this, CGP->scrs[mainMenu]), 582, 464, "ZTBACK.DEF", SDLK_ESCAPE);
		}
		break;
	case loadGame:
		{
			bgAd = new CPicture(BitmapHandler::loadBitmap("ZLOADGAM.bmp"), 114, 312, true);
			buttons[0] = new AdventureMapButton("", CGI->generaltexth->zelp[10].second, bind(&CGPreGame::openSel, CGP, loadGame, false), 545, 4, "ZTSINGL.DEF", SDLK_s);
			buttons[1] = new AdventureMapButton("", CGI->generaltexth->zelp[11].second, bind(&CGPreGame::openSel, CGP, loadGame, true), 568, 120, "ZTMULTI.DEF", SDLK_m);
			buttons[2] = new AdventureMapButton("", CGI->generaltexth->zelp[12].second, 0 /*cb*/, 541, 233, "ZTCAMPN.DEF", SDLK_c);
			buttons[3] = new AdventureMapButton("", CGI->generaltexth->zelp[13].second, 0 /*cb*/, 545, 358, "ZTTUTOR.DEF", SDLK_t);
			buttons[4] = new AdventureMapButton("", CGI->generaltexth->zelp[14].second, bind(&CMenuScreen::moveTo, this, CGP->scrs[mainMenu]), 582, 464, "ZTBACK.DEF", SDLK_ESCAPE);
		}
		break;
	case campaignMain:
		{
			buttons[0] = new AdventureMapButton("", "", 0 /*cb*/, 535, 8, "ZSSSOD.DEF", SDLK_s);
			buttons[1] = new AdventureMapButton("", "", 0 /*cb*/, 494, 117, "ZSSROE.DEF", SDLK_m);
			buttons[2] = new AdventureMapButton("", "", 0 /*cb*/, 486, 241, "ZSSARM.DEF", SDLK_c);
			buttons[3] = new AdventureMapButton("", "", bind(&CGPreGame::openSel, CGP, campaignList, false), 550, 358, "ZSSCUS.DEF", SDLK_t);
			buttons[4] = new AdventureMapButton("", "", bind(&CMenuScreen::moveTo, this, CGP->scrs[newGame]), 582, 464, "ZSSEXIT.DEF", SDLK_ESCAPE);

		}
		break;
	}

	for(int i = 0; i < ARRAY_COUNT(buttons); i++)
		buttons[i]->hoverable = true;
}

CMenuScreen::~CMenuScreen()
{
}

void CMenuScreen::showAll( SDL_Surface * to )
{
	blitAt(CGP->mainbg, 0, 0, to);
	CIntObject::showAll(to);
}

void CMenuScreen::show( SDL_Surface * to )
{
	CIntObject::show(to);
	CGI->videoh->update(pos.x + 8, pos.y + 105, screen, true, false);
}

void CMenuScreen::moveTo( CMenuScreen *next )
{
	GH.popInt(this);
	GH.pushInt(next);
}

CGPreGame::CGPreGame()
{
	GH.defActionsDef = 63;
	CGP = this;
	mainbg = BitmapHandler::loadBitmap("ZPIC1005.bmp");

	for(int i = 0; i < ARRAY_COUNT(scrs); i++)
		scrs[i] = new CMenuScreen((CMenuScreen::EState)i);
}

CGPreGame::~CGPreGame()
{
	SDL_FreeSurface(mainbg);

	for(int i = 0; i < ARRAY_COUNT(scrs); i++)
		delete scrs[i];
}

void CGPreGame::openSel( CMenuScreen::EState type, bool multi )
{
	resetPlayerNames();

	GH.pushInt(new CSelectionScreen(type, multi));
}

void CGPreGame::loadGraphics()
{
	victory = CDefHandler::giveDef("SCNRVICT.DEF");
	loss = CDefHandler::giveDef("SCNRLOSS.DEF");
	bonuses = CDefHandler::giveDef("SCNRSTAR.DEF");
	rHero = BitmapHandler::loadBitmap("HPSRAND1.bmp");
	rTown = BitmapHandler::loadBitmap("HPSRAND0.bmp");
	nHero = BitmapHandler::loadBitmap("HPSRAND6.bmp");
	nTown = BitmapHandler::loadBitmap("HPSRAND5.bmp");
}

void CGPreGame::disposeGraphics()
{
	delete victory;
	delete loss;
	SDL_FreeSurface(rHero);
	SDL_FreeSurface(nHero);
	SDL_FreeSurface(rTown);
	SDL_FreeSurface(nTown);
}

void CGPreGame::update()
{
	if (GH.listInt.size() == 0)
	{
	#ifdef _WIN32
		CGI->videoh->open("ACREDIT.SMK");
	#else
		CGI->videoh->open("ACREDIT.SMK", true, false);
	#endif
		GH.pushInt(scrs[CMenuScreen::mainMenu]);
	}

	CGI->curh->draw1();
	SDL_Flip(screen);
	CGI->curh->draw2();
	updateScreenLT(800, 600);
	GH.topInt()->show(screen);
	GH.updateTime();
	GH.handleEvents();
}

void CGPreGame::resetPlayerNames()
{
	playerNames.clear();
	playerNames.push_back(CGI->generaltexth->allTexts[434]); //we have only one player and his name is "Player"
}

CSelectionScreen::CSelectionScreen(CMenuScreen::EState Type, bool MultiPlayer)
	:multiPlayer(MultiPlayer)
{
	OBJ_CONSTRUCTION_CAPTURING_ALL;
	IShowActivable::type = BLOCK_ADV_HOTKEYS;
	pos.w = 762;
	pos.h = 584;
	if(Type == CMenuScreen::saveGame)
	{
		center(pos);
	}
	else if(Type == CMenuScreen::campaignList)
	{
		bg = new CPicture(BitmapHandler::loadBitmap("CamCust.bmp"), 0, 0, true);

		pos.x = 3;
		pos.y = 6;
	}
	else
	{
		pos.x = 3;
		pos.y = 6;
		bg = new CPicture(BitmapHandler::loadBitmap(rand()%2 ? "ZPIC1000.bmp" : "ZPIC1001.bmp"), -3, -6, true);
	}

	CGP->loadGraphics();
	type = Type;
	curOpts = &sInfo;
	sInfo.difficulty = 1;
	current = NULL;

	sInfo.mode = (Type == CMenuScreen::newGame ? StartInfo::NEW_GAME : StartInfo::LOAD_GAME);
	sInfo.turnTime = 0;
	curTab = NULL;

	card = new InfoCard(type); //right info card
	if (type == CMenuScreen::campaignList)
	{
		opt = NULL;
	} 
	else
	{
		opt = new OptionsTab(type/*, sInfo*/); //scenario options tab
		opt->recActions = DISPOSE;
	}
	sel = new SelectionTab(type, bind(&CSelectionScreen::changeSelection, this, _1), multiPlayer); //scenario selection tab
	sel->recActions = DISPOSE;

	switch(type)
	{
	case CMenuScreen::newGame:
		{
			card->difficulty->onChange = bind(&CSelectionScreen::difficultyChange, this, _1);
			card->difficulty->select(1, 0);
			AdventureMapButton *select = new AdventureMapButton(CGI->generaltexth->zelp[45], bind(&CSelectionScreen::toggleTab, this, sel), 411, 75, "GSPBUTT.DEF", SDLK_s);
			select->addTextOverlay(CGI->generaltexth->allTexts[500], FONT_SMALL);

			AdventureMapButton *opts = new AdventureMapButton(CGI->generaltexth->zelp[46], bind(&CSelectionScreen::toggleTab, this, opt), 411, 503, "GSPBUTT.DEF", SDLK_a);
			opts->addTextOverlay(CGI->generaltexth->allTexts[501], FONT_SMALL);

			AdventureMapButton *random = new AdventureMapButton(CGI->generaltexth->zelp[47], bind(&CSelectionScreen::toggleTab, this, sel), 411, 99, "GSPBUTT.DEF", SDLK_r);
			random->addTextOverlay(CGI->generaltexth->allTexts[740], FONT_SMALL);

			start  = new AdventureMapButton(CGI->generaltexth->zelp[103], bind(&CSelectionScreen::startGame, this), 411, 529, "SCNRBEG.DEF", SDLK_b);
		}
		break;
	case CMenuScreen::loadGame:
		sel->recActions = 255;
		start  = new AdventureMapButton(CGI->generaltexth->zelp[103], bind(&CSelectionScreen::startGame, this), 411, 529, "SCNRLOD.DEF", SDLK_l);
		break;
	case CMenuScreen::saveGame:
		sel->recActions = 255;
		start  = new AdventureMapButton("", CGI->generaltexth->zelp[103].second, bind(&CSelectionScreen::startGame, this), 411, 529, "SCNRSAV.DEF");
		break;
	case CMenuScreen::campaignList:
		sel->recActions = 255;
		start  = new AdventureMapButton(std::pair<std::string, std::string>(), bind(&CSelectionScreen::startCampaign, this), 411, 529, "SCNRLOD.DEF", SDLK_b);
		break;
	}
	

	start->assignedKeys.insert(SDLK_RETURN);

	std::string backName;
	if(Type == CMenuScreen::campaignList)
	{
		backName = "SCNRBACK.DEF";
	}
	else
	{
		backName = "SCNRBACK.DEF";
	}

	back = new AdventureMapButton("", CGI->generaltexth->zelp[105].second, bind(&CGuiHandler::popIntTotally, &GH, this), 581, 529, backName, SDLK_ESCAPE);
}

CSelectionScreen::~CSelectionScreen()
{
	curMap = NULL;
	curOpts = NULL;
	playerColor = -1;
	playerNames.clear();
}

void CSelectionScreen::toggleTab(CIntObject *tab)
{
	if(curTab && curTab->active)
	{
		curTab->deactivate();
		curTab->recActions = DISPOSE;
	}
	if(curTab != tab)
	{
		tab->recActions = 255;
		tab->activate();
		curTab = tab;
	}
	else
	{
		curTab = NULL;
	};
	GH.totalRedraw();
}

void CSelectionScreen::changeSelection( const CMapInfo *to )
{
	curMap = current = to;;

	if(to && type == CMenuScreen::loadGame)
		curOpts->difficulty = to->scenarioOpts->difficulty;
	if(type != CMenuScreen::campaignList && type != CMenuScreen::saveGame)
	{
		updateStartInfo(to, sInfo, to ? to->mapHeader : NULL);
	}
	card->changeSelection(to);
	if(type != CMenuScreen::campaignList)
	{
		opt->changeSelection(to ? to->mapHeader : NULL);
	}
}

void setPlayer(PlayerSettings &pset, unsigned player) 
{
	if(player < playerNames.size())
	{
		pset.name = playerNames[player];
		pset.human = true;
		if(playerColor < 0)
		{
			playerColor = pset.color;
			//playerSerial = pset.serial;
		}
	}
	else
	{
		pset.name = CGI->generaltexth->allTexts[468];//Computer
		pset.human = false;
	}
}

void CSelectionScreen::updateStartInfo( const CMapInfo * to, StartInfo & sInfo, const CMapHeader * mapHeader )
{
	sInfo.playerInfos.clear();
	if(!to) 
		return;

	/*sInfo.playerInfos.resize(to->playerAmnt);*/
	sInfo.mapname = to->filename;
	playerColor = -1;
	ui8 placedPlayers = 0;

	for (int i = 0; i < PLAYER_LIMIT; i++)
	{
		const PlayerInfo &pinfo = mapHeader->players[i];

		//neither computer nor human can play - no player
		if (!(pinfo.canComputerPlay || pinfo.canComputerPlay))
			continue;

		PlayerSettings &pset = sInfo.playerInfos[i];
		pset.color = i;
		if(pinfo.canHumanPlay)
			setPlayer(pset, placedPlayers++);
		else
			setPlayer(pset, -1);

		for (int j = 0; j < F_NUMBER  &&  pset.castle != -1; j++) //we start with none and find matching faction. if more than one, then set to random
		{
			if((1 << j) & pinfo.allowedFactions)
			{
				if (pset.castle >= 0) //we've already assigned a castle and another one is possible -> set random and let player choose
					pset.castle = -1; //breaks

				if (pset.castle == -2) //first available castle - pick
					pset.castle = j;
			}
		}

		if ((pinfo.generateHeroAtMainTown || mapHeader->version==CMapHeader::RoE)  &&  pinfo.hasMainTown  //we will generate hero in front of main town
			|| pinfo.p8) //random hero
			pset.hero = -1;
		else
			pset.hero = -2;

		if(pinfo.mainHeroName.length())
		{
			pset.heroName = pinfo.mainHeroName;
			if((pset.heroPortrait = pinfo.mainHeroPortrait) == 255)
				pset.heroPortrait = pinfo.p9;
		}
		pset.handicap = 0;
	}
}

void CSelectionScreen::startCampaign()
{
	CCampaign * ourCampaign = CCampaignHandler::getCampaign(curMap->filename, curMap->lodCmpgn);
	CCampaignState * campState = new CCampaignState();
	campState->camp = ourCampaign;
	GH.pushInt( new CBonusSelection(campState) );
}

void CSelectionScreen::startGame()
{
	if(type == CMenuScreen::newGame)
	{
		//there must be at least one human player before game can be started
		std::map<int, PlayerSettings>::const_iterator i;
		for(i = curOpts->playerInfos.begin(); i != curOpts->playerInfos.end(); i++)
			if(i->second.human)
				break;

		if(i == curOpts->playerInfos.end())
		{
			GH.pushInt(CInfoWindow::create(CGI->generaltexth->allTexts[530])); //You must position yourself prior to starting the game.
			return;
		}
	}

	if(type != CMenuScreen::saveGame)
	{
		if(!current)
			return;

		selectedName = sInfo.mapname;
		StartInfo *si = new StartInfo(sInfo);
		GH.popIntTotally(this); //delete me
		GH.popInt(GH.topInt()); //only deactivate main menu screen
		//curMap = NULL;
		curOpts = NULL;
		::startGame(si);
	}
	else
	{
		if(!(sel && sel->txt && sel->txt->text.size()))
			return;

		selectedName = GVCMIDirs.UserPath + "/Games/" + sel->txt->text + ".vlgm1";

		CFunctionList<void()> overWrite;
		overWrite += bind(&CCallback::save, LOCPLINT->cb, sel->txt->text);
		overWrite += bind(&CGuiHandler::popIntTotally, &GH, this);

		if(fs::exists(selectedName))
		{
			std::string hlp = CGI->generaltexth->allTexts[493]; //%s exists. Overwrite?
			boost::algorithm::replace_first(hlp, "%s", sel->txt->text);
			LOCPLINT->showYesNoDialog(hlp, std::vector<SComponent*>(), overWrite, 0, false);
		}
		else
			overWrite();

// 		LOCPLINT->cb->save(sel->txt->text);
// 		GH.popIntTotally(this);
	}
}

void CSelectionScreen::difficultyChange( int to )
{
	assert(type == CMenuScreen::newGame);
	sInfo.difficulty = to;
	GH.totalRedraw();
}

// A new size filter (Small, Medium, ...) has been selected. Populate
// selMaps with the relevant data.
void SelectionTab::filter( int size, bool selectFirst )
{
	curItems.clear();

	if(tabType == CMenuScreen::campaignList)
	{
		for (size_t i=0; i<allItems.size(); i++) 
			curItems.push_back(&allItems[i]);
	}
	else
	{
		for (size_t i=0; i<allItems.size(); i++) 
			if( allItems[i].mapHeader && allItems[i].mapHeader->version  &&  (!size || allItems[i].mapHeader->width == size))
				curItems.push_back(&allItems[i]);
	}

	if(curItems.size())
	{
		slider->block(false);
		slider->setAmount(curItems.size());
		sort();
		if(selectFirst)
		{
			slider->moveTo(0);
			onSelect(curItems[0]);
		}
		selectAbs(0);
	}
	else
	{
		slider->block(true);
		onSelect(NULL);
	}
}


void SelectionTab::getFiles(std::vector<FileInfo> &out, const std::string &dirname, const std::string &ext)
{
	if(!boost::filesystem::exists(dirname))
	{
		tlog1 << "Cannot find " << dirname << " directory!\n";
	}
	fs::path tie(dirname);
	fs::directory_iterator end_iter;
	for ( fs::directory_iterator file (tie); file!=end_iter; ++file )
	{
		if(fs::is_regular_file(file->status())
			&& boost::ends_with(file->path().filename(), ext))
		{
			std::time_t date = 0;
			try
			{
				date = fs::last_write_time(file->path());

				out.resize(out.size()+1);
				out.back().date = date;
				out.back().name = file->path().string();
			}
			catch(...)
			{
				tlog2 << "\t\tWarning: very corrupted map file: " << file->path().string() << std::endl; 
			}

		}
	}

	allItems.resize(out.size());
}

void SelectionTab::parseMaps(std::vector<FileInfo> &files, int start, int threads)
{
	int read=0;
	unsigned char mapBuffer[1500];

	while(start < allItems.size())
	{
		gzFile tempf = gzopen(files[start].name.c_str(),"rb");
		read = gzread(tempf, mapBuffer, 1500);
		gzclose(tempf);
		if(read < 50  ||  !mapBuffer[4])
		{
			tlog3 << "\t\tWarning: corrupted map file: " << files[start].name << std::endl; 
		}
		else //valid map
		{
			allItems[start].mapInit(files[start].name, mapBuffer);
			//allItems[start].date = "DATEDATE";// files[start].date;
		}
		start += threads;
	}
}

void SelectionTab::parseGames(std::vector<FileInfo> &files, bool multi)
{
	for(int i=0; i<files.size(); i++)
	{
		CLoadFile lf(files[i].name);
		if(!lf.sfile)
			continue;

		ui8 sign[8]; 
		lf >> sign;
		if(std::memcmp(sign,"VCMISVG",7))
		{
			tlog1 << files[i].name << " is not a correct savefile!" << std::endl;
			continue;
		}
		allItems[i].mapHeader = new CMapHeader();
		lf >> *(allItems[i].mapHeader) >> allItems[i].scenarioOpts;
		allItems[i].filename = files[i].name;
		allItems[i].countPlayers();
		allItems[i].date = std::asctime(std::localtime(&files[i].date));

		if((allItems[i].actualHumanPlayers > 1) != multi) //if multi mode then only multi games, otherwise single
		{
			delete allItems[i].mapHeader;
			allItems[i].mapHeader = NULL;
		}
	}
}

void SelectionTab::parseCampaigns( std::vector<FileInfo> & files )
{
	for(int i=0; i<files.size(); i++)
	{
		//allItems[i].date = std::asctime(std::localtime(&files[i].date));
		allItems[i].filename = files[i].name;
		allItems[i].lodCmpgn = files[i].inLod;
		allItems[i].campaignInit();
	}
}

SelectionTab::SelectionTab(CMenuScreen::EState Type, const boost::function<void(CMapInfo *)> &OnSelect, bool MultiPlayer)
	:onSelect(OnSelect), bg(NULL)
{
	OBJ_CONSTRUCTION;
	selectionPos = 0;
	used = LCLICK | WHEEL | KEYBOARD | DOUBLECLICK;
	slider = NULL;
	txt = NULL;
	tabType = Type;

	if (Type != CMenuScreen::campaignList)
	{
		bg = new CPicture(BitmapHandler::loadBitmap("SCSELBCK.bmp"), 0, 0, true);
		pos.w = bg->pos.w;
		pos.h = bg->pos.h;
	}
	else
	{
		SDL_Surface * tmp1 = BitmapHandler::loadBitmap("CAMCUST.bmp");
		SDL_Surface * tmp = CSDL_Ext::newSurface(400, tmp1->h);
		blitAt(tmp1, 0, 0, tmp);
		SDL_FreeSurface(tmp1);
		bg = new CPicture(tmp, 0, 0, true);
		pos.w = bg->pos.w;
		pos.h = bg->pos.h;
		bg->pos.x = bg->pos.y = 0;
	}

	std::vector<FileInfo> toParse;
	std::vector<CCampaignHeader> cpm;
	switch(tabType)
	{
	case CMenuScreen::newGame:
		getFiles(toParse, DATA_DIR "/Maps", "h3m"); //get all maps
		parseMaps(toParse);
		positions = 18;
		break;

	case CMenuScreen::loadGame:
	case CMenuScreen::saveGame:
		getFiles(toParse, GVCMIDirs.UserPath + "/Games", "vlgm1"); //get all saves
		parseGames(toParse, MultiPlayer);
		if(tabType == CMenuScreen::loadGame)
		{
			positions = 18;
		}
		else
		{
			positions = 16;
		}	
		if(tabType == CMenuScreen::saveGame)
			txt = new CTextInput(Rect(32, 539, 350, 20), Point(-32, -25), "GSSTRIP.bmp", 0);
		break;
	case CMenuScreen::campaignList:
		getFiles(toParse, DATA_DIR "/Maps", "h3c"); //get all campaigns
		for (int g=0; g<toParse.size(); ++g)
		{
			toParse[g].inLod = false;
		}
		//add lod cmpgns
		cpm = CCampaignHandler::getCampaignHeaders(CCampaignHandler::ALL);
		for (int g = 0; g < cpm.size(); g++)
		{
			FileInfo fi;
			fi.inLod = cpm[g].loadFromLod;
			fi.name = cpm[g].filename;
			toParse.push_back(fi);
			if (cpm[g].loadFromLod)
			{
				allItems.push_back(CMapInfo(false));
			}
		}
		parseCampaigns(toParse);
		positions = 18;
		break;

	default:
		assert(0);
		break;
	}


	
	if (tabType != CMenuScreen::campaignList)
	{
		//size filter buttons
		{
			int sizes[] = {36, 72, 108, 144, 0};
			const char * names[] = {"SCSMBUT.DEF", "SCMDBUT.DEF", "SCLGBUT.DEF", "SCXLBUT.DEF", "SCALBUT.DEF"};
			for(int i = 0; i < 5; i++)
				new AdventureMapButton("", CGI->generaltexth->zelp[54+i].second, bind(&SelectionTab::filter, this, sizes[i], true), 158 + 47*i, 46, names[i]);
		}

		//sort buttons buttons
		{
			int xpos[] = {23, 55, 88, 121, 306, 339};
			const char * names[] = {"SCBUTT1.DEF", "SCBUTT2.DEF", "SCBUTCP.DEF", "SCBUTT3.DEF", "SCBUTT4.DEF", "SCBUTT5.DEF"};
			for(int i = 0; i < 6; i++)
				new AdventureMapButton("", CGI->generaltexth->zelp[107+i].second, bind(&SelectionTab::sortBy, this, i), xpos[i], 86, names[i]);
		}
	}
	else
	{
		//sort by buttons
		new AdventureMapButton("", "", bind(&SelectionTab::sortBy, this, _numOfMaps), 23, 86, "CamCusM.DEF"); //by num of maps
		new AdventureMapButton("", "", bind(&SelectionTab::sortBy, this, _name), 55, 86, "CamCusL.DEF"); //by name
	}

	slider = new CSlider(372, 86, tabType != CMenuScreen::saveGame ? 480 : 430, bind(&SelectionTab::sliderMove, this, _1), positions, curItems.size(), 0, false, 1);
	slider->changeUsedEvents(WHEEL, true);
	format =  CDefHandler::giveDef("SCSELC.DEF");

	sortingBy = _format;
	ascending = true;
	filter(0);
	//select(0);
	switch(tabType)
	{
	case CMenuScreen::newGame:
		selectFName(DATA_DIR "/Maps/Arrogance.h3m");
		break;
	case CMenuScreen::loadGame:
	case CMenuScreen::campaignList:
		select(0);
		break;
	case CMenuScreen::saveGame:;
		if(selectedName.size())
		{
			if(selectedName[2] == 'M') //name starts with ./Maps instead of ./Games => there was nothing to select
				txt->setText("NEWGAME");
			else
				selectFName(selectedName);
		}
	}
}

SelectionTab::~SelectionTab()
{
	delete format;
}

void SelectionTab::sortBy( int criteria )
{
	if(criteria == sortingBy)
	{
		ascending = !ascending;
	}
	else
	{
		sortingBy = (ESortBy)criteria;
		ascending = true;
	}
	sort();

	selectAbs(0);
}

void SelectionTab::sort()
{
	if(sortingBy != _name)
		std::stable_sort(curItems.begin(), curItems.end(), mapSorter(_name));
	std::stable_sort(curItems.begin(), curItems.end(), mapSorter(sortingBy));

	if(!ascending)
		std::reverse(curItems.begin(), curItems.end());

	redraw();
}

void SelectionTab::select( int position )
{
	if(!curItems.size()) return;

	// New selection. py is the index in curItems.
	int py = position + slider->value;
	amax(py, 0);
	amin(py, curItems.size()-1);

	selectionPos = py;

	if(position < 0)
		slider->moveTo(slider->value + position);
	else if(position >= positions)
		slider->moveTo(slider->value + position - positions + 1);

	if(txt)
		txt->setText(fs::basename(curItems[py]->filename));

	onSelect(curItems[py]);
}

void SelectionTab::selectAbs( int position )
{
	select(position - slider->value);
}

int SelectionTab::getPosition( int x, int y )
{
	return -1;
}

void SelectionTab::sliderMove( int slidPos )
{
	if(!slider) return; //ignore spurious call when slider is being created
	redraw();
}


// Display the tab with the scenario names
//
// elemIdx is the index of the maps or saved game to display on line 0
// slider->capacity contains the number of available screen lines
// slider->positionsAmnt is the number of elements after filtering
void SelectionTab::printMaps(SDL_Surface *to)
{

	int elemIdx = slider->value;

	// Display all elements if there's enough space
	//if(slider->amount < slider->capacity)
	//	elemIdx = 0;


	SDL_Color itemColor;
#define POS(xx, yy) pos.x + xx, pos.y + yy + line*25
	for (int line = 0; line < positions  &&  elemIdx < curItems.size(); elemIdx++, line++)
	{
		CMapInfo* curMap = curItems[elemIdx];

		if (elemIdx == selectionPos)
			itemColor=tytulowy;
		else 
			itemColor=zwykly;

		if(tabType != CMenuScreen::campaignList)
		{
			//amount of players
			std::ostringstream ostr(std::ostringstream::out);
			ostr << curMap->playerAmnt << "/" << curMap->humenPlayers;
			CSDL_Ext::printAt(ostr.str(), POS(29, 120), FONT_SMALL, itemColor, to);

			//map size
			std::string temp2 = "C";
			switch (curMap->mapHeader->width)
			{
			case 36:
				temp2="S";
				break;
			case 72:
				temp2="M";
				break;
			case 108:
				temp2="L";
				break;
			case 144:
				temp2="XL";
				break;
			}
			CSDL_Ext::printAtMiddle(temp2, POS(70, 128), FONT_SMALL, itemColor, to);

			int temp=-1;
			switch (curMap->mapHeader->version)
			{
			case CMapHeader::RoE:
				temp=0;
				break;
			case CMapHeader::AB:
				temp=1;
				break;
			case CMapHeader::SoD:
				temp=2;
				break;
			case CMapHeader::WoG:
				temp=3;
				break;
			default:
				// Unknown version. Be safe and ignore that map
				tlog2 << "Warning: " << curMap->filename << " has wrong version!\n";
				continue;
			}
			blitAt(format->ourImages[temp].bitmap, POS(88, 117), to);

			//victory conditions
			if (curMap->mapHeader->victoryCondition.condition == winStandard)
				temp = 11;
			else 
				temp = curMap->mapHeader->victoryCondition.condition;
			blitAt(CGP->victory->ourImages[temp].bitmap, POS(306, 117), to);

			//loss conditions
			if (curMap->mapHeader->lossCondition.typeOfLossCon == lossStandard)
				temp=3;
			else 
				temp=curMap->mapHeader->lossCondition.typeOfLossCon;
			blitAt(CGP->loss->ourImages[temp].bitmap, POS(339, 117), to);
		}
		else //if campaign
		{
			//number of maps in campaign
			std::ostringstream ostr(std::ostringstream::out);
			ostr << CGI->generaltexth->campaignRegionNames[ curMap->campaignHeader->mapVersion ].size();
			CSDL_Ext::printAt(ostr.str(), POS(29, 120), FONT_SMALL, itemColor, to);
		}

		std::string name;
		if(tabType != CMenuScreen::campaignList)
		{
			if (!curMap->mapHeader->name.length())
				curMap->mapHeader->name = "Unnamed";
			name = curMap->mapHeader->name;
		}
		else
		{
			name = curMap->campaignHeader->name;
		}

		//print name
		if (tabType == CMenuScreen::newGame || tabType == CMenuScreen::campaignList) {
			
			CSDL_Ext::printAtMiddle(name, POS(213, 128), FONT_SMALL, itemColor, to);
		} else
			CSDL_Ext::printAtMiddle(fs::basename(curMap->filename), POS(213, 128), FONT_SMALL, itemColor, to);
	
	}
#undef POS
}

void SelectionTab::showAll( SDL_Surface * to )
{
	CIntObject::showAll(to);
	printMaps(to);

	std::string title;
	switch(tabType) {
	case CMenuScreen::newGame:
		title = CGI->generaltexth->arraytxt[229];
		break;
	case CMenuScreen::loadGame:
		title = CGI->generaltexth->arraytxt[230];
		break;
	case CMenuScreen::saveGame:
		title = CGI->generaltexth->arraytxt[231];
		break;
	case CMenuScreen::campaignList:
		title = "Select a Campaign"; //TODO: find where is the title
		break;
	}

	CSDL_Ext::printAtMiddle(title, pos.x+205, pos.y+28, FONT_MEDIUM, tytulowy, to); //Select a Scenario to Play
	if(tabType != CMenuScreen::campaignList)
	{
		CSDL_Ext::printAtMiddle(CGI->generaltexth->allTexts[510], pos.x+87, pos.y+62, FONT_SMALL, tytulowy, to); //Map sizes
	}
}

void SelectionTab::clickLeft( tribool down, bool previousState )
{
	if(down)
	{
		int line = getLine();
		if(line != -1)
			select(line);
	}
}
void SelectionTab::keyPressed( const SDL_KeyboardEvent & key )
{
	if(key.state != SDL_PRESSED) return;

	int moveBy = 0;
	switch(key.keysym.sym)
	{
	case SDLK_UP:
		moveBy = -1;
		break;
	case SDLK_DOWN:
		moveBy = +1;
		break;
	case SDLK_PAGEUP:
		moveBy = -positions+1;
		break;
	case SDLK_PAGEDOWN:
		moveBy = +positions-1; 
		break;
	case SDLK_HOME:
		select(-slider->value);
		return;
	case SDLK_END:
		select(curItems.size() - slider->value);
		return;
	default:
		return;
	}
	select(selectionPos - slider->value + moveBy); 
}

void SelectionTab::onDoubleClick()
{
	if(getLine() != -1) //double clicked scenarios list
	{
		//act as if start button was pressed
		(static_cast<CSelectionScreen*>(parent))->start->callback();
	}
}

int SelectionTab::getLine()
{
	int line = -1;
	Point clickPos(GH.current->button.x, GH.current->button.y);
	clickPos = clickPos - pos.topLeft();

	if (clickPos.y > 115  &&  clickPos.y < 564  &&  clickPos.x > 22  &&  clickPos.x < 371)
	{
		line = (clickPos.y-115) / 25; //which line
	}

	return line;
}

void SelectionTab::selectFName( const std::string &fname )
{
	for(int i = curItems.size() - 1; i >= 0; i--)
	{
		if(curItems[i]->filename == fname)
		{
			slider->moveTo(i);
			selectAbs(i);
			return;
		}
	}

	selectAbs(0);
}



InfoCard::InfoCard( CMenuScreen::EState Type )
: difficulty(NULL), sizes(NULL), sFlags(NULL), bg(NULL)
{
	OBJ_CONSTRUCTION;
	pos.x += 393;
	used = RCLICK;
	mapDescription = NULL;
	type = Type;

	Rect descriptionRect(26, 149, 320, 115);
	mapDescription = new CTextBox("", descriptionRect, 1);

	if(type == CMenuScreen::campaignList)
	{
		CSelectionScreen *ss = static_cast<CSelectionScreen*>(parent);
		moveChild(new CPicture(*ss->bg, descriptionRect + Point(-393, 0)), this, mapDescription, true); //move subpicture bg to our description control (by default it's our (Infocard) child)
	}
	else
	{
		bg = new CPicture(BitmapHandler::loadBitmap("GSELPOP1.bmp"), 0, 0, true);
		std::swap(children.front(), children.back());
		pos.w = bg->pos.w;
		pos.h = bg->pos.h;
		sizes = CDefHandler::giveDef("SCNRMPSZ.DEF");
		sFlags = CDefHandler::giveDef("ITGFLAGS.DEF");
		difficulty = new CHighlightableButtonsGroup(0);
		{
			static const char *difButns[] = {"GSPBUT3.DEF", "GSPBUT4.DEF", "GSPBUT5.DEF", "GSPBUT6.DEF", "GSPBUT7.DEF"};
			BLOCK_CAPTURING;

			for(int i = 0; i < 5; i++)
			{
				difficulty->addButton(new CHighlightableButton("", CGI->generaltexth->zelp[24+i].second, 0, 110 + i*32, 450, difButns[i], i));
				difficulty->buttons.back()->pos += pos.topLeft();
			}
		}

		if(type != CMenuScreen::newGame)
			difficulty->block(true);

		//description needs bg
		moveChild(new CPicture(*bg, descriptionRect), this, mapDescription, true); //move subpicture bg to our description control (by default it's our (Infocard) child)
	}

}

InfoCard::~InfoCard()
{
	delete sizes;
	delete sFlags;
}

void InfoCard::showAll( SDL_Surface * to )
{
	CIntObject::showAll(to);

	//blit texts
	if(type != CMenuScreen::campaignList)
	{
		printAtLoc(CGI->generaltexth->allTexts[390] + ":", 24, 400, FONT_SMALL, zwykly, to); //Allies
		printAtLoc(CGI->generaltexth->allTexts[391] + ":", 190, 400, FONT_SMALL, zwykly, to); //Enemies
		printAtLoc(CGI->generaltexth->allTexts[494], 33, 430, FONT_SMALL, tytulowy, to);//"Map Diff:"
		printAtLoc(CGI->generaltexth->allTexts[492] + ":", 133,430, FONT_SMALL, tytulowy, to); //player difficulty
		printAtLoc(CGI->generaltexth->allTexts[218] + ":", 290,430, FONT_SMALL, tytulowy, to); //"Rating:"
		printAtLoc(CGI->generaltexth->allTexts[495], 26, 22, FONT_SMALL, tytulowy, to); //Scenario Name:
		printAtLoc(CGI->generaltexth->allTexts[496], 26, 132, FONT_SMALL, tytulowy, to); //Scenario Description:
		printAtLoc(CGI->generaltexth->allTexts[497], 26, 283, FONT_SMALL, tytulowy, to); //Victory Condition:
		printAtLoc(CGI->generaltexth->allTexts[498], 26, 339, FONT_SMALL, tytulowy, to); //Loss Condition:
	}

	if(curMap)
	{
		if(type != CMenuScreen::campaignList)
		{
			if(type != CMenuScreen::newGame)
			{
				for (int i = 0; i < difficulty->buttons.size(); i++)
				{
					//if(i == curMap->difficulty)
					//	difficulty->buttons[i]->state = 3;
					//else
					//	difficulty->buttons[i]->state = 2;

					difficulty->buttons[i]->showAll(to);
				}
			}

			//victory conditions
			int temp = curMap->mapHeader->victoryCondition.condition+1;
			if (temp>20) temp=0;
			std::string sss = CGI->generaltexth->victoryConditions[temp];
			if (temp && curMap->mapHeader->victoryCondition.allowNormalVictory) sss+= "/" + CGI->generaltexth->victoryConditions[0];
			printAtLoc(sss, 60, 307, FONT_SMALL, zwykly, to);


			//loss conditoins
			temp = curMap->mapHeader->lossCondition.typeOfLossCon+1;
			if (temp>20) temp=0;
			sss = CGI->generaltexth->lossCondtions[temp];
			printAtLoc(sss, 60, 366, FONT_SMALL, zwykly, to);

			//difficulty
			assert(curMap->mapHeader->difficulty <= 4);
			std::string &diff = CGI->generaltexth->arraytxt[142 + curMap->mapHeader->difficulty];
			printAtMiddleLoc(diff, 62, 472, FONT_SMALL, zwykly, to);

			//selecting size icon
			switch (curMap->mapHeader->width)
			{
			case 36:
				temp=0;
				break;
			case 72:
				temp=1;
				break;
			case 108:
				temp=2;
				break;
			case 144:
				temp=3;
				break;
			default:
				temp=4;
				break;
			}
			blitAtLoc(sizes->ourImages[temp].bitmap, 318, 22, to);

			//conditions
			temp = curMap->mapHeader->victoryCondition.condition;
			if (temp>12) temp=11;
			blitAtLoc(CGP->victory->ourImages[temp].bitmap, 24, 302, to); //victory cond descr
			temp=curMap->mapHeader->lossCondition.typeOfLossCon;
			if (temp>12) temp=3;
			blitAtLoc(CGP->loss->ourImages[temp].bitmap, 24, 359, to); //loss cond 

			if(type == CMenuScreen::loadGame)
				printToLoc((static_cast<const CMapInfo*>(curMap))->date,308,34, FONT_SMALL, zwykly, to);

			//print flags
			int fx=64, ex=244, myT;
			//if (curMap->howManyTeams)
			myT = curMap->mapHeader->players[playerColor].team;
			//else 
			//	myT = -1;
			for (std::map<int, PlayerSettings>::const_iterator i = curOpts->playerInfos.begin(); i != curOpts->playerInfos.end(); i++)
			{
				int *myx = ((i->first == playerColor  ||  curMap->mapHeader->players[i->first].team == myT) ? &fx : &ex);
				blitAtLoc(sFlags->ourImages[i->first].bitmap, *myx, 399, to);
				*myx += sFlags->ourImages[i->first].bitmap->w;
			}

			std::string tob;
			switch (curOpts->difficulty)
			{
			case 0:
				tob="80%";
				break;
			case 1:
				tob="100%";
				break;
			case 2:
				tob="130%";
				break;
			case 3:
				tob="160%";
				break;
			case 4:
				tob="200%";
				break;
			}
			printAtMiddleLoc(tob, 311, 472, FONT_SMALL, zwykly, to);
		}

		//blit description
		std::string name;

		if (type == CMenuScreen::campaignList)
		{
			name = curMap->campaignHeader->name;
		} 
		else
		{
			name = curMap->mapHeader->name;
		}

		//name
		if (name.length())
			printAtLoc(name, 26, 39, FONT_BIG, tytulowy, to);
		else 
			printAtLoc("Unnamed", 26, 39, FONT_BIG, tytulowy, to);

		
	}
}

void InfoCard::changeSelection( const CMapInfo *to )
{
	if(to && mapDescription)
	{

		if (type == CMenuScreen::campaignList)
			mapDescription->setTxt(to->campaignHeader->description);
		else
			mapDescription->setTxt(to->mapHeader->description);

		if(type != CMenuScreen::newGame && type != CMenuScreen::campaignList)
			difficulty->select(curOpts->difficulty, 0);
	}
	GH.totalRedraw();
}

void InfoCard::clickRight( tribool down, bool previousState )
{
	static const Rect flagArea(19, 397, 335, 23);
	if(down && curMap && isItInLoc(flagArea, GH.current->motion.x, GH.current->motion.y))
		showTeamsPopup();
}

void InfoCard::showTeamsPopup()
{
	SDL_Surface *bmp = CMessage::drawBox1(256, 90 + 50 * curMap->mapHeader->howManyTeams);
	CSDL_Ext::printAtMiddle(CGI->generaltexth->allTexts[657], 128, 30, FONT_MEDIUM, tytulowy, bmp); //{Team Alignments}

	for(int i = 0; i < curMap->mapHeader->howManyTeams; i++)
	{
		std::vector<ui8> flags;
		std::string hlp = CGI->generaltexth->allTexts[656]; //Team %d
		hlp.replace(hlp.find("%d"), 2, boost::lexical_cast<std::string>(i+1));
		CSDL_Ext::printAtMiddle(hlp, 128, 65 + 50*i, FONT_SMALL, zwykly, bmp);

		for(int j = 0; j < PLAYER_LIMIT; j++)
			if((curMap->mapHeader->players[j].canHumanPlay || curMap->mapHeader->players[j].canComputerPlay)
				&& curMap->mapHeader->players[j].team == i)
				flags.push_back(j);

		int curx = 128 - 9*flags.size();
		for(int j = 0; j < flags.size(); j++)
		{
			blitAt(sFlags->ourImages[flags[j]].bitmap, curx, 75 + 50*i, bmp);
			curx += 18;
		}	
	}

	GH.pushInt(new CInfoPopup(bmp, true));
}

OptionsTab::OptionsTab( CMenuScreen::EState Type)
:type(Type)
{
	OBJ_CONSTRUCTION;
	bg = new CPicture(BitmapHandler::loadBitmap("ADVOPTBK.bmp"), 0, 0, true);
	pos = bg->pos;

	if(type == CMenuScreen::newGame)
		turnDuration = new CSlider(55, 551, 194, bind(&OptionsTab::setTurnLength, this, _1), 1, 11, 11, true, 1);
}

OptionsTab::~OptionsTab()
{

}

void OptionsTab::showAll( SDL_Surface * to )
{
	CIntObject::showAll(to);
	printAtMiddleLoc(CGI->generaltexth->allTexts[515], 222, 30, FONT_BIG, tytulowy, to);
	printAtMiddleWBLoc(CGI->generaltexth->allTexts[516], 222, 58, FONT_SMALL, 55, zwykly, to); //Select starting options, handicap, and name for each player in the game.
	printAtMiddleWBLoc(CGI->generaltexth->allTexts[517], 107, 102, FONT_SMALL, 14, tytulowy, to); //Player Name Handicap Type
	printAtMiddleWBLoc(CGI->generaltexth->allTexts[518], 197, 102, FONT_SMALL, 10, tytulowy, to); //Starting Town
	printAtMiddleWBLoc(CGI->generaltexth->allTexts[519], 273, 102, FONT_SMALL, 10, tytulowy, to); //Starting Hero
	printAtMiddleWBLoc(CGI->generaltexth->allTexts[520], 349, 102, FONT_SMALL, 10, tytulowy, to); //Starting Bonus
	printAtMiddleLoc(CGI->generaltexth->allTexts[521], 222, 538, FONT_SMALL, tytulowy, to); // Player Turn Duration


	if(curOpts->turnTime)
	{
		std::ostringstream os;
		os << (int)curOpts->turnTime << " Minutes";
		printAtMiddleLoc(os.str(), 319,559, FONT_SMALL, zwykly, to);
	}
	else
		printAtMiddleLoc("Unlimited",319,559, FONT_SMALL, zwykly, to);
}

void OptionsTab::nextCastle( int player, int dir )
{
	PlayerSettings &s = curOpts->playerInfos[player];
	si32 &cur = s.castle;
	ui32 allowed = curMap->mapHeader->players[s.color].allowedFactions;

	if (cur == -2) //no castle - no change
		return;

	if (cur == -1) //random => first/last available
	{
		int pom = (dir>0) ? (0) : (F_NUMBER-1); // last or first
		for (;pom >= 0  &&  pom < F_NUMBER;  pom+=dir)
		{
			if((1 << pom) & allowed)
			{
				cur=pom;
				break;
			}
		}
	}
	else // next/previous available
	{
		for (;;)
		{
			cur+=dir;
			if ((1 << cur) & allowed)
				break;

			if (cur >= F_NUMBER  ||  cur<0)
			{
				double p1 = log((double)allowed) / log(2.0f)+0.000001f;
				double check = p1 - ((int)p1);
				if (check < 0.001)
					cur = (int)p1;
				else
					cur = -1;
				break;
			}
		}
	}

	if(s.hero >= 0)
		s.hero = -1;
	if(cur < 0  &&  s.bonus == PlayerSettings::bresource)
		s.bonus = PlayerSettings::brandom;

	entries[player]->selectButtons();
	redraw();
}

void OptionsTab::nextHero( int player, int dir )
{
	PlayerSettings &s = curOpts->playerInfos[player];
	int old = s.hero;
	if (s.castle < 0  ||  !s.human  ||  s.hero == -2)
		return;

	if (s.hero == -1) //random => first/last available
	{
		int max = (s.castle*HEROES_PER_TYPE*2+15),
			min = (s.castle*HEROES_PER_TYPE*2);
		s.hero = nextAllowedHero(min,max,0,dir);
	}
	else
	{
		if(dir > 0)
			s.hero = nextAllowedHero(s.hero,(s.castle*HEROES_PER_TYPE*2+16),1,dir);
		else
			s.hero = nextAllowedHero(s.castle*HEROES_PER_TYPE*2-1,s.hero,1,dir);
	}

	if(old != s.hero)
	{
		usedHeroes.erase(old);
		usedHeroes.insert(s.hero);
		redraw();
	}
}

int OptionsTab::nextAllowedHero( int min, int max, int incl, int dir )
{
	if(dir>0)
	{
		for(int i=min+incl; i<=max-incl; i++)
			if(canUseThisHero(i))
				return i;
	}
	else
	{
		for(int i=max-incl; i>=min+incl; i--)
			if(canUseThisHero(i))
				return i;
	}
	return -1;
}

bool OptionsTab::canUseThisHero( int ID )
{
	//for(int i=0;i<CPG->ret.playerInfos.size();i++)
	//	if(CPG->ret.playerInfos[i].hero==ID) //hero is already taken
	//		return false;

	return !vstd::contains(usedHeroes, ID) && curMap->mapHeader->allowedHeroes[ID];
}

void OptionsTab::nextBonus( int player, int dir )
{
	PlayerSettings &s = curOpts->playerInfos[player];
	si8 &ret = s.bonus += dir;

	if (s.hero==-2 && !curMap->mapHeader->players[s.color].heroesNames.size() && ret==PlayerSettings::bartifact) //no hero - can't be artifact
	{
		if (dir<0)
			ret=PlayerSettings::brandom;
		else ret=PlayerSettings::bgold;
	}

	if(ret > PlayerSettings::bresource)
		ret = PlayerSettings::brandom;
	if(ret < PlayerSettings::brandom)
		ret = PlayerSettings::bresource;

	if (s.castle==-1 && ret==PlayerSettings::bresource) //random castle - can't be resource
	{
		if (dir<0)
			ret=PlayerSettings::bgold;
		else ret=PlayerSettings::brandom;
	}

	redraw();
}

void OptionsTab::changeSelection( const CMapHeader *to )
{
	for(std::map<int, PlayerOptionsEntry*>::iterator it = entries.begin(); it != entries.end(); ++it)
	{
		children -= it->second;
		delete it->second;
	}
	entries.clear();
	usedHeroes.clear();

	OBJ_CONSTRUCTION_CAPTURING_ALL;
	for(std::map<int, PlayerSettings>::iterator it = curOpts->playerInfos.begin(); 
		it != curOpts->playerInfos.end(); ++it)
	{
		entries.insert(std::make_pair(it->first, new PlayerOptionsEntry(this, it->second)));
		const std::vector<SheroName> &heroes = curMap->mapHeader->players[it->first].heroesNames;
		for(size_t hi=0; hi<heroes.size(); hi++)
			usedHeroes.insert(heroes[hi].heroID);
	}
}

void OptionsTab::setTurnLength( int npos )
{
	static const int times[] = {1, 2, 4, 6, 8, 10, 15, 20, 25, 30, 0};
	amin(npos, ARRAY_COUNT(times) - 1);

	curOpts->turnTime = times[npos];
	redraw();
}

void OptionsTab::flagPressed( int player )
{
	static std::pair<int, int> playerToRestore(-1, -1); //<color, player name serial> 

	PlayerSettings &clicked =  curOpts->playerInfos[player];
	PlayerSettings *old = NULL;

	if(playerNames.size() == 1) //single player -> swap
	{
		if(player == playerColor) //that color is already selected, no action needed
			return;


		old = &curOpts->playerInfos[playerColor];
		std::swap(old->human, clicked.human);
		std::swap(old->name, clicked.name);
		playerColor = clicked.color;
	}
	else
	{
		//identify clicked player
		int curNameID = clicked.human 
						? vstd::findPos(playerNames, clicked.name) 
						: -1;

		if(curNameID >= 0  &&  playerToRestore.second == curNameID) //player to restore is about to being replaced -> put him back to the old place
		{
			PlayerSettings &restPos = curOpts->playerInfos[playerToRestore.first];
			setPlayer(restPos, playerToRestore.second);
			playerToRestore.first = playerToRestore.second = 0;
		}

		//who will be put here?
		if(curNameID < 0) //if possible replace computer with unallocated player
		{
			for(int i = 0; i < playerNames.size(); i++)
			{
				if(!curOpts->getPlayersSettings(playerNames[i])) 
				{
					curNameID = i-1; //-1 because it'll incremented soon
					break;
				}
			}
		}

		setPlayer(clicked, ++curNameID); //simply next player

		//if that player was somewhere else, we need to replace him with computer
		if(curNameID < playerNames.size())
		{
			for(std::map<int, PlayerSettings>::iterator i = curOpts->playerInfos.begin(); i != curOpts->playerInfos.end(); i++)
			{
				if(i->first != player  &&  i->second.name == playerNames[curNameID])
				{
					assert(i->second.human);
					playerToRestore.first = i->first;
					playerToRestore.second = vstd::findPos(playerNames, i->second.name);
					setPlayer(i->second, -1); //set computer
					old = &i->second;
					break;
				}
			}
		}
	}

	entries[clicked.color]->selectButtons();
	if(old)
	{
		entries[old->color]->selectButtons();
		if(!entries[playerColor]->fixedHero)
			old->hero = -1;
	}
	GH.totalRedraw();
}

OptionsTab::PlayerOptionsEntry::PlayerOptionsEntry( OptionsTab *owner, PlayerSettings &S)
:s(S)
{
	OBJ_CONSTRUCTION;
	defActions |= SHARE_POS;
	int serial = 0;
	for(int g=0; g < s.color; ++g)
	{
		if( curMap->mapHeader->players[g].canComputerPlay || curMap->mapHeader->players[g].canHumanPlay)
			serial++;
	}

	pos = parent->pos + Point(54, 122 + serial*50);

	static const char *flags[] = {"AOFLGBR.DEF", "AOFLGBB.DEF", "AOFLGBY.DEF", "AOFLGBG.DEF", 
		"AOFLGBO.DEF", "AOFLGBP.DEF", "AOFLGBT.DEF", "AOFLGBS.DEF"};
	static const char *bgs[] = {"ADOPRPNL.bmp", "ADOPBPNL.bmp", "ADOPYPNL.bmp", "ADOPGPNL.bmp", 
		"ADOPOPNL.bmp", "ADOPPPNL.bmp", "ADOPTPNL.bmp", "ADOPSPNL.bmp"};

	bg = new CPicture(BitmapHandler::loadBitmap(bgs[s.color]), 0, 0, true);
	if(owner->type == CMenuScreen::newGame)
	{
		btns[0] = new AdventureMapButton(CGI->generaltexth->zelp[132], bind(&OptionsTab::nextCastle, owner, s.color, -1), 107, 5, "ADOPLFA.DEF");
		btns[1] = new AdventureMapButton(CGI->generaltexth->zelp[133], bind(&OptionsTab::nextCastle, owner, s.color, +1), 168, 5, "ADOPRTA.DEF");
		btns[2] = new AdventureMapButton(CGI->generaltexth->zelp[148], bind(&OptionsTab::nextHero, owner, s.color, -1), 183, 5, "ADOPLFA.DEF");
		btns[3] = new AdventureMapButton(CGI->generaltexth->zelp[149], bind(&OptionsTab::nextHero, owner, s.color, +1), 244, 5, "ADOPRTA.DEF");
		btns[4] = new AdventureMapButton(CGI->generaltexth->zelp[164], bind(&OptionsTab::nextBonus, owner, s.color, -1), 259, 5, "ADOPLFA.DEF");
		btns[5] = new AdventureMapButton(CGI->generaltexth->zelp[165], bind(&OptionsTab::nextBonus, owner, s.color, +1), 320, 5, "ADOPRTA.DEF");
	}
	else
		for(int i = 0; i < 6; i++)
			btns[i] = NULL;

	fixedHero = s.hero != -1; //if we doesn't start with "random hero" it must be fixed or none
	selectButtons(false);

	assert(curMap && curMap->mapHeader);
	const PlayerInfo &p = curMap->mapHeader->players[s.color];
	assert(p.canComputerPlay || p.canHumanPlay); //someone must be able to control this player
	if(p.canHumanPlay && p.canComputerPlay)
		whoCanPlay = HUMAN_OR_CPU;
	else if(p.canComputerPlay)
		whoCanPlay = CPU;
	else
		whoCanPlay = HUMAN;

	if(owner->type != CMenuScreen::scenarioInfo  &&  curMap->mapHeader->players[s.color].canHumanPlay)
	{
		flag = new AdventureMapButton(CGI->generaltexth->zelp[180], bind(&OptionsTab::flagPressed, owner, s.color), -43, 2, flags[s.color]);
		flag->hoverable = true;
	}
	else
		flag = NULL;

	defActions &= ~SHARE_POS;
	town = new SelectedBox(TOWN, s.color);
	town->pos += pos + Point(119, 2);
	hero = new SelectedBox(HERO, s.color);
	hero->pos += pos + Point(195, 2);
	bonus = new SelectedBox(BONUS, s.color);
	bonus->pos += pos + Point(271, 2);
}

void OptionsTab::PlayerOptionsEntry::showAll( SDL_Surface * to )
{
	CIntObject::showAll(to);
	printAtMiddleLoc(s.name, 55, 10, FONT_SMALL, zwykly, to);
	printAtMiddleWBLoc(CGI->generaltexth->arraytxt[206+whoCanPlay], 28, 34, FONT_TINY, 6, zwykly, to);
}

void OptionsTab::PlayerOptionsEntry::selectButtons(bool onlyHero)
{
	if(!btns[0])
		return;

	if(!onlyHero  &&  s.castle != -1)
	{
		btns[0]->disable();
		btns[1]->disable();
	}
	if(fixedHero  ||  !s.human  ||  s.castle < 0)
	{
		btns[2]->disable();
		btns[3]->disable();
	}
	else
	{
		btns[2]->enable(active);
		btns[3]->enable(active);
	}
}

void OptionsTab::SelectedBox::showAll( SDL_Surface * to )
{
	//PlayerSettings &s = curOpts->playerInfos[player];
	SDL_Surface *toBlit = getImg();
	const std::string *toPrint = getText();
	blitAt(toBlit, pos, to);
	printAtMiddleLoc(*toPrint, 23, 39, FONT_TINY, zwykly, to);
}

OptionsTab::SelectedBox::SelectedBox( SelType Which, ui8 Player )
:which(Which), player(Player)
{
	SDL_Surface *img = getImg();
	pos.w = img->w;
	pos.h = img->h;
	used = RCLICK;
}

SDL_Surface * OptionsTab::SelectedBox::getImg() const
{
	const PlayerSettings &s = curOpts->playerInfos[player];
	switch(which)
	{
	case TOWN:
		if (s.castle < F_NUMBER  &&  s.castle >= 0)
			return graphics->getPic(s.castle, true, false);
		else if (s.castle == -1)
			return CGP->rTown;
		else if (s.castle == -2)
			return CGP->nTown;
	case HERO:
		if (s.hero == -1)
		{
			return CGP->rHero;
		}
		else if (s.hero == -2)
		{
			if(s.heroPortrait >= 0)
				return graphics->portraitSmall[s.heroPortrait];
			else
				return CGP->nHero;
		}
		else
		{
			return graphics->portraitSmall[s.hero];
		}
		break;
	case BONUS:
		{
			int pom;
			switch (s.bonus)
			{
			case -1:
				pom=10;
				break;
			case 0:
				pom=9;
				break;
			case 1:
				pom=8;
				break;
			case 2:
				pom=CGI->townh->towns[s.castle].bonus;
				break;
			default: 
				assert(0);
			}
			return CGP->bonuses->ourImages[pom].bitmap;
		}
	default:
		return NULL;
	}
}

const std::string * OptionsTab::SelectedBox::getText() const
{
	const PlayerSettings &s = curOpts->playerInfos[player];	
	switch(which)
	{
	case TOWN:
		if (s.castle < F_NUMBER  &&  s.castle >= 0)
			return &CGI->townh->towns[s.castle].Name();
		else if (s.castle == -1)
			return &CGI->generaltexth->allTexts[522];
		else if (s.castle == -2)
			return &CGI->generaltexth->allTexts[523];
	case HERO:
		if (s.hero == -1)
			return &CGI->generaltexth->allTexts[522];
		else if (s.hero == -2)
		{
			if(s.heroPortrait >= 0)
			{
				if(s.heroName.length())
					return &s.heroName;
				else
					return &CGI->heroh->heroes[s.heroPortrait]->name;
			}
			else
				return &CGI->generaltexth->allTexts[523];
		}
		else
		{
			//if(s.heroName.length())
			//	return &s.heroName;
			//else
				return &CGI->heroh->heroes[s.hero]->name;
		}
	case BONUS:
		switch (s.bonus)
		{
		case -1:
			return &CGI->generaltexth->allTexts[522];
		default:
			return &CGI->generaltexth->arraytxt[214 + s.bonus];
		}
	default:
		return NULL;
	}
}

void OptionsTab::SelectedBox::clickRight( tribool down, bool previousState )
{
	if(indeterminate(down) || !down) return;
	const PlayerSettings &s = curOpts->playerInfos[player];
	SDL_Surface *bmp = NULL;
	const std::string *title = NULL, *subTitle = NULL;

	subTitle = getText();

	int val;
	switch(which)
	{
	case TOWN: 
		val = s.castle; 
		break;
	case HERO: 
		val = s.hero; 
		if(val == -2) //none => we may have some preset info
		{
			int p9 = curMap->mapHeader->players[s.color].p9;
			if(p9 != 255  &&  curOpts->playerInfos[player].heroPortrait >= 0)
				val = p9;
		}
		break;
	case BONUS: 
		val = s.bonus; 
		break;
	}

	if(val == -1  ||  which == BONUS) //random or bonus box
	{
		bmp = CMessage::drawBox1(256, 190);
		std::string *description = NULL;

		switch(which)
		{
		case TOWN:
			title = &CGI->generaltexth->allTexts[103];
			description = &CGI->generaltexth->allTexts[104];
			break;
		case HERO:
			title = &CGI->generaltexth->allTexts[101];
			description = &CGI->generaltexth->allTexts[102];
			break;
		case BONUS:
			{
				switch(val)
				{
				case PlayerSettings::brandom:
					title = &CGI->generaltexth->allTexts[86]; //{Random Bonus}
					description = &CGI->generaltexth->allTexts[94]; //Gold, wood and ore, or an artifact is randomly chosen as your starting bonus
					break;
				case PlayerSettings::bartifact:
					title = &CGI->generaltexth->allTexts[83]; //{Artifact Bonus}
					description = &CGI->generaltexth->allTexts[90]; //An artifact is randomly chosen and equipped to your starting hero
					break;
				case PlayerSettings::bgold:
					title = &CGI->generaltexth->allTexts[84]; //{Gold Bonus}
					subTitle = &CGI->generaltexth->allTexts[87]; //500-1000
					description = &CGI->generaltexth->allTexts[92]; //At the start of the game, 500-1000 gold is added to your Kingdom's resource pool
					break;
				case PlayerSettings::bresource:
					{
						title = &CGI->generaltexth->allTexts[85]; //{Resource Bonus}
						switch(CGI->townh->towns[s.castle].primaryRes)
						{
						case 1:
							subTitle = &CGI->generaltexth->allTexts[694];
							description = &CGI->generaltexth->allTexts[690];
							break;
						case 3:
							subTitle = &CGI->generaltexth->allTexts[695];
							description = &CGI->generaltexth->allTexts[691];
							break;
						case 4:
							subTitle = &CGI->generaltexth->allTexts[692];
							description = &CGI->generaltexth->allTexts[688];
							break;
						case 5:
							subTitle = &CGI->generaltexth->allTexts[693];
							description = &CGI->generaltexth->allTexts[689];
							break;
						case 127:
							subTitle = &CGI->generaltexth->allTexts[89]; //5-10 wood / 5-10 ore
							description = &CGI->generaltexth->allTexts[93]; //At the start of the game, 5-10 wood and 5-10 ore are added to your Kingdom's resource pool
							break;
						}
					}
					break;
				}
			}
			break;
		}

		if(description)
			CSDL_Ext::printAtMiddleWB(*description, 125, 145, FONT_SMALL, 37, zwykly, bmp);
	}
	else if(val == -2)
	{
		return;
	}
	else if(which == TOWN)
	{
		bmp = CMessage::drawBox1(256, 319);
		title = &CGI->generaltexth->allTexts[80];

		CSDL_Ext::printAtMiddle(CGI->generaltexth->allTexts[79], 135, 137, FONT_MEDIUM, tytulowy, bmp);
		
		const CTown &t = CGI->townh->towns[val];
		//print creatures
		int x = 60, y = 159;
		for(int i = 0; i < 7; i++)
		{
			int c = t.basicCreatures[i];
			blitAt(graphics->smallImgs[c], x, y, bmp);
			CSDL_Ext::printAtMiddleWB(CGI->creh->creatures[c]->nameSing, x + 16, y + 45, FONT_TINY, 10, zwykly, bmp);

			if(i == 2)
			{
				x = 40;
				y += 76;
			}
			else
			{
				x += 52;
			}
		}

	}
	else if(val >= 0)
	{
		const CHero *h = CGI->heroh->heroes[val];
		bmp = CMessage::drawBox1(320, 255);
		title = &CGI->generaltexth->allTexts[77];

		CSDL_Ext::printAtMiddle(*title, 167, 36, FONT_MEDIUM, tytulowy, bmp);
		CSDL_Ext::printAtMiddle(*subTitle + " - " + h->heroClass->name, 160, 99, FONT_SMALL, zwykly, bmp);

		blitAt(getImg(), 136, 56, bmp);

		//print specialty
		CSDL_Ext::printAtMiddle(CGI->generaltexth->allTexts[78], 166, 132, FONT_MEDIUM, tytulowy, bmp);
		blitAt(graphics->un44->ourImages[val].bitmap, 140, 150, bmp);
		CSDL_Ext::printAtMiddle(CGI->generaltexth->hTxts[val].bonusName, 166, 203, FONT_SMALL, zwykly, bmp);

		GH.pushInt(new CInfoPopup(bmp, true));
		return;
	}

	if(title)
		CSDL_Ext::printAtMiddle(*title, 135, 36, FONT_MEDIUM, tytulowy, bmp);
	if(subTitle)
		CSDL_Ext::printAtMiddle(*subTitle, 127, 103, FONT_SMALL, zwykly, bmp);

	blitAt(getImg(), 104, 60, bmp);

	GH.pushInt(new CInfoPopup(bmp, true));
}

CScenarioInfo::CScenarioInfo(const CMapHeader *mapHeader, const StartInfo *startInfo)
{
	OBJ_CONSTRUCTION_CAPTURING_ALL;

	for(std::map<int, PlayerSettings>::const_iterator it = startInfo->playerInfos.begin(); 
		it != startInfo->playerInfos.end(); ++it)
	{
		if(it->second.human)
		{
			playerColor = it->first;
		}
	}

	pos.w = 762;
	pos.h = 584;
	center(pos);

	assert(LOCPLINT);
	assert(!curOpts);
	curOpts = new StartInfo(*LOCPLINT->cb->getStartInfo()); //deleted by clearInfo
	assert(!curMap);
	curMap = mapInfoFromGame(); //deleted by clearInfo
	setPlayersFromGame();

	card = new InfoCard(CMenuScreen::scenarioInfo);
	opt = new OptionsTab(CMenuScreen::scenarioInfo);
	opt->changeSelection(0);

	card->difficulty->select(startInfo->difficulty, 0);
	back = new AdventureMapButton("", CGI->generaltexth->zelp[105].second, bind(&CGuiHandler::popIntTotally, &GH, this), 584, 535, "SCNRBACK.DEF", SDLK_ESCAPE);
}

CScenarioInfo::~CScenarioInfo()
{
	clearInfo();
}

bool mapSorter::operator()(const CMapInfo *aaa, const CMapInfo *bbb)
{
	const CMapHeader * a = aaa->mapHeader,
		* b = bbb->mapHeader;
	if(a && b) //if we are sorting scenarios
	{
		switch (sortBy)
		{
		case _format: //by map format (RoE, WoG, etc)
			return (a->version<b->version);
			break;
		case _loscon: //by loss conditions
			return (a->lossCondition.typeOfLossCon<b->lossCondition.typeOfLossCon);
			break;
		case _playerAm: //by player amount
			int playerAmntB,humenPlayersB,playerAmntA,humenPlayersA;
			playerAmntB=humenPlayersB=playerAmntA=humenPlayersA=0;
			for (int i=0;i<8;i++)
			{
				if (a->players[i].canHumanPlay) {playerAmntA++;humenPlayersA++;}
				else if (a->players[i].canComputerPlay) {playerAmntA++;}
				if (b->players[i].canHumanPlay) {playerAmntB++;humenPlayersB++;}
				else if (b->players[i].canComputerPlay) {playerAmntB++;}
			}
			if (playerAmntB!=playerAmntA)
				return (playerAmntA<playerAmntB);
			else
				return (humenPlayersA<humenPlayersB);
			break;
		case _size: //by size of map
			return (a->width<b->width);
			break;
		case _viccon: //by victory conditions
			return (a->victoryCondition.condition < b->victoryCondition.condition);
			break;
		case _name: //by name
			return (a->name<b->name);
			break;
		default:
			return (a->name<b->name);
			break;
		}
	}
	else //if we are sorting campaigns
	{
		switch(sortBy)
		{
			case _numOfMaps: //by number of maps in campaign
				return CGI->generaltexth->campaignRegionNames[ aaa->campaignHeader->mapVersion ].size() < 
					CGI->generaltexth->campaignRegionNames[ bbb->campaignHeader->mapVersion ].size();
				break;
			case _name: //by name
				return aaa->campaignHeader->name < bbb->campaignHeader->name;
				break;
			default:
				return aaa->campaignHeader->name < bbb->campaignHeader->name;
				break;
		}
	}
}

CMultiMode::CMultiMode()
{
	OBJ_CONSTRUCTION;
	bg = new CPicture("MUPOPUP.bmp");
	bg->convertToScreenBPP(); //so we could draw without problems
	blitAt(CPicture("MUMAP.bmp"), 16, 77, *bg); //blit img
	pos = bg->center(); //center, window has size of bg graphic

	bar = new CGStatusBar(new CPicture(Rect(7, 465, 440, 18), 0));//226, 472
	txt = new CTextInput(Rect(19, 436, 334, 16), *bg);
	txt->setText(CGI->generaltexth->allTexts[434]); //Player

	btns[0] = new AdventureMapButton(CGI->generaltexth->zelp[266], bind(&CMultiMode::openHotseat, this), 373, 78, "MUBHOT.DEF");
	btns[6] = new AdventureMapButton(CGI->generaltexth->zelp[288], bind(&CGuiHandler::popIntTotally, ref(GH), this), 373, 424, "MUBCANC.DEF", SDLK_ESCAPE);
}

void CMultiMode::openHotseat()
{
	GH.pushInt(new CHotSeatPlayers(txt->text));
}

CHotSeatPlayers::CHotSeatPlayers(const std::string &firstPlayer)
{
	OBJ_CONSTRUCTION;
	bg = new CPicture("MUHOTSEA.bmp");
	bg->convertToScreenBPP(); //so we could draw without problems
	bg->printAtMiddleWBLoc(CGI->generaltexth->allTexts[446], 185, 55, FONT_BIG, 50, zwykly, *bg); //HOTSEAT	Please enter names
	pos = bg->center(); //center, window has size of bg graphic

	for(int i = 0; i < ARRAY_COUNT(txt); i++)
		txt[i] = new CTextInput(Rect(60, 85 + i*30, 280, 16), *bg);

	txt[0]->setText(firstPlayer);
	txt[0]->giveFocus();

	ok = new AdventureMapButton(CGI->generaltexth->zelp[560], bind(&CHotSeatPlayers::enterSelectionScreen, this), 95, 338, "MUBCHCK.DEF", SDLK_RETURN);
	cancel = new AdventureMapButton(CGI->generaltexth->zelp[561], bind(&CGuiHandler::popIntTotally, ref(GH), this), 205, 338, "MUBCANC.DEF", SDLK_ESCAPE);
	bar = new CGStatusBar(new CPicture(Rect(7, 381, 348, 18), 0));//226, 472
}

void CHotSeatPlayers::enterSelectionScreen()
{
	playerNames.clear();
	for(int i = 0; i < ARRAY_COUNT(txt); i++)
		if(txt[i]->text.length())
			playerNames.push_back(txt[i]->text);

	GH.popInts(2);
	GH.pushInt(new CSelectionScreen(CMenuScreen::newGame));
}

CBonusSelection::CBonusSelection( CCampaignState * _ourCampaign )
: ourCampaign(_ourCampaign), highlightedRegion(NULL), ourHeader(NULL), bonuses(NULL),
	diffLb(NULL), diffRb(NULL)
{
	OBJ_CONSTRUCTION_CAPTURING_ALL;
	static const std::string bgNames [] = {"E1_BG.BMP", "G2_BG.BMP", "E2_BG.BMP", "G1_BG.BMP", "G3_BG.BMP", "N1_BG.BMP",
		"S1_BG.BMP", "BR_BG.BMP", "IS_BG.BMP", "KR_BG.BMP", "NI_BG.BMP", "TA_BG.BMP", "AR_BG.BMP", "HS_BG.BMP",
		"BB_BG.BMP", "NB_BG.BMP", "EL_BG.BMP", "RN_BG.BMP", "UA_BG.BMP", "SP_BG.BMP"};

	if(ourCampaign->mapsConquered.size())
	{
		CGP->resetPlayerNames();
	}

	loadPositionsOfGraphics();

	background = BitmapHandler::loadBitmap(bgNames[ourCampaign->camp->header.mapVersion]);

	SDL_Surface * panel = BitmapHandler::loadBitmap("CAMPBRF.BMP");

	blitAt(panel, 456, 6, background);

	startB = new AdventureMapButton("", "", bind(&CBonusSelection::startMap, this), 475, 536, "CBBEGIB.DEF", SDLK_RETURN);
	backB = new AdventureMapButton("", "", bind(&CBonusSelection::goBack, this), 624, 536, "CBCANCB.DEF", SDLK_ESCAPE);

	//campaign name
	if (ourCampaign->camp->header.name.length())
		printAtLoc(ourCampaign->camp->header.name, 481, 28, FONT_BIG, tytulowy, background);
	else 
		printAtLoc("Unnamed", 481, 28, FONT_BIG, tytulowy, background);

	//map size icon
	sizes = CDefHandler::giveDef("SCNRMPSZ.DEF");


	//campaign description
	printAtLoc(CGI->generaltexth->allTexts[38], 481, 63, FONT_SMALL, tytulowy, background);
 
	cmpgDesc = new CTextBox(ourCampaign->camp->header.description, Rect(480, 86, 286, 117), 1);
	cmpgDesc->showAll(background);

	//map description
	mapDesc = new CTextBox("", Rect(480, 280, 286, 117), 1);

	//bonus choosing
	printAtLoc(CGI->generaltexth->allTexts[71], 510, 431, FONT_MEDIUM, zwykly, background); //Choose a bonus:
	bonuses = new CHighlightableButtonsGroup(bind(&CBonusSelection::selectBonus, this, _1));

	//set left part of window
	for (int g=0; g<ourCampaign->camp->scenarios.size(); ++g)
	{
		if(ourCampaign->camp->conquerable(g))
		{
			regions.push_back(new CRegion(this, true, true, g));
			regions[regions.size()-1]->rclickText = ourCampaign->camp->scenarios[g].regionText;
			if (highlightedRegion == NULL)
			{
				highlightedRegion = regions.back();
				selectMap(g);
			}
		}
		else if (ourCampaign->camp->scenarios[g].conquered) //display as striped
		{
			regions.push_back(new CRegion(this, false, false, g));
			regions[regions.size()-1]->rclickText = ourCampaign->camp->scenarios[g].regionText;
		}
	}

	//init campaign state if necessary
	if (ourCampaign->campaignName.size() == 0)
	{
		ourCampaign->initNewCampaign(sInfo);
	}

	//allies / enemies
	printAtLoc(CGI->generaltexth->allTexts[390] + ":", 486, 407, FONT_SMALL, zwykly, background); //Allies
	printAtLoc(CGI->generaltexth->allTexts[391] + ":", 619, 407, FONT_SMALL, zwykly, background); //Enemies

	SDL_FreeSurface(panel);

	//difficulty
	printAtLoc("Difficulty", 691, 431, FONT_MEDIUM, zwykly, background); //Difficulty
	{//difficulty pics
		for (int b=0; b<ARRAY_COUNT(diffPics); ++b)
		{
			CDefEssential * cde = CDefHandler::giveDefEss("GSPBUT" + boost::lexical_cast<std::string>(b+3) + ".DEF");
			SDL_Surface * surfToDuplicate = cde->ourImages[0].bitmap;
			diffPics[b] = SDL_ConvertSurface(surfToDuplicate, surfToDuplicate->format,
				surfToDuplicate->flags);

			delete cde;
		}
	}
	//difficulty selection buttons
	if (ourCampaign->camp->header.difficultyChoosenByPlayer)
	{
		diffLb = new AdventureMapButton("", "", bind(&CBonusSelection::changeDiff, this, false), 694, 508, "SCNRBLF.DEF");
		diffRb = new AdventureMapButton("", "", bind(&CBonusSelection::changeDiff, this, true), 738, 508, "SCNRBRT.DEF");
	}

	//load miniflags
	sFlags = CDefHandler::giveDef("ITGFLAGS.DEF");
	
}

CBonusSelection::~CBonusSelection()
{
	SDL_FreeSurface(background);
	delete sizes;
	delete ourHeader;
	delete sFlags;
	for (int b=0; b<ARRAY_COUNT(diffPics); ++b)
	{
		SDL_FreeSurface(diffPics[b]);
	}
}

void CBonusSelection::goBack()
{
	GH.popIntTotally(this);
}

void CBonusSelection::showAll( SDL_Surface * to )
{
	CIntObject::showAll(to);
	blitAt(background, pos.x, pos.y, to);

	show(to);
}

void CBonusSelection::loadPositionsOfGraphics()
{
	std::ifstream is(DATA_DIR "/config/campaign_regions.txt", std::ios_base::binary | std::ios_base::in);

	assert(is.is_open());

	for (int g=0; g<CGI->generaltexth->campaignMapNames.size(); ++g)
	{
		SCampPositions sc;
		is >> sc.campPrefix;
		is >> sc.colorSuffixLength;
		bool contReading = true;
		while(contReading)
		{
			SCampPositions::SRegionDesc rd;
			is >> rd.infix;
			if(rd.infix == "END")
			{
				contReading = false;
			}
			else
			{
				is >> rd.xpos >> rd.ypos;
				sc.regions.push_back(rd);
			}
		}

		campDescriptions.push_back(sc);
	}

}

void CBonusSelection::selectMap( int whichOne )
{
	sInfo.difficulty = ourCampaign->camp->scenarios[whichOne].difficulty;
	sInfo.mapname = ourCampaign->camp->header.filename;
	sInfo.mode = StartInfo::CAMPAIGN;

	//get header
	int i = 0;
	delete ourHeader;
	ourHeader = new CMapHeader();
	ourHeader->initFromMemory((const unsigned char*)ourCampaign->camp->mapPieces.find(whichOne)->second.c_str(), i);
// 	CMapInfo *mapInfo = const_cast<CMapInfo *>(curMap);
// 	mapInfo->mapHeader = ourHeader;
// 	mapInfo->countPlayers();
// 	mapInfo->mapHeader = NULL;
	CMapInfo dummyInfo(false);
	dummyInfo.filename = "lala";

	
	CSelectionScreen::updateStartInfo(curMap ? curMap : &dummyInfo, sInfo, ourHeader);
	sInfo.turnTime = 0;
	sInfo.whichMapInCampaign = whichOne;
	sInfo.difficulty = ourCampaign->camp->scenarios[whichOne].difficulty;

	ourCampaign->currentMap = whichOne;

	mapDesc->setTxt(ourHeader->description);

	updateBonusSelection();
}

void CBonusSelection::show( SDL_Surface * to )
{
	blitAt(background, pos.x, pos.y, to);

	//map name
	std::string mapName = ourHeader->name;

	if (mapName.length())
		printAtLoc(mapName, 481, 219, FONT_BIG, tytulowy, to);
	else 
		printAtLoc("Unnamed", 481, 219, FONT_BIG, tytulowy, to);

	//map description
	printAtLoc(CGI->generaltexth->allTexts[496], 481, 253, FONT_SMALL, tytulowy, to);

	mapDesc->showAll(to); //showAll because CTextBox has no show()

	//map size icon
	int temp;
	switch (ourHeader->width)
	{
	case 36:
		temp=0;
		break;
	case 72:
		temp=1;
		break;
	case 108:
		temp=2;
		break;
	case 144:
		temp=3;
		break;
	default:
		temp=4;
		break;
	}
	blitAtLoc(sizes->ourImages[temp].bitmap, 735, 26, to);

	//flags
	int fx=530, ex=674, myT;
	myT = ourHeader->players[playerColor].team;
	for (std::map<int, PlayerSettings>::const_iterator i = sInfo.playerInfos.begin(); i != sInfo.playerInfos.end(); i++)
	{
		int *myx = ((i->first == playerColor  ||  ourHeader->players[i->first].team == myT) ? &fx : &ex);
		blitAtLoc(sFlags->ourImages[i->first].bitmap, *myx, 405, to);
		*myx += sFlags->ourImages[i->first].bitmap->w;
	}

	//difficulty
	blitAt(diffPics[sInfo.difficulty], 709, 455, to);

	CIntObject::show(to);
}

void CBonusSelection::updateBonusSelection()
{
	//graphics:
	//spell - SPELLBON.DEF
	//monster - TWCRPORT.DEF
	//building - BO*.BMP graphics
	//artifact - ARTIFBON.DEF
	//spell scroll - SPELLBON.DEF
	//prim skill - PSKILBON.DEF
	//sec skill - SSKILBON.DEF
	//resource - BORES.DEF
	//player - ?
	//hero -?
	const CCampaignScenario &scenario = ourCampaign->camp->scenarios[sInfo.whichMapInCampaign];
	const std::vector<CScenarioTravel::STravelBonus> & bonDescs = scenario.travelOptions.bonusesToChoose;

	CDefEssential * twcp = CDefHandler::giveDefEss("TWCRPORT.DEF"); //for yellow border

	bonuses->buttons.clear();
	{
		BLOCK_CAPTURING;
		static const char *bonDefs[] = {"SPELLBON.DEF", "TWCRPORT.DEF", "GSPBUT5.DEF", "ARTIFBON.DEF", "SPELLBON.DEF",
			"PSKILBON.DEF", "SSKILBON.DEF", "BORES.DEF", "GSPBUT5.DEF", "GSPBUT5.DEF"};

		for(int i = 0; i < bonDescs.size(); i++)
		{
			SDL_Surface *notSelected, *selected;

			CDefEssential * de = CDefHandler::giveDefEss(bonDefs[bonDescs[i].type]);
			SDL_Surface * surfToDuplicate = NULL;
			bool freeDuplicatedSurface = false;

			std::string desc;
			switch(bonDescs[i].type)
			{
			case 0: //spell
				surfToDuplicate = de->ourImages[bonDescs[i].info2].bitmap;
				desc = CGI->generaltexth->allTexts[715];
				boost::algorithm::replace_first(desc, "%s", CGI->spellh->spells[bonDescs[i].info2].name);
				break;
			case 1: //monster
				surfToDuplicate = de->ourImages[bonDescs[i].info2 + 2].bitmap;
				desc = CGI->generaltexth->allTexts[717];
				boost::algorithm::replace_first(desc, "%d", boost::lexical_cast<std::string>(bonDescs[i].info3));
				boost::algorithm::replace_first(desc, "%s", CGI->creh->creatures[bonDescs[i].info2]->namePl);
				break;
			case 2: //building
				{
					int faction = -1;
					for(std::map<int, PlayerSettings>::iterator it = sInfo.playerInfos.begin(); 
						it != sInfo.playerInfos.end(); ++it)
					{
						if (it->second.human)
						{
							faction = it->second.castle;
							break;
						}
						
					}
					assert(faction != -1);

					std::string bldgBitmapName = CGI->buildh->ERMUtoPicture[faction][CBuildingHandler::campToERMU(bonDescs[i].info1, faction, std::set<si32>())];
					surfToDuplicate = BitmapHandler::loadBitmap(bldgBitmapName);

					freeDuplicatedSurface = true;
				}
				break;
			case 3: //artifact
				surfToDuplicate = de->ourImages[bonDescs[i].info2].bitmap;
				desc = CGI->generaltexth->allTexts[715];
				boost::algorithm::replace_first(desc, "%s", CGI->arth->artifacts[bonDescs[i].info2]->Name());
				break;
			case 4: //spell scroll
				surfToDuplicate = de->ourImages[bonDescs[i].info2].bitmap;
				desc = CGI->generaltexth->allTexts[716];
				boost::algorithm::replace_first(desc, "%s", CGI->spellh->spells[bonDescs[i].info2].name);
				break;
			case 5: //primary skill
				{
					int leadingSkill = -1;
					std::vector<std::pair<int, int> > toPrint; //primary skills to be listed <num, val>
					const ui8* ptr = reinterpret_cast<const ui8*>(&bonDescs[i].info2);
					for (int g=0; g<PRIMARY_SKILLS; ++g)
					{
						if (leadingSkill == -1 || ptr[g] > ptr[leadingSkill])
						{
							leadingSkill = g;
						}
						if (ptr[g] != 0)
						{
							toPrint.push_back(std::make_pair(g, ptr[g]));
						}
					}
					surfToDuplicate = de->ourImages[leadingSkill].bitmap;
					desc = CGI->generaltexth->allTexts[715];

					std::string substitute; //text to be printed instead of %s
					for (int v=0; v<toPrint.size(); ++v)
					{
						substitute += boost::lexical_cast<std::string>(toPrint[v].second);
						substitute += " " + CGI->generaltexth->primarySkillNames[toPrint[v].first];
						if(v != toPrint.size() - 1)
						{
							substitute += ", ";
						}
					}

					boost::algorithm::replace_first(desc, "%s", substitute);
					break;
				}
			case 6: //secondary skill
				surfToDuplicate = de->ourImages[bonDescs[i].info2].bitmap;
				desc = CGI->generaltexth->allTexts[718];

				boost::algorithm::replace_first(desc, "%s", CGI->generaltexth->levels[bonDescs[i].info3]); //skill level
				boost::algorithm::replace_first(desc, "%s", CGI->generaltexth->skillName[bonDescs[i].info2]); //skill name

				break;
			case 7: //resource
				{
					int serialResID = 0;
					switch(bonDescs[i].info1)
					{
					case 0: case 1: case 2: case 3: case 4: case 5: case 6:
						serialResID = bonDescs[i].info1;
						break;
					case 0xFD: //wood + ore
						serialResID = 7;
						break;
					case 0xFE: //rare resources
						serialResID = 8;
						break;
					}
					surfToDuplicate = de->ourImages[serialResID].bitmap;

					desc = CGI->generaltexth->allTexts[717];
					boost::algorithm::replace_first(desc, "%d", boost::lexical_cast<std::string>(bonDescs[i].info2));
					std::string replacement;
					if (serialResID <= 6)
					{
						replacement = CGI->generaltexth->restypes[serialResID];
					}
					else
					{
						replacement = CGI->generaltexth->allTexts[714 + serialResID];
					}
					boost::algorithm::replace_first(desc, "%s", replacement);
				}
				break;
			case 8: //player
				//TODO
				continue;
				break;
			case 9: //hero
				//TODO
				continue;
				break;
			}

			bonuses->addButton(new CHighlightableButton(desc, desc, 0, 475 + i*68, 455, bonDefs[bonDescs[i].type], i));

			notSelected = SDL_ConvertSurface(surfToDuplicate, surfToDuplicate->format, surfToDuplicate->flags);
			selected = SDL_ConvertSurface(notSelected, notSelected->format, notSelected->flags);

			//printing yellow border
			blitAt(twcp->ourImages[1].bitmap, 0, 0, selected);

			//moving surfaces into button
			bonuses->buttons.back()->imgs[0].clear();
			bonuses->buttons.back()->imgs[0].push_back(notSelected);
			bonuses->buttons.back()->imgs[0].push_back(selected);

			//cleaning
			delete de;
			if(freeDuplicatedSurface)
			{
				SDL_FreeSurface(surfToDuplicate);
			}
		}
	}
	if (bonuses->buttons.size() > 0)
	{
		bonuses->select(0, 0);
	}

	delete twcp;

}

void CBonusSelection::startMap()
{
	StartInfo *si = new StartInfo(sInfo);
	//don't pop - we should get back to this screen
	GH.popInts(3);
	curOpts = NULL;
	::startGame(si);
}

void CBonusSelection::selectBonus( int id )
{
	sInfo.choosenCampaignBonus = id;
}

void CBonusSelection::changeDiff( bool increase )
{
	if (increase)
	{
		sInfo.difficulty = std::min(sInfo.difficulty + 1, 4);
	}
	else
	{
		sInfo.difficulty = std::max(sInfo.difficulty - 1, 0);
	}
}

CBonusSelection::CRegion::CRegion( CBonusSelection * _owner, bool _accessible, bool _selectable, int _myNumber )
: owner(_owner), accessible(_accessible), selectable(_selectable), myNumber(_myNumber)
{
	OBJ_CONSTRUCTION;
	used = LCLICK | RCLICK;

	static const std::string colors[2][8] = {
		{"R", "B", "N", "G", "O", "V", "T", "P"},
		{"Re", "Bl", "Br", "Gr", "Or", "Vi", "Te", "Pi"}};

	const SCampPositions & campDsc = owner->campDescriptions[owner->ourCampaign->camp->header.mapVersion];
	const SCampPositions::SRegionDesc & desc = campDsc.regions[myNumber];
	pos.x = desc.xpos;
	pos.y = desc.ypos;

	//loading of graphics

	std::string prefix = campDsc.campPrefix + desc.infix + "_";
	std::string suffix = colors[campDsc.colorSuffixLength - 1][owner->ourCampaign->camp->scenarios[myNumber].regionColor];

	static const std::string infix [] = {"En", "Se", "Co"};
	for (int g = 0; g < ARRAY_COUNT(infix); g++)
	{
		graphics[g] = BitmapHandler::loadBitmap(prefix + infix[g] + suffix + ".BMP");
	}
	pos.w = graphics[0]->w;
	pos.h = graphics[0]->h;

}

CBonusSelection::CRegion::~CRegion()
{
	for (int g=0; g<ARRAY_COUNT(graphics); ++g)
	{
		SDL_FreeSurface(graphics[g]);
	}
}

void CBonusSelection::CRegion::clickLeft( tribool down, bool previousState )
{
	//select if selectable & clicked inside our graphic
	if ( indeterminate(down) )
	{
		return;
	}
	if( !down && selectable && !CSDL_Ext::isTransparent(graphics[0], GH.current->motion.x-pos.x, GH.current->motion.y-pos.y) )
	{
		owner->selectMap(myNumber);
		owner->highlightedRegion = this;
	}
}

void CBonusSelection::CRegion::clickRight( tribool down, bool previousState )
{
	//show r-click text
	if( down && !CSDL_Ext::isTransparent(graphics[0], GH.current->motion.x-pos.x, GH.current->motion.y-pos.y) &&
		rclickText.size() )
	{
		CRClickPopup::createAndPush(rclickText);
	}
}

void CBonusSelection::CRegion::show( SDL_Surface * to )
{
	const SCampPositions::SRegionDesc & desc = owner->campDescriptions[owner->ourCampaign->camp->header.mapVersion].regions[myNumber];
	if (!accessible)
	{
		//show as striped
		blitAt(graphics[2], pos.x, pos.y, to);
	}
	else if (this == owner->highlightedRegion)
	{
		//show as selected
		blitAt(graphics[1], pos.x, pos.y, to);
	}
	else
	{
		//show as not selected selected
		blitAt(graphics[0], pos.x, pos.y, to);
	}
}

CSavingScreen::CSavingScreen(bool hotseat)
 : CSelectionScreen(CMenuScreen::saveGame, hotseat)
{
	ourGame = mapInfoFromGame();
	sInfo = *LOCPLINT->cb->getStartInfo();
	setPlayersFromGame();
}

CSavingScreen::~CSavingScreen()
{

}

