/*
** hash.h
** hash manager for lua
** Luiz Henrique de Figueiredo - 17 Aug 90
** $Id: hash.h,v 2.9 1996/02/07 14:13:17 roberto Exp roberto $
*/

#ifndef hash_h
#define hash_h

#include "types.h"
#include "opcode.h"

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


int      lua_equalObj (Object *t1, Object *t2);
Word     luaI_redimension (Word nhash);
Hash    *lua_createarray (Word nhash);
void     lua_hashmark (Hash *h);
Long     lua_hashcollector (void);
Object  *lua_hashget (Hash *t, Object *ref);
Object 	*lua_hashdefine (Hash *t, Object *ref);
void     lua_next (void);

#endif
