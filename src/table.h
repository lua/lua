/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.10 1994/12/20 21:20:36 roberto Exp $
*/

#ifndef table_h
#define table_h

#include "tree.h"
#include "opcode.h"

extern Symbol *lua_table;
extern TaggedString **lua_constant;

extern char   *lua_file[];
extern int     lua_nfile;


void  lua_initconstant (void);
Word  luaI_findsymbolbyname (char *name);
Word  luaI_findsymbol      (TreeNode *t);
Word  luaI_findconstant    (TreeNode *t);
void  lua_travsymbol      (void (*fn)(Object *));
void  lua_markobject      (Object *o);
void  lua_pack            (void);
char *lua_addfile         (char *fn);
int   lua_delfile 	  (void);
char *lua_filename        (void);

#endif
