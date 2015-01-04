/*
** inout.c
** Provide function to realise the input/output function and debugger 
** facilities.
** Also provides some predefined lua functions.
*/

char *rcs_inout="$Id: inout.c,v 2.43 1996/09/25 12:57:22 roberto Exp $";

#include <stdio.h>
#include <string.h>

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
  int c = fgetc(fp);
  return (c == EOF) ? 0 : c;
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
#define SIZE_PREF 20  /* size of string prefix to appear in error messages */
void lua_openstring (char *s)
{
  char buff[SIZE_PREF+25];
  lua_setinput(stringinput);
  st = s;
  strcpy(buff, "(dostring) >> ");
  strncat(buff, s, SIZE_PREF);
  if (strlen(s) > SIZE_PREF) strcat(buff, "...");
  lua_parsedfile = luaI_createfixedstring(buff)->str;
}

/*
** Function to close an opened string
*/
void lua_closestring (void)
{
}


static void check_arg (int cond, char *func)
{
  if (!cond)
  {
    char buff[100];
    sprintf(buff, "incorrect argument to function `%s'", func);
    lua_error(buff);
  }
}


static int passresults (void)
{
  int arg = 0;
  lua_Object obj;
  while ((obj = lua_getresult(++arg)) != LUA_NOOBJECT)
    lua_pushobject(obj);
  return arg-1;
}
 
/*
** Internal function: do a string
*/
void lua_internaldostring (void)
{
  lua_Object obj = lua_getparam (1);
  if (lua_isstring(obj) && lua_dostring(lua_getstring(obj)) == 0)
    if (passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
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
 if (lua_dofile(fname) == 0)
    if (passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
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
  check_arg(lua_isstring(name), "setglobal");
  lua_pushobject(value);
  lua_storeglobal(lua_getstring(name));
  lua_pushobject(value);  /* return given value */
}

void luaI_getglobal (void)
{
  lua_Object name = lua_getparam(1);
  check_arg(lua_isstring(name), "getglobal");
  lua_pushobject(lua_getglobal(lua_getstring(name)));
}

#define MAXPARAMS	256
void luaI_call (void)
{
  lua_Object f = lua_getparam(1);
  lua_Object arg = lua_getparam(2);
  lua_Object temp, params[MAXPARAMS];
  int narg, i;
  check_arg(lua_istable(arg), "call");
  check_arg(lua_isfunction(f), "call");
  /* narg = arg.n */
  lua_pushobject(arg);
  lua_pushstring("n");
  temp = lua_getsubscript();
  narg = lua_isnumber(temp) ? lua_getnumber(temp) : MAXPARAMS+1;
  /* read arg[1...n] */
  for (i=0; i<narg; i++) {
    if (i>=MAXPARAMS)
      lua_error("argument list too long in function `call'");
    lua_pushobject(arg);
    lua_pushnumber(i+1);
    params[i] = lua_getsubscript();
    if (narg == MAXPARAMS+1 && lua_isnil(params[i])) {
      narg = i;
      break;
    }
  }
  /* push parameters and do the call */
  for (i=0; i<narg; i++)
    lua_pushobject(params[i]);
  if (lua_callfunction(f))
    lua_error(NULL);
  else
    passresults();
}
