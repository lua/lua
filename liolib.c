/*
** $Id: liolib.c,v 1.124 2001/10/26 17:33:30 roberto Exp $
** Standard I/O (and system) library
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua.h"

#include "lauxlib.h"
#include "luadebug.h"
#include "lualib.h"


#ifndef OLD_ANSI
#include <errno.h>
#include <locale.h>
#endif



#ifdef POPEN
/* FILE *popen();
int pclose(); */
#define CLOSEFILE(L, f)    ((pclose(f) == -1) ? fclose(f) : 0)
#else
/* no support for popen */
#define popen(x,y) NULL  /* that is, popen always fails */
#define CLOSEFILE(L, f)    (fclose(f))
#endif


#define INFILE	0
#define OUTFILE 1
#define NOFILE	2

#define FILEHANDLE		"FileHandle"
#define CLOSEDFILEHANDLE	"ClosedFileHandle"


static const char *const filenames[] = {"_INPUT", "_OUTPUT"};
static const char *const basicfiles[] = {"_STDIN", "_STDOUT"};


static int pushresult (lua_State *L, int i) {
  if (i) {
    lua_pushnumber(L, 1);
    return 1;
  }
  else {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    lua_pushnumber(L, errno);
    return 3;
  }
}


/*
** {======================================================
** FILE Operations
** =======================================================
*/


#define checkfile(L,f)	(strcmp(lua_type(L,(f)), FILEHANDLE) == 0)


static FILE *getopthandle (lua_State *L, int inout) {
  FILE *p = (FILE *)(lua_touserdata(L, 1));
  if (p != NULL) {  /* is it a userdata ? */
    if (!checkfile(L, 1)) {  /* not a valid file handle? */
      if (strcmp(lua_type(L, 1), CLOSEDFILEHANDLE) == 0)
        luaL_argerror(L, 1, "file is closed");
      else
        luaL_argerror(L, 1, "(invalid value)");
    }
    lua_pushvalue(L, 1); lua_remove(L, 1);  /* move it to stack top */
  }
  else {  /* try global value */
    lua_getglobal(L, filenames[inout]);
    if (!checkfile(L,-1))
      luaL_verror(L, "global variable `%.10s' is not a valid file handle",
                  filenames[inout]);
    p = (FILE *)(lua_touserdata(L, -1));
  }
  return p;  /* leave handle at stack top to avoid GC */
}


static void newfile (lua_State *L, FILE *f) {
  lua_newuserdatabox(L, f);
  lua_settag(L, lua_name2tag(L, FILEHANDLE));
}


static void newfilewithname (lua_State *L, FILE *f, const char *name) {
  newfile(L, f);
  lua_setglobal(L, name);
}


static int setnewfile (lua_State *L, FILE *f, int inout) {
  if (f == NULL)
    return pushresult(L, 0);
  else {
    newfile(L, f);
    if (inout != NOFILE) {
      lua_pushvalue(L, -1);
      lua_setglobal(L, filenames[inout]);
    }
    return 1;
  }
}


static void resetfile (lua_State *L, int inout) {
  lua_getglobal(L, basicfiles[inout]);
  lua_setglobal(L, filenames[inout]);
}


static int io_close (lua_State *L) {
  FILE *f = (FILE *)(luaL_check_userdata(L, 1, FILEHANDLE));
  int status = 1;
  if (f != stdin && f != stdout && f != stderr) {
    lua_settop(L, 1);  /* make sure file is on top */
    lua_settag(L, lua_name2tag(L, CLOSEDFILEHANDLE));
    status = (CLOSEFILE(L, f) == 0);
  }
  return pushresult(L, status);
}


static int file_collect (lua_State *L) {
  FILE *f = (FILE *)(luaL_check_userdata(L, 1, FILEHANDLE));
  if (f != stdin && f != stdout && f != stderr)
    CLOSEFILE(L, f);
  return 0;
}


