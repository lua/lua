/*
** $Id: liolib.c,v 2.13 2002/07/12 18:54:53 roberto Exp roberto $
** Standard I/O (and system) library
** See Copyright Notice in lua.h
*/


#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua.h"

#include "lauxlib.h"
#include "luadebug.h"
#include "lualib.h"



/*
** {======================================================
** FILE Operations
** =======================================================
*/


#ifndef POPEN
#define pclose(f)    (-1)
#endif


#define FILEHANDLE		"FileHandle"
#define CLOSEDFILEHANDLE	"ClosedFileHandle"

#define IO_INPUT		"_input"
#define IO_OUTPUT		"_output"


static int pushresult (lua_State *L, int i) {
  if (i) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    lua_pushnumber(L, errno);
    return 3;
  }
}


static FILE *tofile (lua_State *L, int findex) {
  FILE **f = (FILE **)lua_touserdata(L, findex);
  if (f && lua_getmetatable(L, findex) &&
      lua_rawequal(L, -1, lua_upvalueindex(1))) {
    lua_pop(L, 1);
    return *f;
  }
  if (findex > 0)
    luaL_argerror(L, findex, "bad file");
  return NULL;
}


static void newfile (lua_State *L, FILE *f) {
  lua_boxpointer(L, f);
  lua_pushliteral(L, FILEHANDLE);
  lua_rawget(L, LUA_REGISTRYINDEX);
  lua_setmetatable(L, -2);
}


static void registerfile (lua_State *L, FILE *f, const char *name,
                                                 const char *impname) {
  lua_pushstring(L, name);
  newfile(L, f);
  if (impname) {
    lua_pushstring(L, impname);
    lua_pushvalue(L, -2);
    lua_settable(L, -6);
  }
  lua_settable(L, -3);
}


static int setnewfile (lua_State *L, FILE *f) {
  if (f == NULL)
    return pushresult(L, 0);
  else {
    newfile(L, f);
    return 1;
  }
}


static int io_close (lua_State *L) {
  FILE *f;
  int status = 1;
  if (lua_isnone(L, 1)) {
    lua_pushstring(L, IO_OUTPUT);
    lua_rawget(L, lua_upvalueindex(1));
  }
  f = tofile(L, 1);
  if (f != stdin && f != stdout && f != stderr) {
    lua_settop(L, 1);  /* make sure file is on top */
    lua_pushliteral(L, CLOSEDFILEHANDLE);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_setmetatable(L, 1);
    status = (pclose(f) != -1) || (fclose(f) == 0);
  }
  return pushresult(L, status);
}


static int io_open (lua_State *L) {
  FILE *f = fopen(luaL_check_string(L, 1), luaL_opt_string(L, 2, "r"));
  return setnewfile(L, f);
}


static int io_popen (lua_State *L) {
#ifndef POPEN
  luaL_error(L, "`popen' not supported");
  return 0;
#else
  FILE *f = popen(luaL_check_string(L, 1), luaL_opt_string(L, 2, "r"));
  return setnewfile(L, f);
#endif
}


static int io_tmpfile (lua_State *L) {
  return setnewfile(L, tmpfile());
}


static FILE *getiofile (lua_State *L, const char *name) {
  FILE *f;
  lua_pushstring(L, name);
  lua_rawget(L, lua_upvalueindex(1));
  f = tofile(L, -1);
  if (f == NULL)
    luaL_error(L, "%s is closed", name);
  return f;
}


static int g_iofile (lua_State *L, const char *name, const char *mode) {
  if (lua_isnoneornil(L, 1)) {
    lua_pushstring(L, name);
    lua_rawget(L, lua_upvalueindex(1));
    return 1;
  }
  else {
    const char *filename = lua_tostring(L, 1);
    lua_pushstring(L, name);
    if (filename) {
      FILE *f = fopen(filename, mode);
      luaL_arg_check(L, f, 1,  strerror(errno));
      newfile(L, f);
    }
    else {
      tofile(L, 1);  /* check that it's a valid file handle */
      lua_pushvalue(L, 1);
    }
    lua_rawset(L, lua_upvalueindex(1));
    return 0;
  }
}


static int io_input (lua_State *L) {
  return g_iofile(L, IO_INPUT, "r");
}


static int io_output (lua_State *L) {
  return g_iofile(L, IO_OUTPUT, "w");
}


/*
** {======================================================
** READ
** =======================================================
*/


