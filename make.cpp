/*SDOC***********************************************************************

	Module:				make.cpp

	Copyright (C) 2009-2014 Ian Prest
	http://ijprest.github.com/presto-build/license.html

	Description:	Main entry point for the application

***********************************************************************EDOC*/
#include "stdafx.h"
#include <signal.h>
#include "lmakelib.h"

// Global Lua state; used by the signal handlers
static lua_State* g_L = NULL;

// Smain structure - used to pass arguments & status between the protected
// pmain() and unprotected main().
struct Smain {
	int argc;
	char** argv;
	int status;
};


/*SDOC***********************************************************************

	Name:			print_version
						print_usage
						l_message

	Action:		Helper functions to print common error strings.

***********************************************************************EDOC*/
static void print_version(void) {
	fprintf(stderr, "Presto Build 0.1 (new-wolf-moon), Copyright (C) 2009-2014 Ian Prest\n"
									LUA_RELEASE ", " LUA_COPYRIGHT "\n");
	fflush(stderr);
}

static void print_usage(void) {
	fprintf(stderr,
	"Usage: presto [options] [target] ...\n"
	"Options:\n"
	"  -B                          Unconditionally make all targets.\n"
	"  -C DIRECTORY                Change to DIRECTORY before doing anything.\n"
	"  -d                          Print lots of debugging information.\n"
	"  -e STAT                     Execute string STAT as lua code\n"
	"  -f FILE                     Read FILE as a makefile.\n"
	"  -h                          Print this message and exit.\n"
	"  -j [N]                      Allow N jobs at once.\n"
	"  -k                          Keep going when some targets can't be made.\n"
	"  -l LIBRARY                  Require lua library LIBRARY\n"
	"  -n                          Noisy; echo commands as they run.\n"
	"  -q                          Run no commands; exit status says if up to date.\n"
	"  -Q                          Just run the lua code and exit.\n"
	"  -v                          Print the version number of make and exit.\n");
	fflush(stderr);
}

static void l_message(const char* msg) {
	fprintf(stderr, "presto: *** %s\n", msg);
	fflush(stderr);
}


/*SDOC***********************************************************************

	Name:			report

	Action:		Report a lua error

***********************************************************************EDOC*/
static int report(lua_State* L, int status) {
	if(status && !lua_isnil(L, -1)) {
		const char* msg = lua_tostring(L, -1);
		if(msg == NULL) msg = "(error object is not a string)";
		l_message(msg);
		lua_pop(L, 1);
	}
	return status;
}


/*SDOC***********************************************************************

	Name:			setsignal
						lstop
						laction

	Action:		Signal handlers when Lua code is running

***********************************************************************EDOC*/
typedef void(*psighndlr)(int);
psighndlr setsignal(psighndlr sighndlr) {
	signal(SIGABRT, sighndlr);
	signal(SIGBREAK, sighndlr);	
	signal(SIGTERM, sighndlr);
	return signal(SIGINT, sighndlr);
}

