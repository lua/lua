/*
** $Id: lfunc.h,v 1.15 2001/02/23 17:17:25 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


#define luaF_isclosed(c, i)	(!((c)->u.l.isopen & (1 << (i))))
#define luaF_openentry(c, i)	((c)->u.l.isopen |= (1 << (i)))
#define luaF_closeentry(c, i)	((c)->u.l.isopen &= ~(1 << (i)))


Proto *luaF_newproto (lua_State *L);
Closure *luaF_newCclosure (lua_State *L, int nelems);
Closure *luaF_newLclosure (lua_State *L, int nelems);
void luaF_LConlist (lua_State *L, Closure *cl);
void luaF_close (lua_State *L, StkId level);
void luaF_freeproto (lua_State *L, Proto *f);
void luaF_freeclosure (lua_State *L, Closure *c);

const l_char *luaF_getlocalname (const Proto *func, int local_number, int pc);


#endif