static int read_number (lua_State *L, FILE *f) {
  lua_Number d;
  if (fscanf(f, LUA_NUMBER_SCAN, &d) == 1) {
    lua_pushnumber(L, d);
    return 1;
  }
  else return 0;  /* read fails */
}


static int test_eof (lua_State *L, FILE *f) {
  int c = getc(f);
  ungetc(c, f);
  lua_pushlstring(L, NULL, 0);
  return (c != EOF);
}


static int read_line (lua_State *L, FILE *f) {
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (;;) {
    size_t l;
    char *p = luaL_prepbuffer(&b);
    if (fgets(p, LUAL_BUFFERSIZE, f) == NULL) {  /* eof? */
      luaL_pushresult(&b);  /* close buffer */
      return (lua_strlen(L, -1) > 0);  /* check whether read something */
    }
    l = strlen(p);
    if (p[l-1] != '\n')
      luaL_addsize(&b, l);
    else {
      luaL_addsize(&b, l - 1);  /* do not include `eol' */
      luaL_pushresult(&b);  /* close buffer */
      return 1;  /* read at least an `eol' */
    }
  }
}


static int read_chars (lua_State *L, FILE *f, size_t n) {
  size_t rlen;  /* how much to read */
  size_t nr;  /* number of chars actually read */
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  rlen = LUAL_BUFFERSIZE;  /* try to read that much each time */
  do {
    char *p = luaL_prepbuffer(&b);
    if (rlen > n) rlen = n;  /* cannot read more than asked */
    nr = fread(p, sizeof(char), rlen, f);
    luaL_addsize(&b, nr);
    n -= nr;  /* still have to read `n' chars */
  } while (n > 0 && nr == rlen);  /* until end of count or eof */
  luaL_pushresult(&b);  /* close buffer */
  return (n == 0 || lua_strlen(L, -1) > 0);
}


static int g_read (lua_State *L, FILE *f, int first) {
  int nargs = lua_gettop(L) - 1;
  int success;
  int n;
  if (nargs == 0) {  /* no arguments? */
    success = read_line(L, f);
    n = first+1;  /* to return 1 result */
  }
  else {  /* ensure stack space for all results and for auxlib's buffer */
    luaL_check_stack(L, nargs+LUA_MINSTACK, "too many arguments");
    success = 1;
    for (n = first; nargs-- && success; n++) {
      if (lua_type(L, n) == LUA_TNUMBER) {
        size_t l = (size_t)lua_tonumber(L, n);
        success = (l == 0) ? test_eof(L, f) : read_chars(L, f, l);
      }
      else {
        const char *p = lua_tostring(L, n);
        if (!p || p[0] != '*')
          return luaL_error(L, "invalid `read' option");
        switch (p[1]) {
          case 'n':  /* number */
            success = read_number(L, f);
            break;
          case 'l':  /* line */
            success = read_line(L, f);
            break;
          case 'a':  /* file */
            read_chars(L, f, ~((size_t)0));  /* read MAX_SIZE_T chars */
            success = 1; /* always success */
            break;
          case 'w':  /* word */
            return luaL_error(L, "obsolete option `*w'");
          default:
            return luaL_argerror(L, n, "invalid format");
        }
      }
    }
  }
  if (!success) {
    lua_pop(L, 1);  /* remove last result */
    lua_pushnil(L);  /* push nil instead */
  }
  return n - first;
}


static int io_read (lua_State *L) {
  return g_read(L, getiofile(L, IO_INPUT), 1);
}


static int f_read (lua_State *L) {
  return g_read(L, tofile(L, 1), 2);
}

/* }====================================================== */


static int g_write (lua_State *L, FILE *f, int arg) {
  int nargs = lua_gettop(L) - 1;
  int status = 1;
  for (; nargs--; arg++) {
    if (lua_type(L, arg) == LUA_TNUMBER) {
      /* optimization: could be done exactly as for strings */
      status = status &&
          fprintf(f, LUA_NUMBER_FMT, lua_tonumber(L, arg)) > 0;
    }
    else {
      size_t l;
      const char *s = luaL_check_lstr(L, arg, &l);
      status = status && (fwrite(s, sizeof(char), l, f) == l);
    }
  }
  pushresult(L, status);
  return 1;
}


static int io_write (lua_State *L) {
  return g_write(L, getiofile(L, IO_OUTPUT), 1);
}


static int f_write (lua_State *L) {
  return g_write(L, tofile(L, 1), 2);
}


