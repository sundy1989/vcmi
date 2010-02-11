#ifndef __GLOBAL_H__
#define __GLOBAL_H__
#include <iostream>
#include <algorithm> //std::find
#include <boost/logic/tribool.hpp>
using boost::logic::tribool;
#include <boost/cstdint.hpp>
typedef boost::uint64_t ui64; //unsigned int 64 bits (8 bytes)
typedef boost::uint32_t ui32;  //unsigned int 32 bits (4 bytes)
typedef boost::uint16_t ui16; //unsigned int 16 bits (2 bytes)
typedef boost::uint8_t ui8; //unsigned int 8 bits (1 byte)
typedef boost::int64_t si64; //signed int 64 bits (8 bytes)
typedef boost::int32_t si32; //signed int 32 bits (4 bytes)
typedef boost::int16_t si16; //signed int 16 bits (2 bytes)
typedef boost::int8_t si8; //signed int 8 bits (1 byte)
#include "int3.h"
#define CHECKTIME 1
#if CHECKTIME
#include "timeHandler.h"
#define THC
#endif

#define NAME_VER ("VCMI 0.75b")
extern std::string NAME; //full name
extern std::string NAME_AFFIX; //client / server
#define CONSOLE_LOGGING_LEVEL 5
#define FILE_LOGGING_LEVEL 6

/* 
 * DATA_DIR contains the game data (Data/, MP3/, ...).
 * USER_DIR is where to save games (Games/) and the config.
 * BIN_DIR is where the vcmiclient/vcmiserver binaries reside
 * LIB_DIR is where the AI libraries reside (linux only)
 */
#ifdef _WIN32
#define DATA_DIR "."
#define USER_DIR  "."
#define BIN_DIR  "."
#define SERVER_NAME "VCMI_server.exe"
#else
#ifndef DATA_DIR
#error DATA_DIR undefined.
#endif
#ifndef BIN_DIR
#error BIN_DIR undefined.
#endif
#ifndef LIB_DIR
#error LIB_DIR undefined.
#endif
#define SERVER_NAME "vcmiserver"
#endif

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

