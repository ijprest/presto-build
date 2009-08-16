/*SDOC***********************************************************************

	Module:				make.cpp

	Copyright (C) 2009 Ian Prest
	http://ijprest.github.com/presto-build/license.html

	Description:	Main entry point for the application

***********************************************************************EDOC*/
#include "stdafx.h"
#include <signal.h>
#include "lmakelib.h"

// Global Lua state; used by the signal handlers
static lua_State* g_L = NULL;

static void lstop(lua_State* L, lua_Debug* /*ar*/) {
	lua_sethook(L, NULL, 0, 0);
	luaL_error(L, "interrupted!");
}
static void laction(int i) {
	// if another SIGINT happens before lstop, terminate process (default action)
	signal(i, SIG_DFL); 
	lua_sethook(g_L, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

static void print_version(void) {
	fprintf(stderr, "Presto Build 0.1 (new-wolf-moon), Copyright (C) 2009 Ian Prest\n"
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
	"  -e STAT                     execute string STAT as lua code\n"
	"  -f FILE                     Read FILE as a makefile.\n"
	"  -h                          Print this message and exit.\n"
	"  -j [N]                      Allow N jobs at once; infinite jobs with no arg.\n"
	"  -k                          Keep going when some targets can't be made.\n"
	"  -l LIBRARY                  require lua library LIBRARY\n"
	"  -q                          Run no commands; exit status says if up to date.\n"
	"  -s                          Don't echo commands.\n"
	"  -v                          Print the version number of make and exit.\n");
	fflush(stderr);
}

static void l_message(const char* msg) {
	fprintf(stderr, "presto: %s\n", msg);
	fflush(stderr);
}


static int report(lua_State* L, int status) {
	if(status && !lua_isnil(L, -1)) {
		const char* msg = lua_tostring(L, -1);
		if(msg == NULL) msg = "(error object is not a string)";
		l_message(msg);
		lua_pop(L, 1);
	}
	return status;
}


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


static int docall(lua_State* L, int narg, int clear) {
	int status;
	int base = lua_gettop(L) - narg;  // function index
	lua_pushcfunction(L, traceback);  // push traceback function
	lua_insert(L, base);  // put it under chunk and args 
	signal(SIGINT, laction);
	status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
	signal(SIGINT, SIG_DFL);
	lua_remove(L, base);  // remove traceback function
	// force a complete garbage collection in case of errors
	if(status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
	return status;
}

static int dofile(lua_State* L, const char* name) {
	int status = luaL_loadfile(L, name) || docall(L, 0, 1);
	return report(L, status);
}


static int dostring(lua_State* L, const char* s, const char* name) {
	int status = luaL_loadbuffer(L, s, strlen(s), name) || docall(L, 0, 1);
	return report(L, status);
}


static int dolibrary(lua_State* L, const char* name) {
	lua_getglobal(L, "require");
	lua_pushstring(L, name);
	return report(L, docall(L, 1, 1));
}

static int handle_luainit(lua_State* L) {
	const char* init = getenv("PRESTO_INIT");
	if(init == NULL) return 0;  // status OK
	else if(init[0] == '@')
		return dofile(L, init+1);
	else
		return dostring(L, init, "=PRESTO_INIT");
}


// Smain structure - 
struct Smain {
	int argc;
	char** argv;
	int status;
};


static int set_flag(lua_State* L, const char* name, int value) {
	lua_getglobal(L, "make");
	lua_getfield(L, -1, "flags");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_pushstring(L, name);
	lua_pushboolean(L, value);
	lua_settable(L, -3);
	lua_pop(L, 2); // pop "make" and "flags"
	return 0;
}

static int set_max_jobs(lua_State* L, int max_jobs) {
	lua_getglobal(L, "make");
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
	s->status = handle_luainit(L);

	// Parse the arguments
	bool parsing_switches = true;
	bool loaded_file = false;
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
						print_usage();
						s->status = 1;
						return 0;
					}
				}

				// Normal switch(es)
				for(char* sw = &s->argv[i][1]; sw && *sw; sw++) {
					switch(*sw) {
					case 'B':
						set_flag(L, "always_make", 1); break;
					case 'd':
						set_flag(L, "debug", 1); break;
					case 'k':
						set_flag(L, "keep_going", 1); break;
					case 'q':
						set_flag(L, "question", 1); break;
					case 's':
						set_flag(L, "silent", 1); break;
					case 'h':
						print_usage(); s->status = 1; return 0;
					case 'v':
						print_version(); s->status = 1; return 0;
					case 'C': // change directory
						if(*(sw+1)) { print_usage(); s->status = 1; return 0; }
						lua_pushstring(L, s->argv[i+1]);
						make_dir_cd(L);
						lua_pop(L,2);
						i++; // skip the next paramter
						break;
					case 'e': // execute lua statement
						if(*(sw+1)) { print_usage(); s->status = 1; return 0; }
						s->status = dostring(L, s->argv[i+1], "=(command line)");
						if(s->status != 0) return 0;
						i++;
						break;
					case 'f': // execute file
						if(*(sw+1)) { print_usage(); s->status = 1; return 0; }
						s->status = dofile(L, s->argv[i+1]);
						if(s->status != 0) return 0;
						i++;
						loaded_file = true;
						break;
					case 'l': // require library
						if(*(sw+1)) { print_usage(); s->status = 1; return 0; }
						s->status = dolibrary(L, s->argv[i+1]);
						if(s->status != 0) return 0;
						i++;
						break;
					case 'j':	// max_jobs
						if(!*(sw+1)) { 
							set_max_jobs(L, atoi(s->argv[i+1]));
						} if(isdigit(*(sw+1))) {
							set_max_jobs(L, atoi(sw+1));
						} else {
							print_usage(); s->status = 1; return 0;
						}
						i++;
						break;
					default:	// unrecognized switch
						print_usage(); 
						s->status = 1; 
						return 0;
					}
				}
				continue;
			}
		}
		// parameter is not a switch

		if(strchr(s->argv[i],'=')) {
			// parameter is a variable assignment; we override the 
			// environment in make.env
			lua_getglobal(L, "make");
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
			lua_getglobal(L, "make");
			lua_getfield(L, -1, "goals");
			luaL_checktype(L, -1, LUA_TTABLE);
			lua_pushstring(L, s->argv[i]);
			lua_pushboolean(L, 1);
			lua_settable(L, -3);
			lua_pop(L, 2); // "make", "goals"
		}
	}

	// If we didn't load any files, try to load makefile.lua 
	// in the current directory.
	if(!loaded_file) {
		// try makefile.lua
		if(PathFileExistsA("makefile.lua")) {
			s->status = dofile(L, "makefile.lua");
		} else if(PathFileExistsA("makefile")) {
			s->status = dofile(L, "makefile");
		} else {
			luaL_error(L, "*** No targets specified and no makefile found.  Stop.");
		}
		if(s->status != 0) return 0;
	}
			
	// call: make.update_goals()
	lua_getglobal(L, "make");
	lua_getfield(L,-1,"update_goals");
	lua_remove(L,-2); // remove "make"
	luaL_checktype(L, -1, LUA_TFUNCTION);
	s->status = docall(L,0,1);

	return 0;
}


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