static int f_seek (lua_State *L) {
  static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  FILE *f = tofile(L, 1);
  int op = luaL_findstring(luaL_opt_string(L, 2, "cur"), modenames);
  long offset = luaL_opt_long(L, 3, 0);
  luaL_arg_check(L, op != -1, 2, "invalid mode");
  op = fseek(f, offset, mode[op]);
  if (op)
    return pushresult(L, 0);  /* error */
  else {
    lua_pushnumber(L, ftell(f));
    return 1;
  }
}


static int io_flush (lua_State *L) {
  return pushresult(L, fflush(getiofile(L, IO_OUTPUT)) == 0);
}


static int f_flush (lua_State *L) {
  return pushresult(L, fflush(tofile(L, 1)) == 0);
}


static const luaL_reg iolib[] = {
  {"input", io_input},
  {"output", io_output},
  {"close", io_close},
  {"flush", io_flush},
  {"open", io_open},
  {"popen", io_popen},
  {"read", io_read},
  {"tmpfile", io_tmpfile},
  {"write", io_write},
  {NULL, NULL}
};


static const luaL_reg flib[] = {
  {"flush", f_flush},
  {"read", f_read},
  {"seek", f_seek},
  {"write", f_write},
  {"close", io_close},
  {NULL, NULL}
};


static void createmeta (lua_State *L) {
  lua_pushliteral(L, FILEHANDLE);  /* S: FH */
  lua_newtable(L);  /* S: mt FH */
  /* close files when collected */
  lua_pushliteral(L, "__gc");  /* S: `gc' mt FH */
  lua_pushvalue(L, -2);  /* S: mt `gc' mt FH */
  lua_pushcclosure(L, io_close, 1);  /* S: close `gc' mt FH */
  lua_rawset(L, -3);  /* S: mt FH */
  /* file methods */
  lua_pushliteral(L, "__gettable");  /* S: `gettable' mt FH */
  lua_pushvalue(L, -2);  /* S: mt `gettable' mt FH */
  lua_rawset(L, -3);  /* S: mt FH */
  lua_pushvalue(L, -1);  /* S: mt mt FH */
  luaL_openlib(L, flib, 1);  /* S: mt FH */
  /* put new metatable into registry */
  lua_rawset(L, LUA_REGISTRYINDEX);  /* S: empty */
  /* meta table for CLOSEDFILEHANDLE */
  lua_pushliteral(L, CLOSEDFILEHANDLE);
  lua_newtable(L);
  lua_rawset(L, LUA_REGISTRYINDEX);
}

/* }====================================================== */


/*
** {======================================================
** Other O.S. Operations
** =======================================================
*/

static int io_execute (lua_State *L) {
  lua_pushnumber(L, system(luaL_check_string(L, 1)));
  return 1;
}


static int io_remove (lua_State *L) {
  return pushresult(L, remove(luaL_check_string(L, 1)) == 0);
}


static int io_rename (lua_State *L) {
  return pushresult(L, rename(luaL_check_string(L, 1),
                    luaL_check_string(L, 2)) == 0);
}


static int io_tmpname (lua_State *L) {
  char buff[L_tmpnam];
  if (tmpnam(buff) != buff)
    return luaL_error(L, "unable to generate a unique filename");
  lua_pushstring(L, buff);
  return 1;
}


static int io_getenv (lua_State *L) {
  lua_pushstring(L, getenv(luaL_check_string(L, 1)));  /* if NULL push nil */
  return 1;
}


static int io_clock (lua_State *L) {
  lua_pushnumber(L, ((lua_Number)clock())/(lua_Number)CLOCKS_PER_SEC);
  return 1;
}


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/

static void setfield (lua_State *L, const char *key, int value) {
  lua_pushstring(L, key);
  lua_pushnumber(L, value);
  lua_rawset(L, -3);
}

static void setboolfield (lua_State *L, const char *key, int value) {
  lua_pushstring(L, key);
  lua_pushboolean(L, value);
  lua_rawset(L, -3);
}

static int getboolfield (lua_State *L, const char *key) {
  int res;
  lua_pushstring(L, key);
  lua_gettable(L, -2);
  res = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return res;
}


static int getfield (lua_State *L, const char *key, int d) {
  int res;
  lua_pushstring(L, key);
  lua_gettable(L, -2);
  if (lua_isnumber(L, -1))
    res = (int)(lua_tonumber(L, -1));
  else {
    if (d == -2)
      return luaL_error(L, "field `%s' missing in date table", key);
    res = d;
  }
  lua_pop(L, 1);
  return res;
}


