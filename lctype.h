/*
** $Id: lctype.h,v 1.1 2009/02/19 17:18:25 roberto Exp roberto $
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


#define lisalpha(x)	(luai_ctype_[x] & MASK(ALPHABIT))
#define lisalnum(x)	(luai_ctype_[x] & (MASK(ALPHABIT) | MASK(DIGITBIT)))
#define lisdigit(x)	(luai_ctype_[x] & MASK(DIGITBIT))
#define lisspace(x)	(luai_ctype_[x] & MASK(SPACEBIT))
#define lisprint(x)	(luai_ctype_[x] & MASK(PRINTBIT))

LUAI_DATA const char luai_ctype_[UCHAR_MAX + 1];

#endif

