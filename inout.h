/*
** $Id: inout.h,v 1.5 1994/11/08 20:06:15 roberto Exp roberto $
*/


#ifndef inout_h
#define inout_h


extern int lua_linenumber;
extern int lua_debug;
extern int lua_debugline;

char *lua_openfile     (char *fn);
void lua_closefile    (void);
char *lua_openstring   (char *s);
void lua_closestring  (void);
void lua_pushfunction (char *file, int function);
void lua_popfunction  (void);
void lua_reportbug    (char *s);

void    lua_internaldofile (void);
void    lua_internaldostring (void);
void    lua_print      (void);
void    luaI_type       (void);
void    lua_obj2number (void);
void	luaI_error     (void);

#endif
