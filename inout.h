/*
** $Id: $
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
int  lua_pushfunction (int file, int function);
void lua_popfunction  (void);
void lua_reportbug    (char *s);

#endif
