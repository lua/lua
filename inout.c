/*
** inout.c
** Provide function to realise the input/output function and debugger 
** facilities.
** Also provides some predefined lua functions.
*/

char *rcs_inout="$Id: inout.c,v 2.10 1994/11/09 18:09:22 roberto Exp roberto $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "tree.h"
#include "lua.h"

/* Exported variables */
int lua_linenumber;
int lua_debug;
int lua_debugline = 0;

/* Internal variables */
#ifndef MAXFUNCSTACK
#define MAXFUNCSTACK 64
#endif
static struct {
  char *file;
  int function;
  int line;
} funcstack[MAXFUNCSTACK];
static int nfuncstack=0;

static FILE *fp;
static char *st;

/*
** Function to get the next character from the input file
*/
static int fileinput (void)
{
 return fgetc (fp);
}

/*
** Function to get the next character from the input string
*/
static int stringinput (void)
{
 return *st++;
}

/*
** Function to open a file to be input unit. 
** Return 0 on success or error message on error.
*/
char *lua_openfile (char *fn)
{
 lua_linenumber = 1;
 lua_setinput (fileinput);
 fp = fopen (fn, "r");
 if (fp == NULL)
 {
   static char buff[32];
   sprintf(buff, "unable to open file %.10s", fn);
   return buff;
 }
 return lua_addfile (fn);
}

/*
** Function to close an opened file
*/
void lua_closefile (void)
{
 if (fp != NULL)
 {
  lua_delfile();
  fclose (fp);
  fp = NULL;
 }
}

/*
** Function to open a string to be input unit
*/
char *lua_openstring (char *s)
{
 lua_linenumber = 1;
 lua_setinput (stringinput);
 st = s;
 {
  char sn[64];
  sprintf (sn, "String: %10.10s...", s);
  return lua_addfile (sn);
 }
}

/*
** Function to close an opened string
*/
void lua_closestring (void)
{
 lua_delfile();
}


/*
** Called to execute  SETFUNCTION opcode, this function pushs a function into
** function stack. Return 0 on success or 1 on error.
*/
int lua_pushfunction (char *file, int function)
{
 if (nfuncstack >= MAXFUNCSTACK-1)
 {
  lua_error ("function stack overflow");
  return 1;
 }
 funcstack[nfuncstack].function = function;
 funcstack[nfuncstack].file = file;
 funcstack[nfuncstack].line= lua_debugline;
 nfuncstack++;
 return 0;
}

/*
** Called to execute  RESET opcode, this function pops a function from 
** function stack.
*/
void lua_popfunction (void)
{
 --nfuncstack;
 lua_debugline = funcstack[nfuncstack].line;
}

/*
** Report bug building a message and sending it to lua_error function.
*/
void lua_reportbug (char *s)
{
 char msg[1024];
 strcpy (msg, s);
 if (lua_debugline != 0)
 {
  int i;
  if (nfuncstack > 0)
  {
   sprintf (strchr(msg,0), 
         "\n\tin statement begining at line %d in function \"%s\" of file \"%s\"",
         lua_debugline, lua_constant[funcstack[nfuncstack-1].function],
  	 funcstack[nfuncstack-1].file);
   sprintf (strchr(msg,0), "\n\tactive stack\n");
   for (i=nfuncstack-1; i>=0; i--)
    sprintf (strchr(msg,0), "\t-> function \"%s\" of file \"%s\"\n", 
                            lua_constant[funcstack[i].function],
			    funcstack[i].file);
  }
  else
  {
   sprintf (strchr(msg,0),
         "\n\tin statement begining at line %d of file \"%s\"", 
         lua_debugline, lua_filename());
  }
 }
 lua_error (msg);
}

 
/*
** Internal function: do a string
*/
void lua_internaldostring (void)
{
 lua_Object obj = lua_getparam (1);
 if (lua_isstring(obj) && !lua_dostring(lua_getstring(obj)))
  lua_pushnumber(1);
 else
  lua_pushnil();
}

/*
** Internal function: do a file
*/
void lua_internaldofile (void)
{
 lua_Object obj = lua_getparam (1);
 if (lua_isstring(obj) && !lua_dofile(lua_getstring(obj)))
  lua_pushnumber(1);
 else
  lua_pushnil();
}
 
/*
** Internal function: print object values
*/
void lua_print (void)
{
 int i=1;
 lua_Object obj;
 while ((obj=lua_getparam (i++)) != 0)
 {
  if      (lua_isnumber(obj))    printf("%g\n",lua_getnumber(obj));
  else if (lua_isstring(obj))    printf("%s\n",lua_getstring(obj));
  else if (lua_isfunction(obj))  printf("function: %p\n",bvalue(luaI_Address(obj)));
  else if (lua_iscfunction(obj)) printf("cfunction: %p\n",lua_getcfunction(obj)
);
  else if (lua_isuserdata(obj))  printf("userdata: %p\n",lua_getuserdata(obj));
  else if (lua_istable(obj))     printf("table: %p\n",avalue(luaI_Address(obj)));
  else if (lua_isnil(obj))       printf("nil\n");
  else                           printf("invalid value to print\n");
 }
}


/*
** Internal function: return an object type.
*/
void luaI_type (void)
{
  lua_Object o = lua_getparam(1);
  if (o == 0)
    lua_error("no parameter to function 'type'");
  switch (lua_type(o))
  {
    case LUA_T_NIL :
      lua_pushstring("nil");
      break;
    case LUA_T_NUMBER :
      lua_pushstring("number");
      break;
    case LUA_T_STRING :
      lua_pushstring("string");
      break;
    case LUA_T_ARRAY :
      lua_pushstring("table");
      break;
    case LUA_T_FUNCTION :
      lua_pushstring("function");
      break;
    case LUA_T_CFUNCTION :
      lua_pushstring("cfunction");
      break;
    default :
      lua_pushstring("userdata");
      break;
  }
}
 
/*
** Internal function: convert an object to a number
*/
void lua_obj2number (void)
{
  lua_Object o = lua_getparam(1);
  if (lua_isnumber(o))
    lua_pushobject(o);
  else if (lua_isstring(o))
  {
    char c;
    float f;
    if (sscanf(lua_getstring(o),"%f %c",&f,&c) == 1)
      lua_pushnumber(f);
    else
      lua_pushnil();
  }
  else
    lua_pushnil();
}


void luaI_error (void)
{
  char *s = lua_getstring(lua_getparam(1));
  if (s == NULL) s = "(no message)";
  lua_reportbug(s);
}