static int io_date (lua_State *L) {
  const char *s = luaL_opt_string(L, 1, "%c");
  time_t t = (time_t)(luaL_opt_number(L, 2, -1));
  struct tm *stm;
  if (t == (time_t)(-1))  /* no time given? */
    t = time(NULL);  /* use current time */
  if (*s == '!') {  /* UTC? */
    stm = gmtime(&t);
    s++;  /* skip `!' */
  }
  else
    stm = localtime(&t);
  if (stm == NULL)  /* invalid date? */
    lua_pushnil(L);
  else if (strcmp(s, "*t") == 0) {
    lua_newtable(L);
    setfield(L, "sec", stm->tm_sec);
    setfield(L, "min", stm->tm_min);
    setfield(L, "hour", stm->tm_hour);
    setfield(L, "day", stm->tm_mday);
    setfield(L, "month", stm->tm_mon+1);
    setfield(L, "year", stm->tm_year+1900);
    setfield(L, "wday", stm->tm_wday+1);
    setfield(L, "yday", stm->tm_yday+1);
    setboolfield(L, "isdst", stm->tm_isdst);
  }
  else {
    char b[256];
    if (strftime(b, sizeof(b), s, stm))
      lua_pushstring(L, b);
    else
      return luaL_error(L, "`date' format too long");
  }
  return 1;
}


static int io_time (lua_State *L) {
  if (lua_isnoneornil(L, 1))  /* called without args? */
    lua_pushnumber(L, time(NULL));  /* return current time */
  else {
    time_t t;
    struct tm ts;
    luaL_check_type(L, 1, LUA_TTABLE);
    lua_settop(L, 1);  /* make sure table is at the top */
    ts.tm_sec = getfield(L, "sec", 0);
    ts.tm_min = getfield(L, "min", 0);
    ts.tm_hour = getfield(L, "hour", 12);
    ts.tm_mday = getfield(L, "day", -2);
    ts.tm_mon = getfield(L, "month", -2) - 1;
    ts.tm_year = getfield(L, "year", -2) - 1900;
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
    if (t == (time_t)(-1))
      lua_pushnil(L);
    else
      lua_pushnumber(L, t);
  }
  return 1;
}


static int io_difftime (lua_State *L) {
  lua_pushnumber(L, difftime((time_t)(luaL_check_number(L, 1)),
                             (time_t)(luaL_opt_number(L, 2, 0))));
  return 1;
}

/* }====================================================== */


static int io_setloc (lua_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = lua_tostring(L, 1);
  int op = luaL_findstring(luaL_opt_string(L, 2, "all"), catnames);
  luaL_arg_check(L, l || lua_isnoneornil(L, 1), 1, "string expected");
  luaL_arg_check(L, op != -1, 2, "invalid option");
  lua_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int io_exit (lua_State *L) {
  exit(luaL_opt_int(L, 1, EXIT_SUCCESS));
  return 0;  /* to avoid warnings */
}

static const luaL_reg syslib[] = {
  {"clock",     io_clock},
  {"date",      io_date},
  {"difftime",  io_difftime},
  {"execute",   io_execute},
  {"exit",      io_exit},
  {"getenv",    io_getenv},
  {"remove",    io_remove},
  {"rename",    io_rename},
  {"setlocale", io_setloc},
  {"time",      io_time},
  {"tmpname",   io_tmpname},
  {NULL, NULL}
};

/* }====================================================== */



LUALIB_API int lua_iolibopen (lua_State *L) {
  createmeta(L);
  luaL_opennamedlib(L, LUA_OSLIBNAME, syslib, 0);
  lua_pushliteral(L, FILEHANDLE);  /* S: FH */
  lua_rawget(L, LUA_REGISTRYINDEX);  /* S: mt */
  lua_pushvalue(L, -1);  /* S: mt mt */
  luaL_opennamedlib(L, LUA_IOLIBNAME, iolib, 1);  /* S: mt */
  lua_pushliteral(L, LUA_IOLIBNAME);  /* S: `io' mt */
  lua_gettable(L, LUA_GLOBALSINDEX);  /* S: io mt */
  /* put predefined file handles into `io' table */
  registerfile(L, stdin, "stdin", IO_INPUT);
  registerfile(L, stdout, "stdout", IO_OUTPUT);
  registerfile(L, stderr, "stderr", NULL);
  lua_pop(L, 2);  /* S: empty */
  return 0;
}