static int io_open (lua_State *L) {
  FILE *f = fopen(luaL_check_string(L, 1), luaL_check_string(L, 2));
  return setnewfile(L, f, NOFILE);
}


static int io_tmpfile (lua_State *L) {
  return setnewfile(L, tmpfile(), NOFILE);
}



static int io_fromto (lua_State *L, int inout, const char *mode) {
  FILE *current;
  if (lua_isnull(L, 1)) {
    getopthandle(L, inout);
    resetfile(L, inout);
    return io_close(L);
  }
  else {
    const char *s = luaL_check_string(L, 1);
    current = (*s == '|') ? popen(s+1, mode) : fopen(s, mode);
    return setnewfile(L, current, inout);
  }
}


static int io_readfrom (lua_State *L) {
  return io_fromto(L, INFILE, "r");
}


static int io_writeto (lua_State *L) {
  return io_fromto(L, OUTFILE, "w");
}


static int io_appendto (lua_State *L) {
  FILE *current = fopen(luaL_check_string(L, 1), "a");
  return setnewfile(L, current, OUTFILE);
}



/*
** {======================================================
** READ
** =======================================================
*/


#ifndef LUA_MAXUNTIL
#define LUA_MAXUNTIL	100
#endif


/*
** Knuth-Morris-Pratt algorithm for string searching
** (based on `Algorithms in MODULA-3', Robert Sedgewick;
**  Addison-Wesley, 1993.)
*/

static void prep_read_until (int next[], const char *p, int pl) {
  int i = 0;
  int j = -1;
  next[0] = -1;
  while (i < pl) {
    if (j == -1 || p[i] == p[j]) {
      i++; j++; next[i] = j;
    }
    else j = next[j];
  }
}


static int read_until (lua_State *L, FILE *f, const char *p, int pl) {
  int c;
  int j;
  int next[LUA_MAXUNTIL+1];
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  prep_read_until(next, p, pl);
  j = 0;
  while ((c = getc(f)) != EOF) {
  NoRead:
    if (c == p[j]) {
      j++;  /* go to next char in pattern */
      if (j == pl) {  /* complete match? */
        luaL_pushresult(&b);  /* close buffer */
        return 1;  /* always success */
      }
    }
    else if (j == 0)
      luaL_putchar(&b, c);
    else {  /* match fail */
      luaL_addlstring(&b, p, j - next[j]);  /* put failed part on result */
      j = next[j];  /* backtrack pattern index */
      goto NoRead;  /* repeat without reading next char */
    }
  }
  /* end of file without a match */
  luaL_addlstring(&b, p, j);  /* put failed part on result */
  luaL_pushresult(&b);  /* close buffer */
  return (lua_strlen(L, -1) > 0);
}


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


static int io_read (lua_State *L) {
  FILE *f = getopthandle(L, INFILE);
  int nargs = lua_gettop(L) - 1;
  int success;
  int n;
  if (nargs == 0) {  /* no arguments? */
    success = read_until(L, f, "\n", 1);  /* read until \n (a line) */
    n = 2;  /* will return n-1 results */
  }
  else {  /* ensure stack space for all results and for auxlib's buffer */
    luaL_check_stack(L, nargs+LUA_MINSTACK, "too many arguments");
    success = 1;
    for (n = 1; n<=nargs && success; n++) {
      if (lua_rawtag(L, n) == LUA_TNUMBER) {
        size_t l = (size_t)lua_tonumber(L, n);
        success = (l == 0) ? test_eof(L, f) : read_chars(L, f, l);
      }
      else {
        const char *p = lua_tostring(L, n);
        if (!p || p[0] != '*')
          lua_error(L, "invalid `read' option");
        switch (p[1]) {
          case 'n':  /* number */
            success = read_number(L, f);
            break;
          case 'l':  /* line */
            success = read_until(L, f, "\n", 1);  /* read until \n */
            break;
          case 'a':  /* file */
            read_chars(L, f, ~((size_t)0));  /* read MAX_SIZE_T chars */
            success = 1; /* always success */
            break;
          case 'w':  /* word */
            lua_error(L, "obsolete option `*w'");
            break;
          case 'u': {  /* read until */
            size_t pl = lua_strlen(L, n) - 2;
            luaL_arg_check(L, 0 < pl && pl <= LUA_MAXUNTIL, n,
                              "invalid read-until length");
            success = read_until(L, f, p+2, (int)(pl));
            break;
          }
          default:
            luaL_argerror(L, n, "invalid format");
            success = 0;  /* to avoid warnings */
        }
      }
    }
  }
  if (!success) {
    lua_pop(L, 1);  /* remove last result */
    lua_pushnil(L);  /* push nil instead */
  }
  return n - 1;
}

