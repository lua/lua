/*
** strlib.c
** String library to LUA
*/

char *rcs_strlib="$Id: strlib.c,v 1.14 1995/11/10 17:54:31 roberto Exp $";

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "lua.h"
#include "lualib.h"


void lua_arg_error(char *funcname)
{
  char buff[100];
  sprintf(buff, "incorrect arguments to function `%s'", funcname);
  lua_error(buff);
}

char *lua_check_string (int numArg, char *funcname)
{
  lua_Object o = lua_getparam(numArg);
  if (!(lua_isstring(o) || lua_isnumber(o)))
    lua_arg_error(funcname);
  return lua_getstring(o);
}

float lua_check_number (int numArg, char *funcname)
{
  lua_Object o = lua_getparam(numArg);
  if (!lua_isnumber(o))
    lua_arg_error(funcname);
  return lua_getnumber(o);
}

static char *newstring (char *s)
{
  char *ns = (char *)malloc(strlen(s)+1);
  if (ns == 0)
    lua_error("not enough memory for new string");
  strcpy(ns, s);
  return ns;
}


/*
** Return the position of the first caracter of a substring into a string
** LUA interface:
**			n = strfind (string, substring, init, end)
*/
static void str_find (void)
{
 char *s1 = lua_check_string(1, "strfind");
 char *s2 = lua_check_string(2, "strfind");
 int init = (lua_getparam(3) == LUA_NOOBJECT) ? 0 :
                                 (int)lua_check_number(3, "strfind")-1;
 char *f = strstr(s1+init,s2);
 if (f != NULL)
 {
  int pos = f-s1+1;
  if (lua_getparam (4) == LUA_NOOBJECT)
   lua_pushnumber (pos);
  else if ((int)lua_check_number(4, "strfind") >= pos+strlen(s2)-1)
   lua_pushnumber (pos);
  else
   lua_pushnil();
 }
 else
  lua_pushnil();
}

/*
** Return the string length
** LUA interface:
**			n = strlen (string)
*/
static void str_len (void)
{
 char *s = lua_check_string(1, "strlen");
 lua_pushnumber(strlen(s));
}


/*
** Return the substring of a string, from start to end
** LUA interface:
**			substring = strsub (string, start, end)
*/
static void str_sub (void)
{
 char *s = lua_check_string(1, "strsub");
 int start = (int)lua_check_number(2, "strsub");
 int end = (lua_getparam(3) == LUA_NOOBJECT) ? strlen(s) :
                                    (int)lua_check_number(3, "strsub");
 if (end < start || start < 1 || end > strlen(s))
  lua_pushliteral("");
 else
 {
  char temp = s[end];
  s[end] = 0;
  lua_pushstring (&s[start-1]);
  s[end] = temp;
 }
}

/*
** Convert a string according to given function.
*/
typedef int (*strfunc)(int s);
static void str_apply (strfunc f, char *funcname)
{
 char *s, *c;
 c = s = newstring(lua_check_string(1, funcname));
 while (*c != 0)
 {
  *c = f(*c);
  c++;
 }
 lua_pushstring(s);
 free(s);
}

/*
** Convert a string to lower case.
** LUA interface:
**			lowercase = strlower (string)
*/
static void str_lower (void)
{
  str_apply(tolower, "strlower");
}


/*
** Convert a string to upper case.
** LUA interface:
**			uppercase = strupper (string)
*/
static void str_upper (void)
{
  str_apply(toupper, "strupper");
}

/*
** get ascii value of a character in a string
*/
static void str_ascii (void)
{
  char *s = lua_check_string(1, "ascii");
  lua_Object o2 = lua_getparam(2);
  int pos;
  pos = (o2 == LUA_NOOBJECT) ? 0 : (int)lua_check_number(2, "ascii")-1;
  if (pos<0 || pos>=strlen(s))
    lua_arg_error("ascii");
  lua_pushnumber(s[pos]);
}

/*
** converts one or more integers to chars in a string
*/
#define maxparams 50
static void str_int2str (void)
{
  char s[maxparams+1];
  int i = 0;
  while (lua_getparam(++i) != LUA_NOOBJECT)
  {
    if (i > maxparams)
      lua_error("too many parameters to function `int2str'");
    s[i-1] = (int)lua_check_number(i, "int2str");
  }
  s[i-1] = 0;
  lua_pushstring(s);
}

/*
** Open string library
*/
void strlib_open (void)
{
 lua_register ("strfind", str_find);
 lua_register ("strlen", str_len);
 lua_register ("strsub", str_sub);
 lua_register ("strlower", str_lower);
 lua_register ("strupper", str_upper);
 lua_register ("ascii", str_ascii);
 lua_register ("int2str", str_int2str);
}
