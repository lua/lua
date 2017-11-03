/*
** $Id: lapi.h,v 2.10 2017/11/01 18:20:48 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "llimits.h"
#include "lstate.h"

#define api_incr_top(L)   {L->top++; api_check(L, L->top <= functop(L->func), \
				"stack overflow");}

#define adjustresults(L,nres) \
    { if ((nres) == LUA_MULTRET && functop(L->func) < L->top) \
      setfunctop(L->func, L->top); }

#define api_checknelems(L,n)	api_check(L, (n) < (L->top - L->func), \
				  "not enough elements in the stack")


#endif
