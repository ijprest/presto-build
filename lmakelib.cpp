/*SDOC***********************************************************************

	Module:				lmakelib.cpp

	Copyright (C) 2009 Ian Prest
	http://ijprest.github.com/presto-build/license.html

	Description:	Presto-specific library of functions that are exposed
								to the running Lua script.

***********************************************************************EDOC*/
#include "stdafx.h"
#include "lmakelib.h"

//***************************************************************************
//***************************  helper functions  ****************************
//***************************************************************************

/*SDOC***********************************************************************

	Name:			lua_getpath

	Action:		Helper function that converts a presto-style path (forward 
						slashes) to a native path (e.g., backslashes on Win32).

	Params:		(same as luaL_checklstring)

	Returns:	converted path

	Comments:	The function must call the lua_path_init() macro prior to 
						using lua_getpath()

***********************************************************************EDOC*/
inline char* _lua_topath(char* out, const char* in, size_t* len) {
	memcpy(out, in, (*len)+1);
	for (size_t i=0; i<(*len); i++)
		if(out[i] == '/') out[i] = '\\';
	return out;
}
#define lua_topath(in, len) (_lua_topath((char*)_alloca((*(len))+1), in, len))

#define lua_path_init()	const char* lua_getpath_s = 0;
#define lua_getpath(L, stack_pos, len) \
	((lua_getpath_s = luaL_checklstring(L, stack_pos, len)), lua_topath( lua_getpath_s, len ))


/*SDOC***********************************************************************

	Name:			lua_pushpath

	Action:		Helper function that pushes a native path onto the Lua stack,
						converting it to a presto-style path (forward slashes) in
						the process.

	Params:		(same as lua_pushstring)

***********************************************************************EDOC*/
inline void lua_pushpath(lua_State *L, const char* path) {
  luaL_Buffer b;
	luaL_buffinit(L, &b);
	size_t len = strlen(path);
	for (size_t i=0; i<len; i++)
		luaL_addchar(&b, (path[i] == '\\') ? '/' : (unsigned char)(path[i]));
  luaL_pushresult(&b);
}


/*SDOC***********************************************************************

	Name:			lua_pushhex

	Action:		Helper function to convert a binary string to a hex 
						representation.

	Params:		(same as lua_pushstring)

***********************************************************************EDOC*/
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


//***************************************************************************
//**************************  make.path functions  **************************
//***************************************************************************

/*SDOC***********************************************************************

	Name:			make_path_to_os
						make_path_from_os

	Action:		Converts a path to/from an OS-specific path, from/to a 
						presto-style path (forward slashes)

	Params:		[1] string - input pathname

	Returns:	[1] string - output pathname

	Comments:	This is normally handled for you by all the make.path.* 
						functions; however, these two routines will be necessary when
						constructing command-strings to pass to make.run().

***********************************************************************EDOC*/
static int make_path_to_os(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);	// convert to backslashes
	lua_pushstring(L,path_in);
	return 1;
}

