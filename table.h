/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.15 1996/01/26 18:03:19 roberto Exp roberto $
*/

#ifndef table_h
#define table_h

#include "tree.h"
#include "opcode.h"

typedef struct
{
 Object  object;
 TreeNode *varname;
} Symbol;


extern Symbol *lua_table;
extern TaggedString **lua_constant;

void  lua_initconstant (void);
Word  luaI_findsymbolbyname (char *name);
Word  luaI_findsymbol      (TreeNode *t);
Word  luaI_findconstant    (TreeNode *t);
Word  luaI_findconstantbyname (char *name);
int   lua_markobject      (Object *o);
Long luaI_collectgarbage (void);
void  lua_pack            (void);


#endif
