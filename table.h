/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.14 1996/01/22 14:15:13 roberto Exp roberto $
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

extern char   *lua_file[];
extern int     lua_nfile;


void  lua_initconstant (void);
Word  luaI_findsymbolbyname (char *name);
Word  luaI_findsymbol      (TreeNode *t);
Word  luaI_findconstant    (TreeNode *t);
Word  luaI_findconstantbyname (char *name);
int   lua_markobject      (Object *o);
Long luaI_collectgarbage (void);
void  lua_pack            (void);


#endif
