/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.25 1997/05/26 14:42:36 roberto Exp roberto $
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

void luaI_initsymbol (void);
void  luaI_initconstant (void);
Word  luaI_findsymbolbyname (char *name);
Word  luaI_findsymbol      (TaggedString *t);
int luaI_globaldefined (char *name);
void luaI_nextvar (void);
TaggedString *luaI_createtempstring (char *name);
void luaI_releasestring (TaggedString *t);
void luaI_fixstring (TaggedString *t);
int   lua_markobject      (TObject *o);
int luaI_ismarked (TObject *o);
void  lua_pack            (void);


#endif
