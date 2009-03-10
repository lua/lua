/*
** $Id: lapi.h,v 2.3 2006/07/11 15:53:29 roberto Exp roberto $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "llimits.h"
#include "lstate.h"

#define api_incr_top(L)   {L->top++; api_check(L, L->top <= L->ci->top);}

#define adjustresults(L,nres) \
    { if ((nres) == LUA_MULTRET && L->top >= L->ci->top) L->ci->top = L->top; }


#endif
