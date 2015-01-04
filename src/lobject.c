/*
** $Id: lobject.c,v 1.13 1998/06/19 16:14:09 roberto Exp $
** Some generic functions over Lua objects
** See Copyright Notice in lua.h
*/

#include <stdlib.h>

#include "lobject.h"
#include "lua.h"


char *luaO_typenames[] = { /* ORDER LUA_T */
    "userdata", "number", "string", "table", "function", "function",
    "nil", "function", "mark", "mark", "mark", "line", NULL
};


TObject luaO_nilobject = {LUA_T_NIL, {NULL}};



/* hash dimensions values */
static long dimensions[] =
 {5L, 11L, 23L, 47L, 97L, 197L, 397L, 797L, 1597L, 3203L, 6421L,
  12853L, 25717L, 51437L, 102811L, 205619L, 411233L, 822433L,
  1644817L, 3289613L, 6579211L, 13158023L, MAX_INT};


int luaO_redimension (int oldsize)
{
  int i;
  for (i=0; dimensions[i]<MAX_INT; i++) {
    if (dimensions[i] > oldsize)
      return dimensions[i];
  }
  lua_error("table overflow");
  return 0;  /* to avoid warnings */
}


int luaO_equalObj (TObject *t1, TObject *t2)
{
  if (ttype(t1) != ttype(t2)) return 0;
  switch (ttype(t1)) {
    case LUA_T_NIL: return 1;
    case LUA_T_NUMBER: return nvalue(t1) == nvalue(t2);
    case LUA_T_STRING: case LUA_T_USERDATA: return svalue(t1) == svalue(t2);
    case LUA_T_ARRAY: return avalue(t1) == avalue(t2);
    case LUA_T_PROTO: return tfvalue(t1)  == tfvalue(t2);
    case LUA_T_CPROTO: return fvalue(t1)  == fvalue(t2);
    case LUA_T_CLOSURE: return t1->value.cl == t2->value.cl;
    default:
     LUA_INTERNALERROR("invalid type");
     return 0; /* UNREACHABLE */
  }
}


void luaO_insertlist (GCnode *root, GCnode *node)
{
  node->next = root->next;
  root->next = node;
  node->marked = 0;
}

#ifdef OLD_ANSI
void luaO_memup (void *dest, void *src, int size)
{
  char *d = dest;
  char *s = src;
  while (size--) d[size]=s[size];
}

void luaO_memdown (void *dest, void *src, int size)
{
  char *d = dest;
  char *s = src;
  int i;
  for (i=0; i<size; i++) d[i]=s[i];
}
#endif

