/*
** hash.h
** hash manager for lua
** Luiz Henrique de Figueiredo - 17 Aug 90
** $Id: hash.h,v 2.5 1994/11/14 18:41:15 roberto Exp roberto $
*/

#ifndef hash_h
#define hash_h

typedef struct node
{
 Object ref;
 Object val;
} Node;

typedef struct Hash
{
 struct Hash   *next;
 char           mark;
 unsigned int   nhash;
 unsigned int   nuse;
 Node          *node;
} Hash;


int      lua_equalObj (Object *t1, Object *t2);
Hash    *lua_createarray (int nhash);
void     lua_hashmark (Hash *h);
int      lua_hashcollector (void);
Object  *lua_hashget (Hash *t, Object *ref);
Object 	*lua_hashdefine (Hash *t, Object *ref);
void     lua_next (void);

#endif
