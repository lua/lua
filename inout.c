/*
** inout.c
** Provide function to realise the input/output function and debugger 
** facilities.
** Also provides some predefined lua functions.
*/

char *rcs_inout="$Id: inout.c,v 2.25 1995/10/25 13:05:51 roberto Exp roberto $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "tree.h"
#include "lua.h"


#ifndef MAXFUNCSTACK
#define MAXFUNCSTACK 100
#endif

#define MAXMESSAGE MAXFUNCSTACK*80


/* Exported variables */
Word lua_linenumber;
Bool lua_debug = 0;
char *lua_parsedfile;


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
 lua_setinput (fileinput);
 if (fn == NULL)
 {
   fp = stdin;
   fn = "(stdin)";
 }
 else
   fp = fopen (fn, "r");
 if (fp == NULL)
 {
   static char buff[255];
   sprintf(buff, "unable to open file `%.200s'", fn);
   return buff;
 }
 lua_linenumber = 1;
 lua_parsedfile = lua_constcreate(fn)->ts.str;
 return NULL;
}

/*
** Function to close an opened file
*/
void lua_closefile (void)
{
 if (fp != NULL && fp != stdin)
 {
  fclose (fp);
  fp = NULL;
 }
}

/*
** Function to open a string to be input unit
*/
void lua_openstring (char *s)
{
 lua_setinput (stringinput);
 st = s;
 lua_linenumber = 1;
 lua_parsedfile = lua_constcreate("(string)")->ts.str;
}

/*
** Function to close an opened string
*/
void lua_closestring (void)
{
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
 

static char *tostring (lua_Object obj)
{
  static char buff[20];
  if (lua_isstring(obj))
    return lua_getstring(obj);
  if (lua_isnumber(obj))
    sprintf(buff, "%g", lua_getnumber(obj));
  else if (lua_isfunction(obj))
    sprintf(buff, "function: %p", (luaI_Address(obj))->value.tf);
  else if (lua_iscfunction(obj))
    sprintf(buff, "cfunction: %p", lua_getcfunction(obj));
  else if (lua_isuserdata(obj))
    sprintf(buff, "userdata: %p", lua_getuserdata(obj));
  else if (lua_istable(obj))
    sprintf(buff, "table: %p", avalue(luaI_Address(obj)));
  else if (lua_isnil(obj))
    sprintf(buff, "nil");
  else buff[0] = 0;
  return buff;
}

void luaI_tostring (void)
{
  lua_pushstring(tostring(lua_getparam(1)));
}

void luaI_print (void)
{
  int i = 1;
  lua_Object obj;
  while ((obj = lua_getparam(i++)) != LUA_NOOBJECT)
    printf("%s\n", tostring(obj));
}

/*
** Internal function: return an object type.
*/
void luaI_type (void)
{
  lua_Object o = lua_getparam(1);
  int t;
  if (o == LUA_NOOBJECT)
    lua_error("no parameter to function 'type'");
  t = lua_type(o);
  switch (t)
  {
    case LUA_T_NIL :
      lua_pushliteral("nil");
      break;
    case LUA_T_NUMBER :
      lua_pushliteral("number");
      break;
    case LUA_T_STRING :
      lua_pushliteral("string");
      break;
    case LUA_T_ARRAY :
      lua_pushliteral("table");
      break;
    case LUA_T_FUNCTION :
    case LUA_T_CFUNCTION :
      lua_pushliteral("function");
      break;
    default :
      lua_pushliteral("userdata");
      break;
  }
  lua_pushnumber(t);
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
  lua_error(s);
}

