/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.22 1997/02/26 17:38:41 roberto Unstable roberto $
*/

#ifndef table_h
#define table_h

#include "tree.h"
#include "opcode.h"

typedef struct
{
 TObject  object;
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
void luaI_nextvar (void);
TaggedString *luaI_createfixedstring  (char *str);
int   lua_markobject      (TObject *o);
int luaI_ismarked (TObject *o);
Long luaI_collectgarbage (void);
void  lua_pack            (void);


#endif