static void lstop(lua_State* L, lua_Debug* /*ar*/) {
	lua_sethook(L, NULL, 0, 0);
	luaL_error(L, "interrupted!");
}
static void laction(int i) {
	// if another SIGINT happens before lstop, terminate process (default action)
	signal(i, SIG_DFL); 
	lua_sethook(g_L, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}


/*SDOC***********************************************************************

	Name:			traceback

	Action:		Report stack traces when there is a lua error

***********************************************************************EDOC*/
static int traceback(lua_State* L) {
	if(!lua_isstring(L, 1))		// 'message' not a string?
		return 1;								// keep it intact
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if(!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return 1;
	}
	lua_getfield(L, -1, "traceback");
	if(!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return 1;
	}
	lua_pushvalue(L, 1);		// pass error message
	lua_pushinteger(L, 2);  // skip this function and traceback
	lua_call(L, 2, 1);			// call debug.traceback
	return 1;
}


/*SDOC***********************************************************************

	Name:			docall

	Action:		Call a chunk of Lua code

***********************************************************************EDOC*/
static int docall(lua_State* L, int narg, int stack = 1) {
	int base = 0; 
	if(stack) {
		base = lua_gettop(L) - narg;	// function index
		lua_pushcfunction(L, traceback);  // push traceback function
		lua_insert(L, base);  // put it under chunk and args 
	}
	psighndlr oldsig = setsignal(laction);
	int status = lua_pcall(L, narg, 0, base);
	setsignal(oldsig);
	if(stack) {
		lua_remove(L, base);  // remove traceback function
	}
	// force a complete garbage collection in case of errors
	if(status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
	return status;
}


/*SDOC***********************************************************************

	Name:			dofile
						dostring
						dolibrary

	Action:		Execute a string or file, or require a Lua library.

***********************************************************************EDOC*/
static int dofile(lua_State* L, const char* name) {
	int status = luaL_loadfile(L, name) || docall(L, 0);
	return report(L, status);
}

static int dostring(lua_State* L, const char* s, const char* name) {
	int status = luaL_loadbuffer(L, s, strlen(s), name) || docall(L, 0);
	return report(L, status);
}

static int dolibrary(lua_State* L, const char* name) {
	lua_getglobal(L, "require");
	lua_pushstring(L, name);
	return report(L, docall(L, 1));
}


/*SDOC***********************************************************************

	Name:			set_flag
						set_max_jobs

	Action:		Helpers used by the command-line parsing code to set a lua
						flag.

***********************************************************************EDOC*/
static int set_flag(lua_State* L, const char* name, int value) {
	lua_getglobal(L, LUA_MAKELIBNAME);
	lua_getfield(L, -1, "flags");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushstring(L, name);
	lua_pushboolean(L, value);
	lua_settable(L, -3);
	lua_pop(L, 2); // pop "make" and "flags"
	return 0;
}

static int set_max_jobs(lua_State* L, int max_jobs) {
	if(!max_jobs) max_jobs = 1024; // some ridiculous number
	lua_getglobal(L, LUA_MAKELIBNAME);
	lua_getfield(L, -1, "jobs");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushstring(L, "slots");
	lua_pushnumber(L, max_jobs);
	lua_settable(L, -3);
	lua_pop(L, 2); // pop "make" and "jobs"
	return 0;
}


/*SDOC***********************************************************************

	Name:			pmain

	Action:		Main program loop; called in protected mode by main()

	Params:		[1] Smain* - light user data, contains program arguments

***********************************************************************EDOC*/
#define bad_usage() (print_usage(), (s->status = 1), 0)
#define handle_status(exp) {if((s->status = (exp)) != 0) return 0;}
#define get_arg() {if(!*(sw+1) && s->argv[i+1]) {arg = s->argv[i+1];i++;} else if(*(sw+1)) {arg = sw+1;sw = NULL;} else {return bad_usage();}}
static int pmain(lua_State* L) {
	struct Smain* s = (struct Smain*)lua_touserdata(L, 1);
	g_L = L;

	// One-time initialization
	lua_gc(L, LUA_GCSTOP, 0);			// stop garbage collector during init
	luaL_openlibs(L);							// open std libraries (string, table, etc.)
	luaopen_make(L);							// open custom "make" library
	dolibrary(L, "mkinit");				// require mkinit.lua code
	lua_gc(L, LUA_GCRESTART, 0);	// restart the garbage collector

	// Handle any intialization code in the PRESTO_INIT environment variable
	const char* init = getenv("PRESTO_INIT");
	if(init && init[0] == '@') {
		handle_status(dofile(L, init+1));
	} else if(init) {
		handle_status(dostring(L, init, "=PRESTO_INIT"));
	}

	// Parse the arguments
	bool parsing_switches = true;
	bool loaded_file = false;
	bool quit = false;
	for(int i = 1; s->argv[i] != NULL; i++) {
		if(parsing_switches) {
			if(s->argv[i][0] == '-') {
				// This looks like a switch
				if(s->argv[i][1] == '-') {
					if(s->argv[i][2] == 0) {
						// "--" turns off switch parsing
						parsing_switches = false;
						continue;
					} else {
						// we don't currently support GNU-style long switch names
						return bad_usage();
					}
				}

				// Normal switch(es)
				char* arg = 0;
				for(char* sw = &s->argv[i][1]; sw && *sw; sw ? sw++ : 0) {
					switch(*sw) {
					case 'B': set_flag(L, "always_make", 1); break;
					case 'd': set_flag(L, "debug", 1); break;
					case 'k': set_flag(L, "keep_going", 1); break;
					case 'n': set_flag(L, "noisy", 1); break;
					case 'q': set_flag(L, "question", 1); break;
					case 'Q': quit = true; break;
					case 'v': print_version(); s->status = 1; return 0;
					case 'C': get_arg();	// change directory
						lua_pushstring(L, arg);
						make_dir_cd(L);
						lua_pop(L,2);
						break;
					case 'e': get_arg();	// execute lua statement
						handle_status(dostring(L, arg, "=(command line)"));
						break;
					case 'f': get_arg();	// execute file
						handle_status(dofile(L, arg));
						loaded_file = true;
						break;
					case 'l': get_arg();	// require library
						handle_status(dolibrary(L, arg));
						break;
					case 'j':	get_arg();	// max_jobs
						set_max_jobs(L, atoi(arg));
						break;
					case 'h':
					default:	// unrecognized switch
						return bad_usage();
					}
				}
				continue;
			}
		}
		// parameter is not a switch

		if(strchr(s->argv[i],'=')) {
			// parameter is a variable assignment; we override the 
			// environment in make.env
			lua_getglobal(L, LUA_MAKELIBNAME);
			lua_getfield(L, -1, "env");
			luaL_checktype(L, -1, LUA_TTABLE);

			// extract the key name
			char* buffer = (char*)_alloca(strlen(s->argv[i]));
			char* key = buffer;
			char* pos = s->argv[i];
			while(*pos && *pos != '=') *key++ = toupper(*pos++);
			*key++ = 0; pos++;
			lua_pushstring(L,buffer);

			// extract the value
			key = buffer;
			while(*pos) *key++ = *pos++;
			*key++ = 0;
			lua_pushstring(L,buffer);

			// set the value into the environment table
			lua_settable(L, -3);
			lua_pop(L, 2); // "make", "env"
		} else {
			// parameter is a goal
			lua_getglobal(L, LUA_MAKELIBNAME);
			lua_getfield(L, -1, "goals");
			luaL_checktype(L, -1, LUA_TTABLE);
			lua_pushstring(L, s->argv[i]);
			lua_pushboolean(L, 1);
			lua_settable(L, -3);
			lua_pop(L, 2); // "make", "goals"
		}
	}

	// If asked, we exit out without trying to build any goals.
	// This is useful if the user just wants to run his Lua code.
	if(quit) return 0;	

	// If we didn't load any files, try to load makefile.lua 
	// in the current directory.
	if(!loaded_file) {
		// try makefile.lua
		if(PathFileExistsA("makefile.lua")) {
			handle_status(dofile(L, "makefile.lua"));
		} else if(PathFileExistsA("makefile")) {
			handle_status(dofile(L, "makefile"));
		} else {
			luaL_error(L, "No targets specified and no makefile found.  Stop.");
		}		
	}
			
	// call: make.update_goals()
	lua_getglobal(L, LUA_MAKELIBNAME);
	lua_getfield(L,-1,"update_goals");
	lua_remove(L,-2); // remove "make"
	luaL_checktype(L, -1, LUA_TFUNCTION);
	if(report(L, docall(L,0,0))) {
		lua_pushnil(L);
		lua_error(L);
	}
	return 0;
}
#undef bad_usage
#undef handle_status
#undef get_arg

/*SDOC***********************************************************************

	Name:		main

	Action:	Main entry point for the application

***********************************************************************EDOC*/
int main(int argc, char** argv) {
	// create lua state
	lua_State* L = lua_open();
	if(L == NULL) {
		l_message("cannot create state: not enough memory");
		return EXIT_FAILURE;
	}

	// Run the main function in protected mode
	struct Smain s = { argc, argv };
	int status = lua_cpcall(L, &pmain, &s);
	report(L, status);

	// destroy the lua state and return
	lua_close(L);
	return(status || s.status) ? EXIT_FAILURE : EXIT_SUCCESS;
}