/*
 * global.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

enum Ecolor {RED, BLUE, TAN, GREEN, ORANGE, PURPLE, TEAL, PINK}; //player's colors
enum EvictoryConditions {artifact, gatherTroop, gatherResource, buildCity, buildGrail, beatHero,
	captureCity, beatMonster, takeDwellings, takeMines, transportItem, winStandard=255};
enum ElossCon {lossCastle, lossHero, timeExpires, lossStandard=255};
enum ECombatInfo{ALIVE = 180, SUMMONED, CLONED, HAD_MORALE, WAITING, MOVED, DEFENDING};

class CGameInfo;
extern CGameInfo* CGI;

const int HEROI_TYPE = 34, 
	TOWNI_TYPE = 98,
	CREI_TYPE = 54,
	EVENTI_TYPE = 26;

const int F_NUMBER = 9; //factions (town types) quantity
const int PLAYER_LIMIT = 8; //player limit per map
const int ALL_PLAYERS = 255; //bitfield
const int HEROES_PER_TYPE=8; //amount of heroes of each type
const int SKILL_QUANTITY=28;
const int SKILL_PER_HERO=8;
const int ARTIFACTS_QUANTITY=171;
const int HEROES_QUANTITY=156;
const int SPELLS_QUANTITY=70;
const int RESOURCE_QUANTITY=8;
const int TERRAIN_TYPES=10;
const int PRIMARY_SKILLS=4;
const int NEUTRAL_PLAYER=255;
const int NAMES_PER_TOWN=16;
const int CREATURES_PER_TOWN = 7; //without upgrades
const int MAX_BUILDING_PER_TURN = 1;
const int SPELL_LEVELS = 5;

#define BFIELD_WIDTH (17)
#define BFIELD_HEIGHT (11)
#define BFIELD_SIZE ((BFIELD_WIDTH) * (BFIELD_HEIGHT))

//uncomment to make it work
//#define MARK_BLOCKED_POSITIONS
//#define MARK_VISITABLE_POSITIONS

#define DEFBYPASS


#ifdef _WIN32
	#define DLL_F_EXPORT __declspec(dllexport)
#else
	#if defined(__GNUC__) && __GNUC__ >= 4
		#define DLL_F_EXPORT	__attribute__ ((visibility("default")))
	#else
		#define DLL_F_EXPORT
	#endif
#endif

#ifdef _WIN32
#define DLL_F_IMPORT __declspec(dllimport)
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define DLL_F_IMPORT	__attribute__ ((visibility("default")))
#else
#define DLL_F_IMPORT
#endif
#endif


#ifdef VCMI_DLL
	#define DLL_EXPORT DLL_F_EXPORT
#else
	#define DLL_EXPORT DLL_F_IMPORT
#endif

template<typename T, size_t N> char (&_ArrayCountObj(const T (&)[N]))[N];  
#define ARRAY_COUNT(arr)    (sizeof(_ArrayCountObj(arr)))

namespace vstd
{
	template <typename Container, typename Item>
	bool contains(const Container & c, const Item &i) //returns true if container c contains item i
	{
		return std::find(c.begin(),c.end(),i) != c.end();
	}
	template <typename V, typename Item, typename Item2>
	bool contains(const std::map<Item,V> & c, const Item2 &i) //returns true if map c contains item i
	{
		return c.find(i)!=c.end();
	}
	template <typename Container1, typename Container2>
	typename Container2::iterator findFirstNot(Container1 &c1, Container2 &c2)//returns first element of c2 not present in c1
	{
		typename Container2::iterator itr = c2.begin();
		while(itr != c2.end())
			if(!contains(c1,*itr))
				return itr;
			else
				++itr;
		return c2.end();
	}
	template <typename Container1, typename Container2>
	typename Container2::const_iterator findFirstNot(const Container1 &c1, const Container2 &c2)//returns const first element of c2 not present in c1
	{
		typename Container2::const_iterator itr = c2.begin();
		while(itr != c2.end())
			if(!contains(c1,*itr))
				return itr;
			else
				++itr;
		return c2.end();
	}
	template <typename Container, typename Item>
	typename Container::iterator find(const Container & c, const Item &i)
	{
		return std::find(c.begin(),c.end(),i);
	}
	template <typename T1, typename T2>
	int findPos(const std::vector<T1> & c, const T2 &s) //returns position of first element in vector c equal to s, if there is no such element, -1 is returned
	{
		for(size_t i=0; i < c.size(); ++i)
			if(c[i] == s)
				return i;
		return -1;
	}
	template <typename T1, typename T2, typename Func>
	int findPos(const std::vector<T1> & c, const T2 &s, const Func &f) //Func(T1,T2) must say if these elements matches
	{
		for(size_t i=0; i < c.size(); ++i)
			if(f(c[i],s))
				return i;
		return -1;
	}
	template <typename Container, typename Item>
	typename Container::iterator find(Container & c, const Item &i) //returns iterator to the given element if present in container, end() if not
	{
		return std::find(c.begin(),c.end(),i);
	}
	template <typename Container, typename Item>
	typename Container::const_iterator find(const Container & c, const Item &i)//returns const iterator to the given element if present in container, end() if not
	{
		return std::find(c.begin(),c.end(),i);
	}
	template <typename Container, typename Item>
	typename Container::size_type operator-=(Container &c, const Item &i) //removes element i from container c, returns false if c does not contain i
	{
		typename Container::iterator itr = find(c,i);
		if(itr == c.end())
			return false;
		c.erase(itr);
		return true;
	}
	template <typename t1>
	void delObj(t1 *a1)
	{
		delete a1;
	}
	template <typename t1, typename t2>
	void assign(t1 &a1, const t2 &a2)
	{
		a1 = a2;
	}
	template <typename t1, typename t2>
	struct assigner
	{
	public:
		t1 &op1;
		t2 op2;
		assigner(t1 &a1, const t2 & a2)
			:op1(a1), op2(a2)
		{}
		void operator()()
		{
			op1 = op2;
		}
	};
	template <typename t1, typename t2>
	assigner<t1,t2> assigno(t1 &a1, const t2 &a2)
	{
		return assigner<t1,t2>(a1,a2);
	}
	template <typename t1, typename t2, typename t3>
	bool equal(const t1 &a1, const t3 t1::* point, const t2 &a2)
	{
		return a1.*point == a2;
	}
	template <typename t1, typename t2>
	bool equal(const t1 &a1, const t2 &a2)
	{
		return a1 == a2;
	}
}
using vstd::operator-=;

template <typename t1, typename t2>
t1 & amax(t1 &a, const t2 &b) //assigns greater of (a, b) to a and returns maximum of (a, b)
{
	if(a >= b)
		return a;
	else
	{
		a = b;
		return a;
	}
}

template <typename t1, typename t2>
t1 & amin(t1 &a, const t2 &b) //assigns smaller of (a, b) to a and returns minimum of (a, b)
{
	if(a <= b)
		return a;
	else
	{
		a = b;
		return a;
	}
}

template <typename t1, typename t2, typename t3>
t1 & abetw(t1 &a, const t2 &b, const t3 &c) //makes a to fit the range <b, c>
{
	amax(a,b);
	amin(a,c);
	return a;
}

#include "CConsoleHandler.h"
extern DLL_EXPORT std::ostream *logfile;
extern DLL_EXPORT CConsoleHandler *console;
template <int lvl> class CLogger //logger, prints log info to console and saves in file
{
public:
	CLogger<lvl>& operator<<(std::ostream& (*fun)(std::ostream&))
	{
		if(lvl < CONSOLE_LOGGING_LEVEL)
			std::cout << fun;
		if((lvl < FILE_LOGGING_LEVEL) && logfile)
			*logfile << fun;
		return *this;
	}

	template<typename T> 
	CLogger<lvl> & operator<<(const T & data)
	{
		if(lvl < CONSOLE_LOGGING_LEVEL)
		{
			if(console)
			{
				console->print(data,lvl);
			}
			else
			{
				std::cout << data << std::flush;
			}
		}
		if((lvl < FILE_LOGGING_LEVEL) && logfile)
		{
			*logfile << data << std::flush;
		}
		return *this;
	}
};

extern DLL_EXPORT CLogger<0> tlog0; //green - standard progress info
extern DLL_EXPORT CLogger<1> tlog1; //red - big errors
extern DLL_EXPORT CLogger<2> tlog2; //magenta - major warnings
extern DLL_EXPORT CLogger<3> tlog3; //yellow - minor warnings
extern DLL_EXPORT CLogger<4> tlog4; //white - detailed log info
extern DLL_EXPORT CLogger<5> tlog5; //gray - minor log info

//XXX pls dont - 'debug macros' are usually more trouble than it's worth
#define HANDLE_EXCEPTION  \
	catch (const std::exception& e) {	\
	tlog1 << e.what() << std::endl;		\
	throw;								\
	}									\
	catch (const std::exception * e)	\
	{									\
		tlog1 << e->what()<< std::endl;	\
		throw;							\
	}									\
	catch (const std::string& e) {		\
		tlog1 << e << std::endl;		\
		throw;							\
	}

#define HANDLE_EXCEPTIONC(COMMAND)  \
	catch (const std::exception& e) {	\
		COMMAND;						\
		tlog1 << e.what() << std::endl;	\
		throw;							\
	}									\
	catch (const std::exception * e)	\
	{									\
		COMMAND;						\
		tlog1 << e->what()<< std::endl;	\
		throw;							\
	}


#if defined(linux) && defined(sparc) 
/* SPARC does not support unaligned memory access. Let gcc know when
 * to emit the right code. */
struct unaligned_Uint16 { ui16 val __attribute__(( packed )); };
struct unaligned_Uint32 { ui32 val __attribute__(( packed )); };

static inline ui16 read_unaligned_u16(const void *p)
{
        const struct unaligned_Uint16 *v = (const struct unaligned_Uint16 *)p;
        return v->val;
}

static inline ui32 read_unaligned_u32(const void *p)
{
        const struct unaligned_Uint32 *v = (const struct unaligned_Uint32 *)p;
        return v->val;
}

#else
#define read_unaligned_u16(p) (* reinterpret_cast<const Uint16 *>(p))
#define read_unaligned_u32(p) (* reinterpret_cast<const Uint32 *>(p))
#endif

#endif // __GLOBAL_H__
