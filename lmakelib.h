/*SDOC***********************************************************************

	Module:				lmakelib.h

	Copyright (C) 2009 Ian Prest
	http://ijprest.github.com/presto-build/license.html

	Description:	Presto-specific library of functions that are exposed
								to the running Lua script.

***********************************************************************EDOC*/
#ifndef lmakelib_h
#define lmakelib_h
#pragma once

#define LUA_MAKELIBNAME	"make"
LUALIB_API int (luaopen_make) (lua_State *L);

extern int make_dir_cd(lua_State *L);

#endif // lmakelib_h
