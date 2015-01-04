/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.21 1996/04/22 18:00:37 roberto Exp $
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
extern Word lua_ntable;
extern TaggedString **lua_constant;
extern Word lua_nconstant;

void luaI_initsymbol (void);
void  luaI_initconstant (void);
Word  luaI_findsymbolbyname (char *name);
Word  luaI_findsymbol      (TaggedString *t);
Word  luaI_findconstant    (TaggedString *t);
Word  luaI_findconstantbyname (char *name);
TaggedString *luaI_createfixedstring  (char *str);
int   lua_markobject      (Object *o);
int luaI_ismarked (Object *o);
Long luaI_collectgarbage (void);
void  lua_pack            (void);


#endif
