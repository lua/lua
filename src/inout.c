/*
** inout.c
** Provide function to realise the input/output function and debugger 
** facilities.
** Also provides some predefined lua functions.
*/

char *rcs_inout="$Id: inout.c,v 2.69 1997/06/27 22:38:49 roberto Exp $";

#include <stdio.h>
#include <string.h>

#include "auxlib.h"
#include "fallback.h"
#include "hash.h"
#include "inout.h"
#include "lex.h"
#include "lua.h"
#include "luamem.h"
#include "luamem.h"
#include "opcode.h"
#include "table.h"
#include "tree.h"
#include "undump.h"
#include "zio.h"


/* Exported variables */
Word lua_linenumber;
char *lua_parsedfile;


char *luaI_typenames[] = { /* ORDER LUA_T */
  "userdata", "line", "cmark", "mark", "function",
  "function", "table", "string", "number", "nil",
  NULL
};



void luaI_setparsedfile (char *name)
{
  lua_parsedfile = luaI_createfixedstring(name)->str;
}


int lua_doFILE (FILE *f, int bin)
{
  ZIO z;
  luaZ_Fopen(&z, f);
  if (bin)
    return luaI_undump(&z);
  else {
    lua_setinput(&z);
    return lua_domain();
  }
}                      


int lua_dofile (char *filename)
{
  int status;
  int c;
  FILE *f = (filename == NULL) ? stdin : fopen(filename, "r");
  if (f == NULL)
    return 2;
  luaI_setparsedfile(filename?filename:"(stdin)");
  c = fgetc(f);
  ungetc(c, f);
  if (c == ID_CHUNK) {
    f = freopen(filename, "rb", f);  /* set binary mode */
    status = lua_doFILE(f, 1);
  }
  else {
    if (c == '#')
      while ((c=fgetc(f)) != '\n') /* skip first line */;
    status = lua_doFILE(f, 0);
  }
  if (f != stdin)
    fclose(f);
  return status;
}                      



#define SIZE_PREF 20  /* size of string prefix to appear in error messages */


int lua_dobuffer (char *buff, int size)
{
  int status;
  ZIO z;
  luaI_setparsedfile("(buffer)");
  luaZ_mopen(&z, buff, size);
  status = luaI_undump(&z);
  return status;
}


int lua_dostring (char *str)
{
  int status;
  char buff[SIZE_PREF+25];
  char *temp;
  ZIO z;
  if (str == NULL) return 1;
  sprintf(buff, "(dostring) >> %.20s", str);
  temp = strchr(buff, '\n');
  if (temp) *temp = 0;  /* end string after first line */
  luaI_setparsedfile(buff);
  luaZ_sopen(&z, str);
  lua_setinput(&z);
  status = lua_domain();
  return status;
}




static int passresults (void)
{
  int arg = 0;
  lua_Object obj;
  while ((obj = lua_getresult(++arg)) != LUA_NOOBJECT)
    lua_pushobject(obj);
  return arg-1;
}


static void packresults (void)
{
  int arg = 0;
  lua_Object obj;
  lua_Object table = lua_createtable();
  while ((obj = lua_getresult(++arg)) != LUA_NOOBJECT) {
    lua_pushobject(table);
    lua_pushnumber(arg);
    lua_pushobject(obj);
    lua_rawsettable();
  }
  lua_pushobject(table);
  lua_pushstring("n");
  lua_pushnumber(arg-1);
  lua_rawsettable();
  lua_pushobject(table);  /* final result */
}
 
