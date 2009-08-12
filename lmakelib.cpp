/*
** $Id: lstrlib.c,v 1.132.1.4 2008/07/11 17:27:21 roberto Exp $
** Standard library for string operations and pattern-matching
** See Copyright Notice in lua.h
*/
#include "stdafx.h"
#include "lmakelib.h"
extern "C" {
#include "md5.h"
}
#include <io.h>
#include <fcntl.h>

/* macro to `unsign' a character */
#define uchar(c)        ((unsigned char)(c))

/*
** "make.path" sub-library
*/

inline char* _lua_topath(char* out, const char* in, size_t* len)
{
	// copy the input string
	memcpy(out, in, (*len)+1);
	// Swap slashes
	for (size_t i=0; i<(*len); i++)
		if(out[i] == '/') out[i] = '\\';
	return out;
}
#define lua_topath(in, len) (_lua_topath((char*)_alloca((*(len))+1), in, len))

#define lua_path_init()	const char* lua_getpath_s = 0;
#define lua_getpath(L, stack_pos, len) \
	((lua_getpath_s = luaL_checklstring(L, stack_pos, len)), lua_topath( lua_getpath_s, len ))

inline void lua_pushpath(lua_State *L, const char* path)
{
	// swap slashes back
  luaL_Buffer b;
	luaL_buffinit(L, &b);
	size_t len = strlen(path);
	for (size_t i=0; i<len; i++)
	{
		if(path[i] == '\\') 
			luaL_addchar(&b, '/');
		else
			luaL_addchar(&b, uchar(path[i]));
	}
  luaL_pushresult(&b);
}

