/*
** hash.h
** hash manager for lua
** Luiz Henrique de Figueiredo - 17 Aug 90
** $Id: hash.h,v 2.8 1995/01/12 14:19:04 roberto Exp $
*/

#ifndef hash_h
#define hash_h

#include "types.h"

typedef struct node
{
 Object ref;
 Object val;
} Node;

typedef struct Hash
{
 struct Hash   *next;
 char           mark;
 Word          nhash;
 Word           nuse;
 Node          *node;
} Hash;


Bool     lua_equalObj (Object *t1, Object *t2);
Hash    *lua_createarray (Word nhash);
void     lua_hashmark (Hash *h);
Long     lua_hashcollector (void);
Object  *lua_hashget (Hash *t, Object *ref);
Object 	*lua_hashdefine (Hash *t, Object *ref);
void     lua_next (void);

#endif
