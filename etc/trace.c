/*
* trace.c
* a simple execution tracer for Lua
*/

#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "lualib.h"
#include "luadebug.h"

lua_State *lua_state = NULL;
#define	L	lua_state	/* lazy! */

static FILE* LOG;		/* output file */
static int I=0;			/* indentation level */

static void linehook(lua_State *L, lua_Debug *ar)
{
 fprintf(LOG,"%*sdo_line(%d)\t-- %d\n",I,"",ar->currentline,I);
}

static void callhook(lua_State *L, lua_Debug *ar)
{
 fprintf(LOG,"%*sdo_%s\t-- %p %d\n",I,"",ar->event,ar->_func,I);
 if (*ar->event=='r') --I; else ++I;
}

void start_trace(FILE* logfile)
{
 lua_setlinehook(L,linehook);
 lua_setcallhook(L,callhook);
 LOG=logfile;
}

void stop_trace(void)
{
 lua_setlinehook(L,NULL);
 lua_setcallhook(L,NULL);
 fclose(LOG);
}

int main(void)
{
 int rc;
 L=lua_open(0);
 lua_baselibopen(L);
 lua_iolibopen(L);
 lua_strlibopen(L);
 lua_mathlibopen(L);
 lua_dblibopen(L);
 start_trace(stderr);
 rc=lua_dofile(L,0);
 stop_trace();
 return rc;
}
