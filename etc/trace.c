/*
* trace.c
* a simple execution tracer for Lua
*/

#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "luadebug.h"

static FILE* LOG;		/* output file */
static int L=0;			/* indentation level */

static void linehook(int line)
{
 fprintf(LOG,"%*sLINE(%d)\t-- %d\n",L,"",line,L);
}

static void callhook(lua_Function func, char* file, int line)
{
 fprintf(LOG,"%*sCALL('%s',%d)\t-- %d\n",L,"",file,line,L);
 if (line==0 && strcmp(file,"(return)")==0) --L; else ++L;
}

void start_trace(FILE* logfile)
{
 lua_setlinehook(linehook);
 lua_setcallhook(callhook);
 lua_setdebug(1);
 LOG=logfile;
}

void stop_trace(void)
{
 lua_setlinehook(NULL);
 lua_setcallhook(NULL);
 lua_setdebug(0);
 fclose(LOG);
}

int main(void)
{
 int rc;
 lua_open();
 start_trace(stderr);
 rc=lua_dofile(0);
 stop_trace();
 return rc;
}
