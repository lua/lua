/*
** hash.h
** hash manager for lua
** $Id: hash.h,v 2.14 1997/03/19 19:41:10 roberto Exp roberto $
*/

#ifndef hash_h
#define hash_h

#include "types.h"
#include "opcode.h"

typedef struct node {
 TObject ref;
 TObject val;
} Node;

typedef struct Hash {
  struct Hash *next;
  Node *node;
  int nhash;
  int nuse;
  int htag;
  char mark;
} Hash;


int      lua_equalObj (TObject *t1, TObject *t2);
int      luaI_redimension (int nhash);
Hash    *lua_createarray (int nhash);
void     lua_hashmark (Hash *h);
Long     lua_hashcollector (void);
void	 luaI_hashcallIM (void);
TObject  *lua_hashget (Hash *t, TObject *ref);
TObject 	*lua_hashdefine (Hash *t, TObject *ref);
void     lua_next (void);

#endif
