/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.25 1997/05/26 14:42:36 roberto Exp $
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
int luaI_globaldefined (char *name);
void luaI_nextvar (void);
TaggedString *luaI_createfixedstring  (char *str);
int   lua_markobject      (TObject *o);
int luaI_ismarked (TObject *o);
void  lua_pack            (void);


#endif
