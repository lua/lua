/*
** hash.h
** hash manager for lua
** $Id: hash.h,v 2.13 1997/02/26 17:38:41 roberto Unstable roberto $
*/

#ifndef hash_h
#define hash_h

#include "types.h"
#include "opcode.h"

typedef struct node {
 Object ref;
 Object val;
} Node;

typedef struct Hash {
  struct Hash *next;
  Node *node;
  int nhash;
  int nuse;
  int htag;
  char mark;
} Hash;


int      lua_equalObj (Object *t1, Object *t2);
int      luaI_redimension (int nhash);
Hash    *lua_createarray (int nhash);
void     lua_hashmark (Hash *h);
Long     lua_hashcollector (void);
void	 luaI_hashcallIM (void);
Object  *lua_hashget (Hash *t, Object *ref);
Object 	*lua_hashdefine (Hash *t, Object *ref);
void     lua_next (void);

#endif
