/*
** hash.h
** hash manager for lua
** Luiz Henrique de Figueiredo - 17 Aug 90
** $Id: hash.h,v 2.6 1994/11/17 13:58:57 roberto Stab roberto $
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
 Word          nhash;
 Word           nuse;
 Node          *node;
} Hash;


Bool     lua_equalObj (Object *t1, Object *t2);
Hash    *lua_createarray (Word nhash);
void     lua_hashmark (Hash *h);
Word     lua_hashcollector (void);
Object  *lua_hashget (Hash *t, Object *ref);
Object 	*lua_hashdefine (Hash *t, Object *ref);
void     lua_next (void);

#endif
