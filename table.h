/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.17 1996/02/12 18:32:40 roberto Exp roberto $
*/

#ifndef table_h
#define table_h

#include "tree.h"
#include "opcode.h"

typedef struct
{
 Object  object;
 TaggedString *varname;
} Symbol;


extern Symbol *lua_table;
extern TaggedString **lua_constant;

void luaI_initsymbol (void);
void  luaI_initconstant (void);
Word  luaI_findsymbolbyname (char *name);
Word  luaI_findsymbol      (TaggedString *t);
Word  luaI_findconstant    (TaggedString *t);
Word  luaI_findconstantbyname (char *name);
TaggedString *lua_constcreate  (char *str);
int   lua_markobject      (Object *o);
Long luaI_collectgarbage (void);
void  lua_pack            (void);


#endif
