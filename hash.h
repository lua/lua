/*
** hash.h
** hash manager for lua
** Luiz Henrique de Figueiredo - 17 Aug 90
** $Id: hash.h,v 2.2 1994/08/05 19:25:09 celes Exp celes $
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
 char           mark;
 unsigned int   nhash;
 unsigned int   nuse;
 Node          *node;
} Hash;


Hash    *lua_createarray (int nhash);
void     lua_hashmark (Hash *h);
void     lua_hashcollector (void);
Object  *lua_hashget (Hash *t, Object *ref);
Object 	*lua_hashdefine (Hash *t, Object *ref);
void     lua_next (void);

#endif
