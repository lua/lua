/*
** $Id: $
** 'ctype' functions for Lua
** See Copyright Notice in lua.h
*/

#ifndef lctype_h
#define lctype_h


#include <limits.h>

#include "lua.h"


#define ALPHABIT	0
#define DIGITBIT	1
#define PRINTBIT	2
#define SPACEBIT	3


#define MASK(B)		(1 << (B))


#define lisalpha(x)	(lctypecode[x] & MASK(ALPHABIT))
#define lisalnum(x)	(lctypecode[x] & (MASK(ALPHABIT) | MASK(DIGITBIT)))
#define lisdigit(x)	(lctypecode[x] & MASK(DIGITBIT))
#define lisspace(x)	(lctypecode[x] & MASK(SPACEBIT))
#define lisprint(x)	(lctypecode[x] & MASK(PRINTBIT))

LUAI_DATA const char lctypecode[UCHAR_MAX + 1];

#endif

