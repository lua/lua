/*
* trace.c
* a simple execution tracer
*/

#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "luadebug.h"

static FILE* P;			/* output file */
static int L=0;			/* indentation level */

static void linehook(int line)
{
 fprintf(P,"%*sLINE(%d)\t-- %d\n",L,"",line,L);
}

static void callhook(lua_Function func, char* file, int line)
{
 fprintf(P,"%*sCALL('%s',%d)\t-- %d\n",L,"",file,line,L);
 if (line==0 && strcmp(file,"(return)")==0) --L; else ++L;
}

void start_trace(void)
{
 lua_linehook=linehook;
 lua_callhook=callhook;
 lua_debug=1;
#if 0
 P=fopen("trace.out","w");
#else
 P=stderr;
#endif
}

void stop_trace(void)
{
 lua_linehook=NULL;
 lua_callhook=NULL;
 lua_debug=0;
 fclose(P);
}

int main(void)
{
 int rc;
 lua_open();
 start_trace();
 rc=lua_dofile(0);
 stop_trace();
 return rc;
}