static int make_path_from_os(lua_State *L) {
	size_t l; 
	const char* path_in = luaL_checklstring(L, 1, &l);
	lua_pushpath(L, path_in);	// convert to regular slashes
	return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_short
						make_path_long

	Action:		Converts a path from standard format to short format (8.3 
						filenames), or vice-versa.

	Params:		[1] string - input pathname

	Returns:	[1] string - output pathname

***********************************************************************EDOC*/
static int make_path_short(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	GetShortPathNameA(path_in, path_out, MAX_PATH);
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_long(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	GetLongPathNameA(path_in, path_out, MAX_PATH);
	lua_pushpath(L, path_out);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_full

	Action:		Returns the full path name for the specified file.

	Comments:	Filename is based on the current working directory.

	Params:		[1] string - input pathname

	Returns:	[1] string - output pathname

***********************************************************************EDOC*/
static int make_path_full(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	GetFullPathNameA(path_in, MAX_PATH, path_out, NULL);
	lua_pushpath(L, path_out);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_canonicalize

	Action:		Canonicalizes a path (removes "." and "..")

	Params:		[1] string - input pathname

	Returns:	[1] string - output pathname

***********************************************************************EDOC*/
static int make_path_canonicalize(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	PathCanonicalizeA(path_out, path_in);
	lua_pushpath(L, path_out);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_add_slash
						make_path_remove_slash

	Action:		Add or remove a trailing slash.

	Params:		[1] string - input pathname

	Returns:	[1] string - output pathname

***********************************************************************EDOC*/
static int make_path_add_slash(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathAddBackslashA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_remove_slash(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathRemoveBackslashA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_remove_ext

	Action:		Removes the extension from a filename.

	Params:		[1] string - input pathname (e.g., "foo.cpp")

	Returns:	[1] string - output pathname (e.g. "foo")

***********************************************************************EDOC*/
static int make_path_remove_ext(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathRemoveExtensionA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_quote
						make_path_unquote

	Action:		Adds or removes quotes aroung a pathname if the filename 
						contains any spaces.

	Params:		[1] string - input pathname

	Returns:	[1] string - output pathname

***********************************************************************EDOC*/
static int make_path_quote(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathQuoteSpacesA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}

static int make_path_unquote(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path_in);
	PathUnquoteSpacesA(path_out); 
	lua_pushpath(L, path_out);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_get_ext

	Action:		Returns only the extension of the file.

	Params:		[1] string - input pathname (e.g., "foo.cpp")

	Returns:	[1] string - output pathname (e.g., ".cpp")

***********************************************************************EDOC*/
static int make_path_get_ext(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	const char* path_out = PathFindExtensionA(path_in);
	lua_pushpath(L, path_out ? path_out : "");
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_get_name

	Action:		Returns only the filename portion of a path.

	Params:		[1] string - input pathname (e.g., "c:/path/to/foo.cpp")

	Returns:	[1] string - output pathname (e.g., "foo.cpp")

***********************************************************************EDOC*/
static int make_path_get_name(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	const char* path_out = PathFindFileNameA(path_in);
	lua_pushpath(L, path_out ? path_out : "");
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_get_dir

	Action:		Returns only the directory portion of a path.

	Params:		[1] string - input pathname (e.g., "c:/path/to/foo.cpp")

	Returns:	[1] string - output pathname (e.g., "c:/path/to/")

***********************************************************************EDOC*/
static int make_path_get_dir(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	char* path_out = PathFindFileNameA(path_in);
	*path_out = 0;
	lua_pushpath(L, path_in);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_is_relative

	Action:		Returns true if the specified path is relative (false if it
						is an absolute path).

	Params:		[1] string - input pathname

	Returns:	[1] bool

***********************************************************************EDOC*/
static int make_path_is_relative(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	BOOL result = PathIsRelativeA(path_in);
	lua_pushboolean(L, result ? 1 : 0);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_add_ext

	Action:		Adds an extension to the given path/filename.

	Params:		[1] string - input pathname (e.g., "foo")
						[2] string - extension (e.g., ".cpp")

	Returns:	[1] string - output pathname (e.g. "foo.cpp")

***********************************************************************EDOC*/
static int make_path_add_ext(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path1);
	PathAddExtensionA(path_out, path2);
	lua_pushpath(L, path_out);
	return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_change_ext

	Action:		Adds an extension to the given path/filename.

	Params:		[1] string - input pathname (e.g., "foo.obj")
						[2] string - extension (e.g., ".cpp")

	Returns:	[1] string - output pathname (e.g. "foo.cpp")

***********************************************************************EDOC*/
static int make_path_change_ext(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	char path_out[MAX_PATH] = {};
	strcpy_s(path_out, MAX_PATH, path1);
	PathRenameExtensionA(path_out, path2);
	lua_pushpath(L, path_out);
	return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_combine

	Action:		Combines two path fragments.

	Params:		[1] string - directory part (e.g., "c:/path")
						[2] string - file part (e.g., "to/foo.cpp")

	Returns:	[1] string - output pathname (e.g. "c:/path/to/foo.cpp")

	Comments:	Adds extra slashes as necessary.

***********************************************************************EDOC*/
static int make_path_combine(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	char path_out[MAX_PATH] = {};
	PathCombineA(path_out, path1, path2);
	lua_pushpath(L, path_out);
	return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_common

	Action:		Returns the greatest common prefix for two files.

	Params:		[1] string - file1 (e.g., "c:/path/to/foo.cpp")
						[2] string - file2 (e.g., "c:/path/two/bar.cpp")

	Returns:	[1] string - common prefix (e.g. "c:/path")

***********************************************************************EDOC*/
static int make_path_common(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	char path_out[MAX_PATH] = {};
	PathCommonPrefixA(path1, path2, path_out);
	lua_pushpath(L, path_out);
	return 1;
}


/*SDOC***********************************************************************

	Name:			make_path_glob

	Action:		Returns all the files that match a given filespec pattern.

	Params:		[1] string - filespec pattern (e.g., "c:/path/to/*.cpp")

	Returns:	[1] table - matching files (e.g., {"foo.cpp","bar.cpp"})

***********************************************************************EDOC*/
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


/*SDOC***********************************************************************

	Name:			make_path_where

	Action:		Searches a PATH-like variable for a given file.

	Params:		[1] string - file to find (e.g., "notepad.exe")
						[2] string - PATH to search (e.g., "c:/program files;c:/windows")
						 or: table - (e.g., {"c:/program files","c:/windows"})
				     or: nil   - search %PATH% from the environment

	Returns:	[1] string - matching file/path (e.g., "c:/windows/notepad.exe")
	           or: nil   - if not found

***********************************************************************EDOC*/
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
	if(path_out[0])
		lua_pushpath(L, path_out);
	else
		lua_pushnil(L);
	return 1;
}

static const luaL_Reg make_pathlib[] = {
  {"canonicalize", make_path_canonicalize},		// make.path.canonicalize
	{"add_slash", make_path_add_slash},					// make.path.add_slash
	{"remove_slash", make_path_remove_slash},		// make.path.remove_slash
	{"is_relative", make_path_is_relative},			// make.path.is_relative
	{"quote",make_path_quote},									// make.path.quote
	{"unquote",make_path_unquote},							// make.path.unquote
	{"add_ext",make_path_add_ext},							// make.path.add_ext
	{"get_ext",make_path_get_ext},							// make.path.get_ext
	{"change_ext",make_path_change_ext},				// make.path.change_ext
	{"remove_ext",make_path_remove_ext},				// make.path.remove_ext
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


//***************************************************************************
//**************************  make.file functions  **************************
//***************************************************************************

/*SDOC***********************************************************************

	Name:			make_file_exists

	Action:		Returns true if the specified file exists.

	Params:		[1] string - input pathname

	Returns:	[1] bool

***********************************************************************EDOC*/
static int make_file_exists(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	BOOL result = PathFileExistsA(path_in);
	lua_pushboolean(L, result ? 1 : 0);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_file_temp

	Action:		Returns the name of a unique temporary file.

	Returns:	[1] string - temporary filename

	Comments:	An empty file with the returned name is created; the caller
						should ensure it gets deleted.

***********************************************************************EDOC*/
static int make_file_temp(lua_State *L) {
	char dir[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, dir);
	char path_out[MAX_PATH] = {};
	GetTempFileNameA(dir, "pre", 0, path_out);
	lua_pushpath(L, path_out);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_file_touch

	Action:		"Touches" the specified file

	Params:		[1] string - file to touch

	Comments:	If the file does not exist, it is created.  If the file
						already exists, it's last-modified date is updated.

***********************************************************************EDOC*/
static int make_file_touch(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	HANDLE hFile = CreateFileA(path_in, FILE_WRITE_ATTRIBUTES, 0, NULL, 
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE)
		luaL_error(L, "error touching file " LUA_QS, path_in);
	SYSTEMTIME st = {};
	FILETIME ft = {};
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	SetFileTime(hFile, &ft, &ft, &ft);
	CloseHandle(hFile);
  return 0;
}


/*SDOC***********************************************************************

	Name:			make_file_copy

	Action:		Copy a file

	Params:		[1] string - source
						[2] string - destination

***********************************************************************EDOC*/
static int make_file_copy(lua_State *L) {
	lua_path_init();
  size_t l1; char* path1 = lua_getpath(L, 1, &l1);
  size_t l2; char* path2 = lua_getpath(L, 2, &l2);
	if(!CopyFileA(path1, path2, FALSE))
		luaL_error(L, "error copying file " LUA_QS " to " LUA_QS, path1, path2);
	return 0;
}


/*SDOC***********************************************************************

	Name:			make_file_delete

	Action:		Delete a file

	Params:		[1] string - file to delete

	Comments:	Fails silently if the file doesn't exist.

***********************************************************************EDOC*/
static int make_file_delete(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	if(!DeleteFileA(path_in) && GetLastError() != ERROR_FILE_NOT_FOUND)
		luaL_error(L, "error deleting file " LUA_QS, path_in);
	return 0;
}


/*SDOC***********************************************************************

	Name:			make_file_size

	Action:		Returns the size of a file

	Params:		[1] string - filename

	Returns:	[1] number - length of the file
						 or: nil - if the file doesn't exist

***********************************************************************EDOC*/
static int make_file_size(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	HANDLE hFile = CreateFileA(path_in, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER size;
		GetFileSizeEx(hFile, &size);
		CloseHandle(hFile);
		lua_pushnumber(L,(double)size.QuadPart);
	} else {
		lua_pushnil(L);
	}
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_file_time

	Action:		Returns the timestamp of a file

	Params:		[1] string - filename

	Returns:	[1] number - timestamp
						 or: nil - if the file doesn't exist

	Comments:	Timestamps in presto are numbers (in seconds), offset by the 
						time that presto was launched.  Positive numbers represent
						times *after* presto was launched.  Negative numbers represent
						times *before* presto was launched.

						On Windows systems, no attempt is made to overcome FAT's 
						limitations (2-second resolution, no adjustment for timezones,
						etc.).  You really should be running NTFS.

***********************************************************************EDOC*/
static __int64 start_time = 0;
static int make_file_time(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	HANDLE hFile = CreateFileA(path_in, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile != INVALID_HANDLE_VALUE) {
		FILETIME ft;
		GetFileTime(hFile, NULL, NULL, &ft);
		CloseHandle(hFile);
		__int64 time = ((__int64)ft.dwLowDateTime | (((__int64)ft.dwHighDateTime)<<32)) - start_time;
		lua_pushnumber(L,(double)time * 1.0e-7);
	} else {
		lua_pushnil(L);
	}
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_file_md5

	Action:		Computes the MD5 hash for a file.

	Params:		[1] string - filename

	Returns:	[1] string - MD5 hash (hex-coded)

	Comments:	This function should be used with caution.  Presto is 
						single-threaded, so calling this function on a large file 
						can interfere with (or delay) the execution of other jobs.

***********************************************************************EDOC*/
static int make_file_md5(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);

	// compute the md5 hash
	HANDLE hFile = CreateFileA(path_in, GENERIC_READ, FILE_SHARE_READ, NULL, 
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( hFile == INVALID_HANDLE_VALUE)
		luaL_error(L, "error opening file "LUA_QS" for reading", path_in);
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

static const luaL_Reg make_filelib[] = {
	{"exists", make_file_exists},								// make.file.exists
	{"temp",make_file_temp},										// make.file.temp
	{"copy",make_file_copy},										// make.file.copy
	{"touch",make_file_touch},									// make.file.touch
	{"delete",make_file_delete},								// make.file.delete
	{"size",make_file_size},										// make.file.size
	{"time",make_file_time},										// make.file.time
	{"md5",make_file_md5},											// make.file.md5
  {NULL, NULL}
};


//***************************************************************************
//**************************  make.dir functions  ***************************
//***************************************************************************

/*SDOC***********************************************************************

	Name:			make_dir_is_dir

	Action:		Returns true if the specified path refers to a directory.

	Params:		[1] string - input pathname

	Returns:	[1] bool

***********************************************************************EDOC*/
static int make_dir_is_dir(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	BOOL result = PathIsDirectoryA(path_in);
	lua_pushboolean(L, result ? 1 : 0);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_dir_is_empty

	Action:		Returns true if the specified directory is empty.

	Params:		[1] string - input pathname

	Returns:	[1] bool

***********************************************************************EDOC*/
static int make_dir_is_empty(lua_State *L) {
	lua_path_init();
  size_t l; char* path_in = lua_getpath(L, 1, &l);
	BOOL result = PathIsDirectoryEmptyA(path_in);
	lua_pushboolean(L, result ? 1 : 0);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_dir_temp

	Action:		Returns the user-defined temporary directory (typically %TEMP%)

	Returns:	[1] string - tempdir

***********************************************************************EDOC*/
static int make_dir_temp(lua_State *L) {
	char path_out[MAX_PATH] = {};
	GetTempPathA(MAX_PATH, path_out);
	lua_pushpath(L, path_out);
  return 1;
}


/*SDOC***********************************************************************

	Name:			make_dir_cd

	Action:		Change/get the current working directory

	Params:		[1] string - new working directory
						 or: nil - don't change directory; just return cwd

	Returns:	[1] string - current working directory

***********************************************************************EDOC*/
int make_dir_cd(lua_State *L) {
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


/*SDOC***********************************************************************

	Name:			make_dir_md

	Action:		Create a new directory.

	Params:		[1] string - new directory

***********************************************************************EDOC*/
static int make_dir_md(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);
	if(!CreateDirectoryA(path_in, NULL))
		luaL_error(L, "error creating directory " LUA_QS, path_in);
	return 0;
}


/*SDOC***********************************************************************

	Name:			make_dir_rd

	Action:		Remove a directory.

	Params:		[1] string - directory to remove

	Comments:	The directory must be empty.

***********************************************************************EDOC*/
static int make_dir_rd(lua_State *L) {
	lua_path_init();
	size_t l; char* path_in = lua_getpath(L, 1, &l);
	if(!RemoveDirectoryA(path_in))
		luaL_error(L, "error removing directory " LUA_QS, path_in);
	return 0;
}

static const luaL_Reg make_dirlib[] = {
	{"is_dir", make_dir_is_dir},								// make.dir.is_dir
	{"is_empty", make_dir_is_empty},						// make.dir.is_empty
	{"temp",make_dir_temp},											// make.dir.temp
	{"cd",make_dir_cd},													// make.dir.cd
	{"md",make_dir_md},													// make.dir.md
	{"rd",make_dir_rd},													// make.dir.rd
  {NULL, NULL}
};


//***************************************************************************
//**************************  make.proc functions  **************************
//***************************************************************************

// Process USERDATA type
struct process {
	OVERLAPPED olp;					// for overlapped I/O on the proc's stdout handle
	HANDLE hProcess;				// handle to the process
	HANDLE hOutputRead;			// handle to the proc's stdout
	DWORD dwExitCode;				// exit code
	bool bWaiting;					// are we waiting for overlapped I/O?
	char buffer[1024];			// overlapped I/O buffer
	char leftovers[1024];		// buffer of data that hasn't been printed yet
};


/*SDOC***********************************************************************

	Name:			make_proc_spawn

	Action:		Spawn a new process

	Params:		[1] string - command line (OS-specific)
						[2] table - environment table (e.g., {v1="value1",v2="value2"})
						 or: nil - to inherit presto's environment

	Returns:	[1] table - {data = --[[process USERDATA]]--}

	Comments:	The process will execute in the background.  All output is 
						buffered, and nothing will be dumped to the screen unless/until
						make.proc.flushio() is called.
	
						If an environment table is specified, it will completely 
						replace the original environment.  (If you need variables from
						the original environment, they can be copied from make.env.)

***********************************************************************EDOC*/
static int make_proc_spawn(lua_State *L) {
	// retrieve the command-line
	size_t l; 
	const char* path_in = luaL_checklstring(L, 1, &l);
	char* command_line = (char*)_alloca(l+1);
	strcpy(command_line, path_in);

	// retrieve the environment
	char* env = NULL;
	char env_buffer[32768];
	if(!lua_isnil(L, 2)) {
		luaL_checktype(L, 2, LUA_TTABLE);
		env = env_buffer;

		// loop over the table
		lua_pushnil(L);
		while(lua_next(L,2)) {
			// get the environment variable name
			const char* name = luaL_checkstring(L,-2);
			while(*name) *env++ = *name++;
			*env++ = '=';
			const char* value = luaL_checkstring(L,-1);
			while(*value) *env++ = *value++;
			*env++ = 0;
			lua_pop(L,1);
		}
		*env++ = 0; // double-null terminated
		env = env_buffer;
	}

	// Create the child output pipe (inheritable)
	HANDLE hOutputReadTmp, hOutputWrite;
	SECURITY_ATTRIBUTES sa = { sizeof(sa) };
	sa.bInheritHandle = TRUE;
	if(!MyCreatePipeEx(&hOutputReadTmp, &hOutputWrite, &sa, 0, FILE_FLAG_OVERLAPPED, 0)) // TODO: should buffer size be bigger?
		luaL_error(L, "error creating pipe");
	// Duplicate the pipe for stderr; this way, if the child 
	// process closes one of stdout/stderr, the other still works.
	HANDLE hErrorWrite;
  if(!DuplicateHandle(GetCurrentProcess(), hOutputWrite, GetCurrentProcess(), &hErrorWrite, 0, TRUE, DUPLICATE_SAME_ACCESS))
     luaL_error(L, "error duplicating pipe handle");
  // Duplicate the output read handle as uninheritable; otherwise
  // the child inherits it and a non-closeable handle to the pipe
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

	// Create a new USERDATA to hold the process information
	lua_newtable(L);
	process* proc = (process*)lua_newuserdata(L, sizeof(process));
	memset(proc, 0, sizeof(*proc));
	proc->hOutputRead = hOutputRead;
	proc->hProcess = pi.hProcess;
	proc->olp.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
	lua_setfield(L, -2, "data");
	return 1;
}


/*SDOC***********************************************************************

	Name:			make_proc_exitcode

	Action:		Returns the exit code for a process.

	Params:		[1] table - process table from make.proc.spawn()

	Returns:	[1] number - exit code
						 or: nil - if the process is still running.

***********************************************************************EDOC*/
static int make_proc_exitcode(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "data");
	process* p = (process*)lua_touserdata(L,-1);
	luaL_argcheck(L, p != NULL && lua_objlen(L,-1) == sizeof(process), 1, LUA_QL("process") " expected");
	if(p->hProcess != INVALID_HANDLE_VALUE)
		lua_pushnil(L);
	else
		lua_pushnumber(L, p->dwExitCode);
	return 1;
}


/*SDOC***********************************************************************

	Name:			make_proc_flushio

	Action:		Handles the output of a running process.

	Params:		[1] table - process table from make.proc.spawn()
											- "print" function must be specified

	Comments:	Any output the process has written to it's stdout handle is
						line-buffered, and output via the user-specified "print" 
						function.

***********************************************************************EDOC*/
static int make_proc_flushio_helper(lua_State *L, process* p, DWORD dwRead) {
	p->buffer[dwRead] = 0;
	char *pos = p->buffer, *line_start = p->buffer;
	while(dwRead && *pos) {
		if(*pos == '\r') {
			// hit the end of a line
			*pos = 0;

			// get the "print" function from the process table
			lua_getfield(L, 1, "print");
			luaL_checktype(L, -1, LUA_TFUNCTION);

			// build a string; use the remaining portion from the 
			// last read, plus everything up to the CR/LF this time...
			luaL_Buffer b;
			luaL_buffinit(L, &b);
			luaL_addstring(&b, p->leftovers);
			luaL_addstring(&b, line_start);
			luaL_pushresult(&b);
			lua_call(L, 1, 0);
			// erase the leftovers
			p->leftovers[0] = 0;

			// Increment our pointers
			if(pos[1] == '\n') { dwRead--; pos++; }
			line_start = pos + 1;
		}
		dwRead--; pos++;
	}

	// we have some leftover data; copy it to the process buffer
	// to be written next time
	if(line_start != pos)
		strcat(p->leftovers, line_start); // bug waiting to happen

	return 0;
}

static int make_proc_flushio(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "data");
	process* p = (process*)lua_touserdata(L,-1);
	luaL_argcheck(L, p != NULL && lua_objlen(L,-1) == sizeof(process), 1, LUA_QL("process") " expected");
	if(p->hProcess == INVALID_HANDLE_VALUE)
		return 0; // process is already done!

	DWORD dwRead;
	if(p->bWaiting) {
		if(!GetOverlappedResult(p->hOutputRead, &p->olp, &dwRead, FALSE)) {
			goto error;
		} else {
			// yay! we got data
			make_proc_flushio_helper(L, p, dwRead);
		}
		p->bWaiting = false;
	}

	// try to read some more data
	while(ReadFile(p->hOutputRead, p->buffer, sizeof(p->buffer)-1, &dwRead, &p->olp)) {
		// we got data right away
		make_proc_flushio_helper(L, p, dwRead);
		p->bWaiting = false;
	}

error:
	DWORD dwError = GetLastError();
	switch(dwError) {
		case ERROR_BROKEN_PIPE:	{
			// this is the normal exit path; close our handles and exit
			WaitForSingleObject(p->hProcess, INFINITE); // should already be done
			GetExitCodeProcess(p->hProcess, &p->dwExitCode);
			CloseHandle(p->hOutputRead);
			CloseHandle(p->hProcess);
			CloseHandle(p->olp.hEvent);
			p->hProcess = INVALID_HANDLE_VALUE;
			p->bWaiting = false;
			// write any leftover data
			if(p->leftovers[0]) {
				p->buffer[0] = '\r';
				p->buffer[1] = 0;
				make_proc_flushio_helper(L, p, 1);
			}
			break;
		}
		case ERROR_IO_INCOMPLETE:
		case ERROR_IO_PENDING: {
			// This is normal; there currently isn't any data to read, so 
			// we'll come back and try again later
			p->bWaiting = true;
			break;
		}
		default: {
			char str[MAX_PATH];
			FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,0,dwError,0,str,sizeof(str),NULL);
			luaL_error(L, "error reading from pipe: %d, %s", dwError, str);
			break;
		}
	}
	return 0;
}


/*SDOC***********************************************************************

	Name:			make_proc_wait

	Action:		Waits for a change in state in one or more processes.

	Params:		[1] table - array of process tables from make.proc.spawn()

	Comments:	Will not return until one of the specified processes has either
						finished, or has written more output to stdout.

***********************************************************************EDOC*/
static int make_proc_wait(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);

	// loop over the input array and accumulate handles
	std::vector<HANDLE> handles;
	int pos = 1;
	while(1) {
		lua_pushnumber(L,pos++);
		lua_gettable(L,1);
		if(lua_isnil(L,-1)) 
			break;
		luaL_checktype(L, -1, LUA_TTABLE);

		// Get the process data
		lua_getfield(L, -1, "data");
		process* p = (process*)lua_touserdata(L,-1);
		if(p == NULL || lua_objlen(L,-1) != sizeof(process))
			luaL_error(L, LUA_QL("process") " expected");

		handles.push_back(p->hProcess);
		if(p->olp.hEvent != INVALID_HANDLE_VALUE)
			handles.push_back(p->olp.hEvent);

		lua_pop(L,1);
	}

	// Wait for one of the handles to be signalled
	WaitForMultipleObjects(handles.size(), &handles[0], FALSE, INFINITE);
	return 0;
}

static const luaL_Reg make_proclib[] = {
	{"spawn", make_proc_spawn},									// make.proc.spawn
	{"flushio", make_proc_flushio},							// make.proc.flushio
	{"wait", make_proc_wait},										// make.proc.wait
	{"exit_code", make_proc_exitcode},					// make.proc.exit_code
  {NULL, NULL}
};


//***************************************************************************
//****************************  make functions  *****************************
//***************************************************************************

/*SDOC***********************************************************************

	Name:			make_now

	Action:		Returns the current time.

	Returns:	[1] number - current time

	Comments:	Timestamps in presto are numbers (in seconds), offset by the 
						time that presto was launched.  See also make_file_time().

***********************************************************************EDOC*/
static int make_now(lua_State *L) {
	SYSTEMTIME st;
	GetSystemTime(&st);
	FILETIME ft;
	SystemTimeToFileTime(&st,&ft);
	__int64 time = ((__int64)ft.dwLowDateTime | (((__int64)ft.dwHighDateTime)<<32)) - start_time;
	lua_pushnumber(L,(double)time * 1.0e-7);
	return 1;
}


/*SDOC***********************************************************************

	Name:			make_md5

	Action:		Computes the MD5 hash for a string.

	Params:		[1] string - string to hash

	Returns:	[1] string - MD5 hash (hex-coded)

	Comments:	This function should be used with caution.  Presto is 
						single-threaded, so calling this function on a large string.

***********************************************************************EDOC*/
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

static const luaL_Reg make_rootlib[] = {
	{"now", make_now},													// make.now
	{"md5", make_md5},													// make.md5
  {NULL, NULL}
};


//***************************************************************************
//****************************  misc functions  *****************************
//***************************************************************************

/*SDOC***********************************************************************

	Name:			luaopen_make

	Action:		Registers the make.* library functions.

	Comments:	This function should be used with caution.  Presto is 
						single-threaded, so calling this function on a large string.

***********************************************************************EDOC*/
LUALIB_API int luaopen_make(lua_State *L) {
	// FILETIME is a 64-bit int, representing 100-nanosecond increments since 1/1/1601.
	// We want our numbers to fit nicely into a double without losing any precision, so
	// we subtract out the start-time.
	SYSTEMTIME st;
	GetSystemTime(&st);
	static FILETIME ft2;
	SystemTimeToFileTime(&st, &ft2);
	start_time = (__int64)ft2.dwLowDateTime | (((__int64)ft2.dwHighDateTime)<<32);

	// Set up environment table (make.env)
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

	// Set up make.flags (empty table, all flags default false)
	lua_newtable(L);
	lua_setfield(L, -2, "flags");

	// Register the "make" table
	lua_setfield(L, LUA_GLOBALSINDEX, LUA_MAKELIBNAME);

	// Register all the make.* functions
	luaL_register(L, LUA_MAKELIBNAME ".path", make_pathlib);
	luaL_register(L, LUA_MAKELIBNAME ".file", make_filelib);
	luaL_register(L, LUA_MAKELIBNAME ".dir", make_dirlib);
	luaL_register(L, LUA_MAKELIBNAME ".proc", make_proclib);
	luaL_register(L, LUA_MAKELIBNAME, make_rootlib);
  return 1;
}