static int make_path_tempdir (lua_State *L) {
	char path_out[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, path_out);
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_tempfile (lua_State *L) {
	char dir[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, dir);
	char path_out[MAX_PATH] = {};
	GetTempFileNameA(dir, "lmk", 0, path_out);
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_short (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	GetShortPathNameA(path_in, path_out, MAX_PATH);
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_long (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	GetLongPathNameA(path_in, path_out, MAX_PATH);
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_full (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	GetFullPathNameA(path_in, MAX_PATH, path_out, NULL);
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_canonicalize (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	PathCanonicalizeA(path_out, path_in);
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_add_slash (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathAddBackslashA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_remove_slash (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathRemoveBackslashA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_remove_extension (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathRemoveExtensionA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_quote (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathQuoteSpacesA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_unquote (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathUnquoteSpacesA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_get_extension (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	const char* path_out = PathFindExtensionA(path_in);
	lua_pushpath(L, path_out ? path_out : "");
  return 1;
}

static int make_path_get_name (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	const char* path_out = PathFindFileNameA(path_in);
	lua_pushpath(L, path_out ? path_out : "");
  return 1;
}

static int make_path_get_dir (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char* path_out = PathFindFileNameA(path_in);
	*path_out = 0;
	lua_pushpath(L, path_in);
  return 1;
}

static int make_path_exists (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	BOOL result = PathFileExistsA(path_in);
	lua_pushboolean(L, result ? 1 : 0);
  return 1;
}

static int make_path_is_dir (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	BOOL result = PathIsDirectoryA(path_in);
	lua_pushboolean(L, result ? 1 : 0);
  return 1;
}

static int make_path_is_dir_empty (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	BOOL result = PathIsDirectoryEmptyA(path_in);
	lua_pushboolean(L, result ? 1 : 0);
  return 1;
}

static int make_path_is_relative (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	BOOL result = PathIsRelativeA(path_in);
	lua_pushboolean(L, result ? 1 : 0);
  return 1;
}

static int make_path_add_extension(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path1);
	PathAddExtensionA(path_out, path2);
	lua_pushpath(L, path_out);
	return 1;
}

static int make_path_change_extension(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path1);
	PathRenameExtensionA(path_out, path2);
	lua_pushpath(L, path_out);
	return 1;
}

static int make_path_combine(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	char path_out[MAX_PATH] = {};
	PathCombineA(path_out, path1, path2);
	lua_pushpath(L, path_out);
	return 1;
}

static int make_path_common(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	char path_out[MAX_PATH] = {};
	PathCommonPrefixA(path1, path2, path_out);
	lua_pushpath(L, path_out);
	return 1;
}

static int make_path_touch (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	HANDLE hFile = CreateFileA(path_in, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE)
		luaL_error(L, "error touching file '%s'", path_in);
	SYSTEMTIME st = {};
	FILETIME ft = {};
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	SetFileTime(hFile, &ft, &ft, &ft);
	CloseHandle(hFile);
  return 0;
}

static int make_path_copy(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	if(!CopyFileA(path1, path2, FALSE))
		luaL_error(L, "error copying file '%s' to '%s'", path1, path2);
	return 0;
}

static int make_path_delete(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	if(!DeleteFileA(path_in))
		luaL_error(L, "error deleting file '%s'", path_in);
	return 0;
}

static int make_path_getsize (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	HANDLE hFile = CreateFileA(path_in, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE)
		luaL_error(L, "error reading file size '%s'", path_in);
	LARGE_INTEGER size;
	GetFileSizeEx(hFile, &size);
	CloseHandle(hFile);
	lua_pushnumber(L,(double)size.QuadPart);
  return 1;
}

static __int64 start_time = 0;
static int make_path_filetime (lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	HANDLE hFile = CreateFileA(path_in, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE)
		luaL_error(L, "error reading file time '%s'", path_in);
	FILETIME ft;
	GetFileTime(hFile, NULL, NULL, &ft);
	CloseHandle(hFile);
	__int64 time = ((__int64)ft.dwLowDateTime | (((__int64)ft.dwHighDateTime)<<32)) - start_time;
	lua_pushnumber(L,(double)time * 1.0e-7);
  return 1;
}

static int make_now (lua_State *L) {
	SYSTEMTIME st;
	GetSystemTime(&st);
	FILETIME ft;
	SystemTimeToFileTime(&st,&ft);
	__int64 time = ((__int64)ft.dwLowDateTime | (((__int64)ft.dwHighDateTime)<<32)) - start_time;
	lua_pushnumber(L,(double)time * 1.0e-7);
	return 1;
}

static int make_path_cd(lua_State *L) {
	if(lua_gettop(L)) {// optional directory
		lua_path_init();
		size_t l; char* path_in = lua_getpath(L, 1, &l);
		SetCurrentDirectoryA(path_in);
	}
	char path_out[MAX_PATH] = {};
	GetCurrentDirectoryA(MAX_PATH, path_out);
	lua_pushpath(L, path_out);
	return 1;
}

static int make_path_md(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);
	if(!CreateDirectoryA(path_in, NULL))
		luaL_error(L, "error creating directory '%s'", path_in);
	return 0;
}

static int make_path_rd(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);
	if(!RemoveDirectoryA(path_in))
		luaL_error(L, "error removing directory '%s'", path_in);
	return 0;
}

static int make_path_glob(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);

	lua_newtable(L);
	WIN32_FIND_DATAA finddata;
	HANDLE hFind = FindFirstFileA(path_in, &finddata);
	int pos = 1;
	if(hFind != INVALID_HANDLE_VALUE) {
		do {
			if(strcmp(finddata.cFileName,".") && strcmp(finddata.cFileName,"..")) {
				lua_pushnumber(L, pos++);							 // key
				lua_pushstring(L, finddata.cFileName); // value
				lua_settable(L, -3);									 // t[key] = value
			}
		} while(FindNextFileA(hFind, &finddata));
		FindClose(hFind);
	}
	return 1;
}

// make.path.where("notepad.exe")
// make.path.where("notepad.exe",{"c:/program files","c:/windows"})
// make.path.where("notepad.exe","c:/program files;c:/windows")
static int make_path_where(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	int pos = 1;
	if(lua_gettop(L) >= 2) { 
		if(lua_isstring(L,2) && !lua_isnumber(L,2)) { 
			// semi-colon separated string
			size_t l2; char* path = lua_getpath(L, 2, &l2);
			SearchPathA(path,path_in,NULL,MAX_PATH,path_out,NULL);
		} else if(lua_istable(L,2))  {
			// extract all the path strings
			char path[32768];
			char* path_pos = path;
			while(1) {
				lua_pushnumber(L,pos++);
				lua_gettable(L,2);
				if(lua_isnil(L,-1)) 
					break;
				size_t lua_path_len;
				const char* lua_path = luaL_checklstring(L, -1, &lua_path_len);
				_lua_topath(path_pos, lua_path, &lua_path_len);
				path_pos += strlen(path_pos);
				*path_pos++ = ';';
				*path_pos = 0;
			}
			SearchPathA(path,path_in,NULL,MAX_PATH,path_out,NULL);
		} else {
			luaL_error(L, "expected string or table for search path");
		}
	} else { 
		// use %PATH%
		SearchPathA(NULL,path_in,NULL,MAX_PATH,path_out,NULL);
	}
	lua_pushpath(L, path_out);
	return 1;
}

inline void lua_pushhex(lua_State *L, const unsigned char* buffer, size_t len) {
	static unsigned char hexchars[] = "0123456789abcdef";
  luaL_Buffer b;
	luaL_buffinit(L, &b);
	for (size_t i=0; i<len; i++) {
		luaL_addchar(&b, hexchars[(buffer[i] & 0xf0) >> 4]);
		luaL_addchar(&b, hexchars[buffer[i] & 0x0f]);
	}
  luaL_pushresult(&b);
}

static int make_md5(lua_State *L) {
	// get the input string
	size_t len;
	const char* string = luaL_checklstring(L, 1, &len);

	// compute the md5 hash
	MD5_CTX md5;
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char*)string, len); // cast OK; doesn't actually change input
	MD5Final(&md5);

	// send the result to lua
	lua_pushhex(L, md5.digest, sizeof(md5.digest));
	return 1;
}

static int make_file_md5(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);

	// compute the md5 hash
	HANDLE hFile = CreateFileA(path_in, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE)
		luaL_error(L, "error opening file '%s' for reading", path_in);
	MD5_CTX md5;
	MD5Init(&md5);

	// Read & process 4k at a time
	unsigned char buffer[4096];
	DWORD dwRead;
	do {
		ReadFile(hFile, buffer, sizeof(buffer), &dwRead, NULL);
		MD5Update(&md5, buffer, dwRead);
	} while(dwRead == sizeof(buffer));
	CloseHandle(hFile);
	MD5Final(&md5);

	// send the result to lua
	lua_pushhex(L, md5.digest, sizeof(md5.digest));
	return 1;
}

static int make_path_to_os(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);	// this will convert to backslashes
	lua_pushstring(L,path_in);
	return 1;
}

static int make_path_from_os(lua_State *L) {
	size_t l; 
	const char* path_in = luaL_checklstring(L, 1, &l);
	lua_pushpath(L, path_in);	// this will convert to regular slashes
	return 1;
}

static int make_proc_spawn(lua_State *L) {
	size_t l; 
	const char* path_in = luaL_checklstring(L, 1, &l);
	char* command_line = (char*)_alloca(l+1);
	strcpy(command_line, path_in);
	const char* env = NULL;

	// Create the child output pipe (inheritable)
	HANDLE hOutputReadTmp, hOutputWrite;
	SECURITY_ATTRIBUTES sa = { sizeof(sa) };
	sa.bInheritHandle = TRUE;
	if(!CreatePipe(&hOutputReadTmp, &hOutputWrite, &sa, 0)) // TODO: should buffer size be bigger?
		luaL_error(L, "error creating pipe");
	// Duplicate the pipe for stderr; this way, if the child 
	// process closes one of stdout/stderr, the other still works.
	HANDLE hErrorWrite;
  if(!DuplicateHandle(GetCurrentProcess(), hOutputWrite, GetCurrentProcess(), &hErrorWrite, 0, TRUE, DUPLICATE_SAME_ACCESS))
     luaL_error(L, "error duplicating pipe handle");
  // Duplicate the output read handle as uninheritable; otherwise
  // the child inherits it and a non-closeable handles to the pipe
  // is created.
	HANDLE hOutputRead;
  if(!DuplicateHandle(GetCurrentProcess(), hOutputReadTmp, GetCurrentProcess(), &hOutputRead, 0, FALSE, DUPLICATE_SAME_ACCESS))
    luaL_error(L, "error duplicating pipe handle");
  // Close the inheritable copy
  if(!CloseHandle(hOutputReadTmp)) 
		luaL_error(L, "error closing pipe handle");

	// Launch the child process
	STARTUPINFOA si = { sizeof(si) };
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = INVALID_HANDLE_VALUE; // no input!
	si.hStdOutput = hOutputWrite;
	si.hStdError = hErrorWrite;
	PROCESS_INFORMATION pi = {};
	CreateProcessA(NULL, command_line, NULL, NULL, TRUE, 0, (void*)env, NULL, &si, &pi);
	// Close unnecessary thread handle
	CloseHandle(pi.hThread);

	// Close the pipe handles; we make sure to not maintain any
	// handles to the write end of the pipes so that the child
	// can exit properly.
	if(!CloseHandle(hOutputWrite) || !CloseHandle(hErrorWrite)) 
		luaL_error(L, "error closing pipe handle");

	// Return the process information to lua
	lua_newtable(L);
	lua_pushstring(L, "procid");
	lua_pushnumber(L, (DWORD)pi.hProcess);
	lua_settable(L, -3);
	lua_pushstring(L, "procread");
	lua_pushnumber(L, (DWORD)hOutputRead);
	lua_settable(L, -3);
	return 1;
}

static int make_proc_flushio(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_pushstring(L, "procread");
	lua_gettable(L, 1);
	if(lua_isnil(L,-1)) 
		luaL_error(L, "not a proper process descriptor");
	HANDLE hReadHandle = (HANDLE)(DWORD)luaL_checknumber(L, -1);
	lua_pop(L,1);

	DWORD dwAvailable;
	if(!PeekNamedPipe(hReadHandle, NULL, 0, NULL, &dwAvailable, NULL))
		goto error;
	if(dwAvailable > 0)
	{
		char* buffer = (char*)_alloca(dwAvailable+1);
		buffer[dwAvailable] = 0;
		DWORD dwRead;
		if(!ReadFile(hReadHandle, buffer, dwAvailable, &dwRead, NULL))
			goto error;

		char* pos = buffer;
		while(dwAvailable) {
			if(*pos == '\r') {
				*pos = 0;

				lua_pushstring(L, "print");
				lua_gettable(L, 1);
				luaL_checktype(L, -1, LUA_TFUNCTION);
				lua_pushstring(L, buffer);
				lua_call(L, 1, 0);

				if(pos[1] == '\n') { dwAvailable--; pos++; }
				buffer = pos + 1;
			}
			dwAvailable--; pos++;
		}
		if(buffer != pos) {
			lua_pushstring(L, "print");
			lua_gettable(L, 1);
			luaL_checktype(L, -1, LUA_TFUNCTION);
			lua_pushstring(L, buffer);
			lua_call(L, 1, 0);
		}

		//if(!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, dwRead, NULL, NULL))
		//_setmode(_fileno(stdout), _O_BINARY);
		//if(!fwrite(buffer, 1, dwRead, stdout))
		//	luaL_error(L, "error writing to stdout");
		//_setmode(_fileno(stdout), _O_TEXT);
	}	
	return 0;

error:
	if(GetLastError() != ERROR_BROKEN_PIPE)
		luaL_error(L, "error reading from pipe");

	// this is the normal exit path; close our handles and exit
	lua_pushstring(L, "procid");
	lua_gettable(L, 1);
	HANDLE hProcess = (HANDLE)(DWORD)luaL_checknumber(L, -1);
	lua_pop(L,1);

  DWORD result;
  GetExitCodeProcess(hProcess, &result);

	lua_pushstring(L, "exit_code");
	lua_pushnumber(L, result);
	lua_settable(L, 1);

	CloseHandle(hReadHandle);
	CloseHandle(hProcess);
	return 0;
}

static const luaL_Reg make_pathlib[] = {
  {"canonicalize", make_path_canonicalize},		// make.path.canonicalize
	{"add_slash", make_path_add_slash},					// make.path.add_slash
	{"remove_slash", make_path_remove_slash},		// make.path.remove_slash
	{"is_relative", make_path_is_relative},			// make.path.is_relative
	{"quote",make_path_quote},									// make.path.quote
	{"unquote",make_path_unquote},							// make.path.unquote
	{"add_ext",make_path_add_extension},				// make.path.add_ext
	{"get_ext",make_path_get_extension},				// make.path.get_ext
	{"change_ext",make_path_change_extension},	// make.path.change_ext
	{"remove_ext",make_path_remove_extension},	// make.path.remove_ext
	{"get_name",make_path_get_name},						// make.path.get_name
	{"get_dir",make_path_get_dir},							// make.path.get_dir
	{"combine",make_path_combine},							// make.path.combine
	{"common",make_path_common},								// make.path.common
	{"short",make_path_short},									// make.path.short
	{"long",make_path_long},										// make.path.long
	{"full",make_path_full},										// make.path.full
	{"glob",make_path_glob},										// make.path.glob
	{"where",make_path_where},									// make.path.where
	{"to_os",make_path_to_os},									// make.path.to_os
	{"from_os",make_path_from_os},							// make.path.from_os
  {NULL, NULL}
};
static const luaL_Reg make_filelib[] = {
	{"exists", make_path_exists},								// make.file.exists
	{"temp",make_path_tempfile},								// make.file.temp
	{"copy",make_path_copy},										// make.file.copy
	{"touch",make_path_touch},									// make.file.touch
	{"delete",make_path_delete},								// make.file.delete
	{"size",make_path_getsize},									// make.file.size
	{"time",make_path_filetime},								// make.file.time
	{"md5",make_file_md5},											// make.file.md5
  {NULL, NULL}
};
static const luaL_Reg make_dirlib[] = {
	{"is_dir", make_path_is_dir},								// make.dir.is_dir
	{"is_empty", make_path_is_dir_empty},				// make.dir.is_empty
	{"temp",make_path_tempdir},									// make.dir.temp
	{"cd",make_path_cd},												// make.dir.cd
	{"md",make_path_md},												// make.dir.md
	{"rd",make_path_rd},												// make.dir.rd
  {NULL, NULL}
};
static const luaL_Reg make_proclib[] = {
	{"spawn", make_proc_spawn},									// make.proc.spawn
	{"flushio", make_proc_flushio},							// make.proc.flushio
  {NULL, NULL}
};
static const luaL_Reg make_rootlib[] = {
	{"now", make_now},													// make.now
	{"md5", make_md5},													// make.md5
  {NULL, NULL}
};


/*
** Open make library
*/
LUALIB_API int luaopen_make (lua_State *L) {
	
	luaL_newmetatable(L,"PROC*");


	// FILETIME is a 64-bit int, representing 100-nanosecond increments since 1/1/1601.
	// We want our numbers to fit nicely into a double without losing any precision, so
	// we subtract out the start-time.
	SYSTEMTIME st;
	GetSystemTime(&st);
	static FILETIME ft2;
	SystemTimeToFileTime(&st, &ft2);
	start_time = (__int64)ft2.dwLowDateTime | (((__int64)ft2.dwHighDateTime)<<32);

	// set up environment
	lua_newtable(L);
	lua_pushstring(L, "env");
	lua_newtable(L);
	char keyb[32768], valueb[32768];
	#undef GetEnvironmentStrings
	const char* env = GetEnvironmentStrings();
	while(*env) {
		char* key = keyb;
		while(*env != '=') { *key++ = toupper(*env++); } 
		env++; *key = 0;
		char* value = valueb;
		while(*env) { *value++ = *env++; }
		env++; *value = 0;
		if(strlen(keyb)) {
			lua_pushstring(L, keyb);
			lua_pushstring(L, valueb);
			lua_settable(L, -3); // make.env[key] = value
		}
	}
	lua_settable(L, -3); // make.env
	lua_setfield(L, LUA_GLOBALSINDEX, LUA_MAKELIBNAME);

  //luaL_register(L, LUA_MAKELIBNAME, makelib);
	luaL_register(L, LUA_MAKELIBNAME ".path", make_pathlib);
	luaL_register(L, LUA_MAKELIBNAME ".file", make_filelib);
	luaL_register(L, LUA_MAKELIBNAME ".dir", make_dirlib);
	luaL_register(L, LUA_MAKELIBNAME ".proc", make_proclib);
	luaL_register(L, LUA_MAKELIBNAME, make_rootlib);

	//Sleep(30000);
  return 1;
}