/*
** Internal function: do a string
*/
static void lua_internaldostring (void)
{
  lua_Object err = lua_getparam(2);
  if (err != LUA_NOOBJECT) {  /* set new error method */
    luaL_arg_check(lua_isnil(err) || lua_isfunction(err), 2,
                   "must be a valid error handler");
    lua_pushobject(err);
    err = lua_seterrormethod();
  }
  if (lua_dostring(luaL_check_string(1)) == 0)
    if (passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
  if (err != LUA_NOOBJECT) {  /* restore old error method */
    lua_pushobject(err);
    lua_seterrormethod();
  }
}

/*
** Internal function: do a file
*/
static void lua_internaldofile (void)
{
 char *fname = luaL_opt_string(1, NULL);
 if (lua_dofile(fname) == 0)
    if (passresults() == 0)
      lua_pushuserdata(NULL);  /* at least one result to signal no errors */
}
 

static char *tostring (lua_Object obj)
{
  TObject *o = luaI_Address(obj);
  switch (ttype(o)) {
    case LUA_T_NUMBER:  case LUA_T_STRING:
      return lua_getstring(obj);
    case LUA_T_ARRAY: case LUA_T_FUNCTION:
    case LUA_T_CFUNCTION: case LUA_T_NIL:
      return luaI_typenames[-ttype(o)];
    case LUA_T_USERDATA: {
      char *buff = luaI_buffer(30);
      sprintf(buff, "userdata: %p", o->value.ts->u.v);
      return buff;
    }
    default: return "<unknown object>";
  }
}

static void luaI_tostring (void)
{
  lua_pushstring(tostring(lua_getparam(1)));
}

static void luaI_print (void)
{
  int i = 1;
  lua_Object obj;
  while ((obj = lua_getparam(i++)) != LUA_NOOBJECT)
    printf("%s\n", tostring(obj));
}

static void luaI_type (void)
{
  lua_Object o = lua_getparam(1);
  luaL_arg_check(o != LUA_NOOBJECT, 1, "no argument");
  lua_pushstring(luaI_typenames[-ttype(luaI_Address(o))]);
  lua_pushnumber(lua_tag(o));
}

/*
** Internal function: convert an object to a number
*/
static void lua_obj2number (void)
{
  lua_Object o = lua_getparam(1);
  if (lua_isnumber(o))
    lua_pushnumber(lua_getnumber(o));
}


static void luaI_error (void)
{
  char *s = lua_getstring(lua_getparam(1));
  if (s == NULL) s = "(no message)";
  lua_error(s);
}

static void luaI_assert (void)
{
  lua_Object p = lua_getparam(1);
  if (p == LUA_NOOBJECT || lua_isnil(p))
    lua_error("assertion failed!");
}

static void luaI_setglobal (void)
{
  lua_Object value = lua_getparam(2);
  luaL_arg_check(value != LUA_NOOBJECT, 2, NULL);
  lua_pushobject(value);
  lua_setglobal(luaL_check_string(1));
  lua_pushobject(value);  /* return given value */
}

static void luaI_rawsetglobal (void)
{
  lua_Object value = lua_getparam(2);
  luaL_arg_check(value != LUA_NOOBJECT, 2, NULL);
  lua_pushobject(value);
  lua_rawsetglobal(luaL_check_string(1));
  lua_pushobject(value);  /* return given value */
}

static void luaI_getglobal (void)
{
  lua_pushobject(lua_getglobal(luaL_check_string(1)));
}

static void luaI_rawgetglobal (void)
{
  lua_pushobject(lua_rawgetglobal(luaL_check_string(1)));
}

static void luatag (void)
{
  lua_pushnumber(lua_tag(lua_getparam(1)));
}


static int getnarg (lua_Object table)
{
  lua_Object temp;
  /* temp = table.n */
  lua_pushobject(table); lua_pushstring("n"); temp = lua_gettable();
  return (lua_isnumber(temp) ? lua_getnumber(temp) : MAX_WORD);
}

static void luaI_call (void)
{
  lua_Object f = lua_getparam(1);
  lua_Object arg = lua_getparam(2);
  int withtable = (strcmp(luaL_opt_string(3, ""), "pack") == 0);
  int narg, i;
  luaL_arg_check(lua_isfunction(f), 1, "function expected");
  luaL_arg_check(lua_istable(arg), 2, "table expected");
  narg = getnarg(arg);
  /* push arg[1...n] */
  for (i=0; i<narg; i++) {
    lua_Object temp;
    /* temp = arg[i+1] */
    lua_pushobject(arg); lua_pushnumber(i+1); temp = lua_gettable();
    if (narg == MAX_WORD && lua_isnil(temp))
      break;
    lua_pushobject(temp);
  }
  if (lua_callfunction(f))
    lua_error(NULL);
  else if (withtable)
    packresults();
  else
    passresults();
}

static void luaIl_settag (void)
{
  lua_Object o = lua_getparam(1);
  luaL_arg_check(lua_istable(o), 1, "table expected");
  lua_pushobject(o);
  lua_settag(luaL_check_number(2));
}

static void luaIl_newtag (void)
{
  lua_pushnumber(lua_newtag());
}

static void rawgettable (void)
{
  lua_Object t = lua_getparam(1);
  lua_Object i = lua_getparam(2);
  luaL_arg_check(t != LUA_NOOBJECT, 1, NULL);
  luaL_arg_check(i != LUA_NOOBJECT, 2, NULL);
  lua_pushobject(t);
  lua_pushobject(i);
  lua_pushobject(lua_rawgettable());
}

static void rawsettable (void)
{
  lua_Object t = lua_getparam(1);
  lua_Object i = lua_getparam(2);
  lua_Object v = lua_getparam(3);
  luaL_arg_check(t != LUA_NOOBJECT && i != LUA_NOOBJECT && v != LUA_NOOBJECT,
                 0, NULL);
  lua_pushobject(t);
  lua_pushobject(i);
  lua_pushobject(v);
  lua_rawsettable();
}


static void luaI_collectgarbage (void)
{
  lua_pushnumber(lua_collectgarbage(luaL_opt_number(1, 0)));
}


/*
** Internal functions
*/
static struct {
  char *name;
  lua_CFunction func;
} int_funcs[] = {
  {"assert", luaI_assert},
  {"call", luaI_call},
  {"collectgarbage", luaI_collectgarbage},
  {"dofile", lua_internaldofile},
  {"dostring", lua_internaldostring},
  {"error", luaI_error},
  {"getglobal", luaI_getglobal},
  {"newtag", luaIl_newtag},
  {"next", lua_next},
  {"nextvar", luaI_nextvar},
  {"print", luaI_print},
  {"rawgetglobal", luaI_rawgetglobal},
  {"rawgettable", rawgettable},
  {"rawsetglobal", luaI_rawsetglobal},
  {"rawsettable", rawsettable},
  {"seterrormethod", luaI_seterrormethod},
#if LUA_COMPAT2_5
  {"setfallback", luaI_setfallback},
#endif
  {"setglobal", luaI_setglobal},
  {"settagmethod", luaI_settagmethod},
  {"gettagmethod", luaI_gettagmethod},
  {"settag", luaIl_settag},
  {"tonumber", lua_obj2number},
  {"tostring", luaI_tostring},
  {"tag", luatag},
  {"type", luaI_type}
};

#define INTFUNCSIZE (sizeof(int_funcs)/sizeof(int_funcs[0]))


void luaI_predefine (void)
{
  int i;
  Word n;
  for (i=0; i<INTFUNCSIZE; i++) {
    n = luaI_findsymbolbyname(int_funcs[i].name);
    s_ttype(n) = LUA_T_CFUNCTION; s_fvalue(n) = int_funcs[i].func;
  }
  n = luaI_findsymbolbyname("_VERSION");
  s_ttype(n) = LUA_T_STRING; s_tsvalue(n) = lua_createstring(LUA_VERSION);
}


