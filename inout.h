/*
** $Id: inout.h,v 1.2 1994/10/11 14:38:17 celes Exp roberto $
*/


#ifndef inout_h
#define inout_h

extern int lua_linenumber;
extern int lua_debug;
extern int lua_debugline;

int  lua_openfile     (char *fn);
void lua_closefile    (void);
int  lua_openstring   (char *s);
void lua_closestring  (void);
int  lua_pushfunction (char *file, int function);
void lua_popfunction  (void);
void lua_reportbug    (char *s);

void    lua_internaldofile (void);
void    lua_internaldostring (void);
void    lua_print      (void);
void    lua_type       (void);

#endif
