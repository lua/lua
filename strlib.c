/*
** strlib.c
** String library to LUA
*/

char *rcs_strlib="$Id: strlib.c,v 1.14 1995/11/10 17:54:31 roberto Exp roberto $";

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

char *luaI_addchar (int c)
{
  static char *buff = NULL;
  static int max = 0;
  static int n = 0;
  if (n >= max)
  {
    if (max == 0)
    {
      max = 100;
      buff = (char *)malloc(max);
    }
    else
    {
      max *= 2;
      buff = (char *)realloc(buff, max);
    }
    if (buff == NULL)
      lua_error("memory overflow");
  }
  buff[n++] = c;
  if (c == 0)
    n = 0;  /* prepare for next string */
  return buff;
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
** Convert a string to lower case.
** LUA interface:
**			lowercase = strlower (string)
*/
static void str_lower (void)
{
  char *s = lua_check_string(1, "strlower");
  luaI_addchar(0);
  while (*s)
    luaI_addchar(tolower(*s++));
  lua_pushstring(luaI_addchar(0));
}


/*
** Convert a string to upper case.
** LUA interface:
**			uppercase = strupper (string)
*/
static void str_upper (void)
{
  char *s = lua_check_string(1, "strupper");
  luaI_addchar(0);
  while (*s)
    luaI_addchar(toupper(*s++));
  lua_pushstring(luaI_addchar(0));
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
static void str_int2str (void)
{
  int i = 0;
  luaI_addchar(0);
  while (lua_getparam(++i) != LUA_NOOBJECT)
    luaI_addchar((int)lua_check_number(i, "int2str"));
  lua_pushstring(luaI_addchar(0));
}


#define MAX_CONVERTION 2000
#define MAX_FORMAT 50

static void io_format (void)
{
  int arg = 1;
  char *strfrmt = lua_check_string(arg++, "format");
  luaI_addchar(0);  /* initialize */
  while (*strfrmt)
  {
    if (*strfrmt != '%')
      luaI_addchar(*strfrmt++);
    else if (*++strfrmt == '%')
      luaI_addchar(*strfrmt++);  /* %% */
    else
    { /* format item */
      char form[MAX_FORMAT];      /* store the format ('%...') */
      char buff[MAX_CONVERTION];  /* store the formated value */
      int size = 0;
      int i = 0;
      form[i++] = '%';
      form[i] = *strfrmt++;
      while (!isalpha(form[i]))
      {
        if (isdigit(form[i]))
        {
          size = size*10 + form[i]-'0';
          if (size >= MAX_CONVERTION)
            lua_error("format size/precision too long in function `format'");
        }
        else if (form[i] == '.')
          size = 0;  /* re-start */
        if (++i >= MAX_FORMAT)
            lua_error("bad format in function `format'");
        form[i] = *strfrmt++;
      }
      form[i+1] = 0;  /* ends string */
      switch (form[i])
      {
        case 's':
        {
          char *s = lua_check_string(arg++, "format");
          if (strlen(s) >= MAX_CONVERTION)
            lua_error("string argument too long in function `format'");
          sprintf(buff, form, s);
          break;
        }
        case 'c':  case 'd':  case 'i': case 'o':
        case 'u':  case 'x':  case 'X':
          sprintf(buff, form, (int)lua_check_number(arg++, "format"));
          break;
        case 'e':  case 'E': case 'f': case 'g':
          sprintf(buff, form, lua_check_number(arg++, "format"));
          break;
        default:  /* also treat cases 'pnLlh' */
          lua_error("invalid format option in function `format'");
      }
      for (i=0; buff[i]; i++)  /* move formated value to result */
        luaI_addchar(buff[i]);
    }
  }
  lua_pushstring(luaI_addchar(0));  /* push the result */
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
 lua_register ("format",    io_format);
}
