/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.5 1994/11/14 21:40:14 roberto Exp roberto $
*/

#ifndef table_h
#define table_h

#include "tree.h"

extern Symbol *lua_table;
extern char  **lua_constant;

extern char   *lua_file[];
extern int     lua_nfile;

extern Word    lua_block;
extern Word    lua_nentity;
extern Word    lua_recovered;


void  lua_initconstant (void);
int   luaI_findsymbolbyname (char *name);
int   luaI_findsymbol      (TreeNode *t);
int   luaI_findconstant    (TreeNode *t);
void  lua_travsymbol      (void (*fn)(Object *));
void  lua_markobject      (Object *o);
void  lua_pack            (void);
char *lua_createstring    (char *s);
char *lua_addfile         (char *fn);
int   lua_delfile 	  (void);
char *lua_filename        (void);

#endif
