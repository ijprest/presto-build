#ifndef lmakelib_h
#define lmakelib_h

#include "lua.h"

#define LUA_MAKELIBNAME	"make"
LUALIB_API int (luaopen_make) (lua_State *L);

#endif // lmakelib_h
