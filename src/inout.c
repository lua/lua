/*
** inout.c
** Provide function to realise the input/output function and debugger 
** facilities.
** Also provides some predefined lua functions.
*/

char *rcs_inout="$Id: inout.c,v 2.36 1996/03/19 22:28:37 roberto Exp $";

#include <stdio.h>

#include "lex.h"
#include "opcode.h"
#include "inout.h"
#include "table.h"
#include "tree.h"
#include "lua.h"
#include "mem.h"


/* Exported variables */
Word lua_linenumber;
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
** Return the file.
*/
FILE *lua_openfile (char *fn)
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
   return NULL;
 lua_linenumber = 1;
 lua_parsedfile = luaI_createfixedstring(fn)->str;
 return fp;
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
 lua_parsedfile = luaI_createfixedstring("(string)")->str;
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
 char *fname = NULL;
 if (lua_isstring(obj))
   fname = lua_getstring(obj);
 else if (obj != LUA_NOOBJECT)
   lua_error("invalid argument to function `dofile'");
 /* else fname = NULL */
 if (!lua_dofile(fname))
  lua_pushnumber(1);
 else
  lua_pushnil();
}
 

static char *tostring (lua_Object obj)
{
  char *buff = luaI_buffer(20);
  if (lua_isstring(obj))   /* get strings and numbers */
    return lua_getstring(obj);
  else switch(lua_type(obj))
  {
    case LUA_T_FUNCTION:
      sprintf(buff, "function: %p", (luaI_Address(obj))->value.tf);
      break;
    case LUA_T_CFUNCTION:
      sprintf(buff, "cfunction: %p", lua_getcfunction(obj));
      break;
    case LUA_T_ARRAY:
      sprintf(buff, "table: %p", avalue(luaI_Address(obj)));
      break;
    case LUA_T_NIL:
      sprintf(buff, "nil");
      break;
    default:
      sprintf(buff, "userdata: %p", lua_getuserdata(obj));
      break;
  }
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
    lua_pushnumber(lua_getnumber(o));
  else
    lua_pushnil();
}


void luaI_error (void)
{
  char *s = lua_getstring(lua_getparam(1));
  if (s == NULL) s = "(no message)";
  lua_error(s);
}

void luaI_assert (void)
{
  lua_Object p = lua_getparam(1);
  if (p == LUA_NOOBJECT || lua_isnil(p))
    lua_error("assertion failed!");
}

void luaI_setglobal (void)
{
  lua_Object name = lua_getparam(1);
  lua_Object value = lua_getparam(2);
  if (!lua_isstring(name))
    lua_error("incorrect argument to function `setglobal'");
  lua_pushobject(value);
  lua_storeglobal(lua_getstring(name));
  lua_pushobject(value);  /* return given value */
}

void luaI_getglobal (void)
{
  lua_Object name = lua_getparam(1);
  if (!lua_isstring(name))
    lua_error("incorrect argument to function `getglobal'");
  lua_pushobject(lua_getglobal(lua_getstring(name)));
}
