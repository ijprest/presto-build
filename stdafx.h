/*SDOC***********************************************************************

	Module:				stdafx.h

	Copyright (C) 2009-2014 Ian Prest
	http://ijprest.github.com/presto-build/license.html

	Description:	Precompiled header file for MSVC

***********************************************************************EDOC*/
#include <windows.h>
#include <shlwapi.h>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

extern "C" {
#define LUA_LIB
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <vector>

// RSA Data Security, Inc. MD5 Message-Digest Algorithm
extern "C" {
#include "md5.h"
}

// CreatePipe-like function that lets one or both handles be overlapped
extern "C" {
BOOL APIENTRY MyCreatePipeEx(OUT LPHANDLE lpReadPipe, 
														 OUT LPHANDLE lpWritePipe, 
														 IN LPSECURITY_ATTRIBUTES lpPipeAttributes,
														 IN DWORD nSize,
														 DWORD dwReadMode,
														 DWORD dwWriteMode);
}
