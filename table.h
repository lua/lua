/*
** Module to control static tables
** TeCGraf - PUC-Rio
** $Id: table.h,v 2.3 1994/10/17 19:03:23 celes Exp roberto $
*/

#ifndef table_h
#define table_h

extern Symbol *lua_table;
extern char  **lua_constant;

extern char   *lua_file[];
extern int     lua_nfile;

extern Word    lua_block;
extern Word    lua_nentity;
extern Word    lua_recovered;


void  lua_initconstant (void);
int   lua_findsymbol      (char *s);
int   lua_findconstant    (char *s);
void  lua_travsymbol      (void (*fn)(Object *));
void  lua_markobject      (Object *o);
void  lua_pack            (void);
char *lua_createstring    (char *s);
char *lua_addfile         (char *fn);
int   lua_delfile 	  (void);
char *lua_filename        (void);
void  lua_nextvar         (void);

#endif
