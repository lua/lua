/*
* trace.c -- a simple execution tracer for Lua
*/

#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

static FILE* LOG;		/* log file */
static int I=0;			/* indentation level */

static void hook(lua_State *L, lua_Debug *ar)
{
 const char* s="";
 switch (ar->event)
 {
  case LUA_HOOKTAILRET: ar->event=LUA_HOOKRET;
  case LUA_HOOKRET: s="return"; break;
  case LUA_HOOKCALL: s="call"; break;
  case LUA_HOOKLINE: s="line"; break;
  default: break;
 }
 fprintf(LOG,"[%d]\t%*s%s\t-- %d\n",I,I,"",s,ar->currentline);
 if (ar->event==LUA_HOOKCALL) ++I; else if (ar->event==LUA_HOOKRET) --I;
}

static void start_trace(lua_State *L, FILE* logfile)
{
 lua_sethook(L,hook,LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE, 0);
 LOG=logfile;
}

static void stop_trace(lua_State *L)
{
 lua_sethook(L,NULL,0,0);
 fclose(LOG);
}

int main(void)
{
 int rc;
 lua_State *L=lua_open();
 lua_baselibopen(L);
 lua_tablibopen(L);
 lua_iolibopen(L);
 lua_strlibopen(L);
 lua_mathlibopen(L);
 lua_dblibopen(L);
 start_trace(L,stderr);
 rc=lua_dofile(L,NULL);
 stop_trace(L);
 return rc;
}