/* }====================================================== */


static int io_write (lua_State *L) {
  FILE *f = getopthandle(L, OUTFILE);
  int nargs = lua_gettop(L)-1;
  int arg;
  int status = 1;
  for (arg=1; arg<=nargs; arg++) {
    if (lua_rawtag(L, arg) == LUA_TNUMBER) {
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


static int io_seek (lua_State *L) {
  static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  FILE *f = (FILE *)(luaL_check_userdata(L, 1, FILEHANDLE));
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
  FILE *f = (lua_isnull(L, 1)) ? (FILE *)(NULL) :
                                 (FILE *)(luaL_check_userdata(L, 1, FILEHANDLE));
  return pushresult(L, fflush(f) == 0);
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
    lua_error(L, "unable to generate a unique filename");
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


static int getfield (lua_State *L, const char *key, int d) {
  int res;
  lua_pushstring(L, key);
  lua_rawget(L, -2);
  if (lua_isnumber(L, -1))
    res = (int)(lua_tonumber(L, -1));
  else {
    if (d == -2)
      luaL_verror(L, "field `%.20s' missing in date table", key);
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
    setfield(L, "isdst", stm->tm_isdst);
  }
  else {
    char b[256];
    if (strftime(b, sizeof(b), s, stm))
      lua_pushstring(L, b);
    else
      lua_error(L, "invalid `date' format");
  }
  return 1;
}


static int io_time (lua_State *L) {
  if (lua_isnull(L, 1))  /* called without args? */
    lua_pushnumber(L, time(NULL));  /* return current time */
  else {
    time_t t;
    struct tm ts;
    luaL_check_rawtype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);  /* make sure table is at the top */
    ts.tm_sec = getfield(L, "sec", 0);
    ts.tm_min = getfield(L, "min", 0);
    ts.tm_hour = getfield(L, "hour", 12);
    ts.tm_mday = getfield(L, "day", -2);
    ts.tm_mon = getfield(L, "month", -2)-1;
    ts.tm_year = getfield(L, "year", -2)-1900;
    ts.tm_isdst = getfield(L, "isdst", -1);
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
  int op = luaL_findstring(luaL_opt_string(L, 2, "all"), catnames);
  luaL_arg_check(L, op != -1, 2, "invalid option");
  lua_pushstring(L, setlocale(cat[op], luaL_check_string(L, 1)));
  return 1;
}


static int io_exit (lua_State *L) {
  exit(luaL_opt_int(L, 1, EXIT_SUCCESS));
  return 0;  /* to avoid warnings */
}

/* }====================================================== */



static int io_debug (lua_State *L) {
  for (;;) {
    char buffer[250];
    fprintf(stderr, "lua_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    lua_dostring(L, buffer);
    lua_settop(L, 0);  /* remove eventual returns */
  }
}


#define LEVELS1	12	/* size of the first part of the stack */
#define LEVELS2	10	/* size of the second part of the stack */

static int errorfb (lua_State *L) {
  int level = 1;  /* skip level 0 (it's this function) */
  int firstpart = 1;  /* still before eventual `...' */
  lua_Debug ar;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  luaL_addstring(&b, "error: ");
  luaL_addstring(&b, luaL_check_string(L, 1));
  luaL_addstring(&b, "\n");
  while (lua_getstack(L, level++, &ar)) {
    char buff[120];  /* enough to fit following `sprintf's */
    if (level == 2)
      luaL_addstring(&b, "stack traceback:\n");
    else if (level > LEVELS1 && firstpart) {
      /* no more than `LEVELS2' more levels? */
      if (!lua_getstack(L, level+LEVELS2, &ar))
        level--;  /* keep going */
      else {
        luaL_addstring(&b, "       ...\n");  /* too many levels */
        while (lua_getstack(L, level+LEVELS2, &ar))  /* find last levels */
          level++;
      }
      firstpart = 0;
      continue;
    }
    sprintf(buff, "%4d:  ", level-1);
    luaL_addstring(&b, buff);
    lua_getinfo(L, "Snl", &ar);
    switch (*ar.namewhat) {
      case 'g':  case 'l':  /* global, local */
        sprintf(buff, "function `%.50s'", ar.name);
        break;
      case 'f':  /* field */
        sprintf(buff, "method `%.50s'", ar.name);
        break;
      case 't':  /* tag method */
        sprintf(buff, "`%.50s' tag method", ar.name);
        break;
      default: {
        if (*ar.what == 'm')  /* main? */
          sprintf(buff, "main of %.70s", ar.short_src);
        else if (*ar.what == 'C')  /* C function? */
          sprintf(buff, "%.70s", ar.short_src);
        else
          sprintf(buff, "function <%d:%.70s>", ar.linedefined, ar.short_src);
        ar.source = NULL;  /* do not print source again */
      }
    }
    luaL_addstring(&b, buff);
    if (ar.currentline > 0) {
      sprintf(buff, " at line %d", ar.currentline);
      luaL_addstring(&b, buff);
    }
    if (ar.source) {
      sprintf(buff, " [%.70s]", ar.short_src);
      luaL_addstring(&b, buff);
    }
    luaL_addstring(&b, "\n");
  }
  luaL_pushresult(&b);
  lua_getglobal(L, LUA_ALERT);
  if (lua_isfunction(L, -1)) {  /* avoid loop if _ALERT is not defined */
    lua_pushvalue(L, -2);  /* error message */
    lua_rawcall(L, 1, 0);
  }
  return 0;
}



static const luaL_reg iolib[] = {
  {"appendto",  io_appendto},
  {"clock",     io_clock},
  {"closefile", io_close},
  {"date",      io_date},
  {"debug",     io_debug},
  {"difftime",  io_difftime},
  {"execute",   io_execute},
  {"exit",      io_exit},
  {"flush",     io_flush},
  {"getenv",    io_getenv},
  {"openfile",  io_open},
  {"read",      io_read},
  {"readfrom",  io_readfrom},
  {"remove",    io_remove},
  {"rename",    io_rename},
  {"seek",      io_seek},
  {"setlocale", io_setloc},
  {"time",      io_time},
  {"tmpfile",   io_tmpfile},
  {"tmpname",   io_tmpname},
  {"write",     io_write},
  {"writeto",   io_writeto},
  {LUA_ERRORMESSAGE, errorfb}
};


LUALIB_API int lua_iolibopen (lua_State *L) {
  int iotag = lua_newtype(L, FILEHANDLE, LUA_TUSERDATA);
  lua_newtype(L, CLOSEDFILEHANDLE, LUA_TUSERDATA);
  luaL_openl(L, iolib);
  /* predefined file handles */
  newfilewithname(L, stdin, basicfiles[INFILE]);
  newfilewithname(L, stdout, basicfiles[OUTFILE]);
  newfilewithname(L, stderr, "_STDERR");
  resetfile(L, INFILE);
  resetfile(L, OUTFILE);
  /* close files when collected */
  lua_pushcfunction(L, file_collect);
  lua_settagmethod(L, iotag, "gc");
  return 0;
}

