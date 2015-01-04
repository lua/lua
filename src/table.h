/*
** table.c
** Module to control static tables
** TeCGraf - PUC-Rio
** 11 May 93
*/

#ifndef table_h
#define table_h

extern Symbol *lua_table;
extern Word    lua_ntable;

extern char  **lua_constant;
extern Word    lua_nconstant;

extern char  **lua_string;
extern Word    lua_nstring;

extern Hash  **lua_array;
extern Word    lua_narray;

extern char   *lua_file[];
extern int     lua_nfile;

#define lua_markstring(s)	(*((s)-1))


int   lua_findsymbol           (char *s);
int   lua_findenclosedconstant (char *s);
int   lua_findconstant         (char *s);
void  lua_markobject           (Object *o);
char *lua_createstring         (char *s);
void *lua_createarray          (void *a);
int   lua_addfile              (char *fn);
char *lua_filename             (void);
void  lua_nextvar              (void);

#endif
