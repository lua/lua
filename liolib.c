/*
** $Id: liolib.c,v 1.109 2001/02/23 17:28:12 roberto Exp roberto $
** Standard I/O (and system) library
** See Copyright Notice in lua.h
*/


#include <ctype.h>
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


#ifndef l_realloc
#define l_malloc(s)		malloc(s)
#define l_realloc(b,os,s)	realloc(b, s)
#define l_free(b, os)		free(b)
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

#define FILEHANDLE	l_s("FileHandle")


static const l_char *const filenames[] = {l_s("_INPUT"), l_s("_OUTPUT")};


static int pushresult (lua_State *L, int i) {
  if (i) {
    lua_pushuserdata(L, NULL);
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


#define checkfile(L,f)	(strcmp(lua_xtype(L,(f)), FILEHANDLE) == 0)


static FILE *getopthandle (lua_State *L, int inout) {
  FILE *p = (FILE *)lua_touserdata(L, 1);
  if (p != NULL) {  /* is it a userdata ? */
    if (!checkfile(L, 1)) {
      if (strcmp(lua_xtype(L, 1), l_s("ClosedFileHandle")) == 0)
        luaL_argerror(L, 1, l_s("file is closed"));
      else
        luaL_argerror(L, 1, l_s("(invalid value)"));
    }
    /* move it to stack top */
    lua_pushvalue(L, 1); lua_remove(L, 1);
  }
  else if (inout != NOFILE) {  /* try global value */
    lua_getglobal(L, filenames[inout]);
    if (!checkfile(L,-1))
      luaL_verror(L, l_s("global variable `%.10s' is not a valid file handle"),
                  filenames[inout]);
    p = (FILE *)lua_touserdata(L, -1);
  }
  return p;  /* leave handle at stack top to avoid GC */
}


static void pushfile (lua_State *L, FILE *f) {
  lua_pushusertag(L, f, lua_type2tag(L, FILEHANDLE));
}


static void setfilebyname (lua_State *L, FILE *f, const l_char *name) {
  pushfile(L, f);
  lua_setglobal(L, name);
}


#define setfile(L,f,inout)	(setfilebyname(L,f,filenames[inout]))


static int setreturn (lua_State *L, FILE *f, int inout) {
  if (f == NULL)
    return pushresult(L, 0);
  else {
    if (inout != NOFILE)
      setfile(L, f, inout);
    pushfile(L, f);
    return 1;
  }
}


static int closefile (lua_State *L, FILE *f) {
  if (f == stdin || f == stdout || f == stderr)
    return 1;
  else {
    lua_pushuserdata(L, f);
    lua_settag(L, lua_type2tag(L, l_s("ClosedFileHandle")));
    return (CLOSEFILE(L, f) == 0);
  }
}


static int io_close (lua_State *L) {
  FILE *f = (FILE *)luaL_check_userdata(L, 1, FILEHANDLE);
  return pushresult(L, closefile(L, f));
}


static int file_collect (lua_State *L) {
  FILE *f = (FILE *)luaL_check_userdata(L, 1, FILEHANDLE);
  if (f != stdin && f != stdout && f != stderr)
    CLOSEFILE(L, f);
  return 0;
}


static int io_open (lua_State *L) {
  FILE *f = fopen(luaL_check_string(L, 1), luaL_check_string(L, 2));
  return setreturn(L, f, NOFILE);
}


static int io_tmpfile (lua_State *L) {
  return setreturn(L, tmpfile(), NOFILE);
}



static int io_fromto (lua_State *L, int inout, const l_char *mode) {
  FILE *current;
  if (lua_isnull(L, 1)) {
    closefile(L, getopthandle(L, inout));
    current = (inout == 0) ? stdin : stdout;    
  }
  else if (checkfile(L, 1))  /* deprecated option */
    current = (FILE *)lua_touserdata(L, 1);
  else {
    const l_char *s = luaL_check_string(L, 1);
    current = (*s == l_c('|')) ? popen(s+1, mode) : fopen(s, mode);
  }
  return setreturn(L, current, inout);
}


static int io_readfrom (lua_State *L) {
  return io_fromto(L, INFILE, l_s("r"));
}


static int io_writeto (lua_State *L) {
  return io_fromto(L, OUTFILE, l_s("w"));
}


static int io_appendto (lua_State *L) {
  FILE *current = fopen(luaL_check_string(L, 1), l_s("a"));
  return setreturn(L, current, OUTFILE);
}



/*
** {======================================================
** READ
** =======================================================
*/


static int read_number (lua_State *L, FILE *f) {
  double d;
  if (fscanf(f, l_s("%lf"), &d) == 1) {
    lua_pushnumber(L, d);
    return 1;
  }
  else return 0;  /* read fails */
}


static int read_word (lua_State *L, FILE *f) {
  l_charint c;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  do { c = fgetc(f); } while (isspace(c));  /* skip spaces */
  while (c != EOF && !isspace(c)) {
    luaL_putchar(&b, c);
    c = fgetc(f);
  }
  ungetc(c, f);
  luaL_pushresult(&b);  /* close buffer */
  return (lua_strlen(L, -1) > 0);
}


static int read_line (lua_State *L, FILE *f) {
  int n = 0;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (;;) {
    l_char *p = luaL_prepbuffer(&b);
    if (!fgets(p, LUAL_BUFFERSIZE, f))  /* read fails? */
      break;
    n = strlen(p);
    if (p[n-1] != l_c('\n'))
      luaL_addsize(&b, n); 
    else {
      luaL_addsize(&b, n-1);  /* do not add the `\n' */
      break;
    }
  }
  luaL_pushresult(&b);  /* close buffer */
  return (n > 0);  /* read something? */
}


static void read_file (lua_State *L, FILE *f) {
  size_t len = 0;
  size_t size = LUAL_BUFFERSIZE;
  size_t oldsize = 0;
  l_char *buffer = NULL;
  for (;;) {
    l_char *newbuffer = (l_char *)l_realloc(buffer, oldsize, size);
    if (newbuffer == NULL) {
      l_free(buffer, oldsize);
      lua_error(L, l_s("not enough memory to read a file"));
    }
    buffer = newbuffer;
    len += fread(buffer+len, sizeof(l_char), size-len, f);
    if (len < size) break;  /* did not read all it could */
    oldsize = size;
    size *= 2;
  }
  lua_pushlstring(L, buffer, len);
  l_free(buffer, size);
}


static int read_chars (lua_State *L, FILE *f, size_t n) {
  if (n == 0) {  /* test eof? */
    l_charint c = fgetc(f);
    ungetc(c, f);
    lua_pushlstring(L, NULL, 0);
    return (c != EOF);
  }
  else {
    l_char *buffer;
    size_t n1;
    l_char statbuff[LUAL_BUFFERSIZE];
    if (n <= LUAL_BUFFERSIZE)
      buffer = statbuff;
    else {
      buffer = (l_char  *)l_malloc(n);
      if (buffer == NULL)
        lua_error(L, l_s("not enough memory to read a file"));
    }
    n1 = fread(buffer, sizeof(l_char), n, f);
    lua_pushlstring(L, buffer, n1);
    if (buffer != statbuff) l_free(buffer, n);
    return (n1 > 0 || n == 0);
  }
}


static int io_read (lua_State *L) {
  FILE *f = getopthandle(L, INFILE);
  int nargs = lua_gettop(L)-1;
  int success;
  int n;
  if (nargs == 0) {  /* no arguments? */
    success = read_line(L, f);
    n = 2;  /* will return n-1 results */
  }
  else {  /* ensure stack space for all results and for auxlib's buffer */
    luaL_checkstack(L, nargs+LUA_MINSTACK, l_s("too many arguments"));
    success = 1;
    for (n = 1; n<=nargs && success; n++) {
      if (lua_type(L, n) == LUA_TNUMBER)
        success = read_chars(L, f, (size_t)lua_tonumber(L, n));
      else {
        const l_char *p = lua_tostring(L, n);
        if (!p || p[0] != l_c('*'))
          lua_error(L, l_s("invalid `read' option"));
        switch (p[1]) {
          case l_c('n'):  /* number */
            success = read_number(L, f);
            break;
          case l_c('l'):  /* line */
            success = read_line(L, f);
            break;
          case l_c('a'):  /* file */
            read_file(L, f);
            success = 1; /* always success */
            break;
          case l_c('w'):  /* word */
            success = read_word(L, f);
            break;
          default:
            luaL_argerror(L, n, l_s("invalid format"));
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
    if (lua_type(L, arg) == LUA_TNUMBER) {  /* LUA_NUMBER */
      /* optimization: could be done exactly as for strings */
      status = status && fprintf(f, l_s("%.16g"), lua_tonumber(L, arg)) > 0;
    }
    else {
      size_t l;
      const l_char *s = luaL_check_lstr(L, arg, &l);
      status = status && (fwrite(s, sizeof(l_char), l, f) == l);
    }
  }
  pushresult(L, status);
  return 1;
}


static int io_seek (lua_State *L) {
  static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static const l_char *const modenames[] = {l_s("set"), l_s("cur"), l_s("end"), NULL};
  FILE *f = (FILE *)luaL_check_userdata(L, 1, FILEHANDLE);
  int op = luaL_findstring(luaL_opt_string(L, 2, l_s("cur")), modenames);
  long offset = luaL_opt_long(L, 3, 0);
  luaL_arg_check(L, op != -1, 2, l_s("invalid mode"));
  op = fseek(f, offset, mode[op]);
  if (op)
    return pushresult(L, 0);  /* error */
  else {
    lua_pushnumber(L, ftell(f));
    return 1;
  }
}


static int io_flush (lua_State *L) {
  FILE *f = getopthandle(L, NOFILE);
  luaL_arg_check(L, f || lua_isnull(L, 1), 1, l_s("invalid file handle"));
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
  l_char buff[L_tmpnam];
  if (tmpnam(buff) != buff)
    lua_error(L, l_s("unable to generate a unique filename"));
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

static void setfield (lua_State *L, const l_char *key, int value) {
  lua_pushstring(L, key);
  lua_pushnumber(L, value);
  lua_rawset(L, -3);
}


static int getfield (lua_State *L, const l_char *key, int d) {
  int res;
  lua_pushstring(L, key);
  lua_rawget(L, -2);
  if (lua_isnumber(L, -1))
    res = (int)lua_tonumber(L, -1);
  else {
    if (d == -2)
      luaL_verror(L, l_s("field `%.20s' missing in date table"), key);
    res = d;
  }
  lua_pop(L, 1);
  return res;
}


static int io_date (lua_State *L) {
  const l_char *s = luaL_opt_string(L, 1, l_s("%c"));
  time_t t = (time_t)luaL_opt_number(L, 2, -1);
  struct tm *stm;
  if (t == (time_t)-1)  /* no time given? */
    t = time(NULL);  /* use current time */
  if (*s == l_c('!')) {  /* UTC? */
    stm = gmtime(&t);
    s++;  /* skip `!' */
  }
  else
    stm = localtime(&t);
  if (stm == NULL)  /* invalid date? */
    lua_pushnil(L);
  else if (strcmp(s, l_s("*t")) == 0) {
    lua_newtable(L);
    setfield(L, l_s("sec"), stm->tm_sec);
    setfield(L, l_s("min"), stm->tm_min);
    setfield(L, l_s("hour"), stm->tm_hour);
    setfield(L, l_s("day"), stm->tm_mday);
    setfield(L, l_s("month"), stm->tm_mon+1);
    setfield(L, l_s("year"), stm->tm_year+1900);
    setfield(L, l_s("wday"), stm->tm_wday+1);
    setfield(L, l_s("yday"), stm->tm_yday+1);
    setfield(L, l_s("isdst"), stm->tm_isdst);
  }
  else {
    l_char b[256];
    if (strftime(b, sizeof(b), s, stm))
      lua_pushstring(L, b);
    else
      lua_error(L, l_s("invalid `date' format"));
  }
  return 1;
}


static int io_time (lua_State *L) {
  if (lua_isnull(L, 1))  /* called without args? */
    lua_pushnumber(L, time(NULL));  /* return current time */
  else {
    time_t t;
    struct tm ts;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);  /* make sure table is at the top */
    ts.tm_sec = getfield(L, l_s("sec"), 0);
    ts.tm_min = getfield(L, l_s("min"), 0);
    ts.tm_hour = getfield(L, l_s("hour"), 12);
    ts.tm_mday = getfield(L, l_s("day"), -2);
    ts.tm_mon = getfield(L, l_s("month"), -2)-1;
    ts.tm_year = getfield(L, l_s("year"), -2)-1900;
    ts.tm_isdst = getfield(L, l_s("isdst"), -1);
    t = mktime(&ts);
    if (t == (time_t)-1)
      lua_pushnil(L);
    else
      lua_pushnumber(L, t);
  }
  return 1;
}


static int io_difftime (lua_State *L) {
  lua_pushnumber(L, difftime((time_t)luaL_check_number(L, 1),
                             (time_t)luaL_opt_number(L, 2, 0)));
  return 1;
}

/* }====================================================== */


static int io_setloc (lua_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const l_char *const catnames[] = {l_s("all"), l_s("collate"), l_s("ctype"), l_s("monetary"),
     l_s("numeric"), l_s("time"), NULL};
  int op = luaL_findstring(luaL_opt_string(L, 2, l_s("all")), catnames);
  luaL_arg_check(L, op != -1, 2, l_s("invalid option"));
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
    l_char buffer[250];
    fprintf(stderr, l_s("lua_debug> "));
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
        strcmp(buffer, l_s("cont\n")) == 0)
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
  luaL_addstring(&b, l_s("error: "));
  luaL_addstring(&b, luaL_check_string(L, 1));
  luaL_addstring(&b, l_s("\n"));
  while (lua_getstack(L, level++, &ar)) {
    l_char buff[120];  /* enough to fit following `sprintf's */
    if (level == 2)
      luaL_addstring(&b, l_s("stack traceback:\n"));
    else if (level > LEVELS1 && firstpart) {
      /* no more than `LEVELS2' more levels? */
      if (!lua_getstack(L, level+LEVELS2, &ar))
        level--;  /* keep going */
      else {
        luaL_addstring(&b, l_s("       ...\n"));  /* too many levels */
        while (lua_getstack(L, level+LEVELS2, &ar))  /* find last levels */
          level++;
      }
      firstpart = 0;
      continue;
    }
    sprintf(buff, l_s("%4d:  "), level-1);
    luaL_addstring(&b, buff);
    lua_getinfo(L, l_s("Snl"), &ar);
    switch (*ar.namewhat) {
      case l_c('g'):  case l_c('l'):  /* global, local */
        sprintf(buff, l_s("function `%.50s'"), ar.name);
        break;
      case l_c('f'):  /* field */
        sprintf(buff, l_s("method `%.50s'"), ar.name);
        break;
      case l_c('t'):  /* tag method */
        sprintf(buff, l_s("`%.50s' tag method"), ar.name);
        break;
      default: {
        if (*ar.what == l_c('m'))  /* main? */
          sprintf(buff, l_s("main of %.70s"), ar.short_src);
        else if (*ar.what == l_c('C'))  /* C function? */
          sprintf(buff, l_s("%.70s"), ar.short_src);
        else
          sprintf(buff, l_s("function <%d:%.70s>"), ar.linedefined, ar.short_src);
        ar.source = NULL;  /* do not print source again */
      }
    }
    luaL_addstring(&b, buff);
    if (ar.currentline > 0) {
      sprintf(buff, l_s(" at line %d"), ar.currentline);
      luaL_addstring(&b, buff);
    }
    if (ar.source) {
      sprintf(buff, l_s(" [%.70s]"), ar.short_src);
      luaL_addstring(&b, buff);
    }
    luaL_addstring(&b, l_s("\n"));
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
  {l_s("appendto"),  io_appendto},
  {l_s("clock"),     io_clock},
  {l_s("closefile"), io_close},
  {l_s("date"),      io_date},
  {l_s("debug"),     io_debug},
  {l_s("difftime"),  io_difftime},
  {l_s("execute"),   io_execute},
  {l_s("exit"),      io_exit},
  {l_s("flush"),     io_flush},
  {l_s("getenv"),    io_getenv},
  {l_s("openfile"),  io_open},
  {l_s("read"),      io_read},
  {l_s("readfrom"),  io_readfrom},
  {l_s("remove"),    io_remove},
  {l_s("rename"),    io_rename},
  {l_s("seek"),      io_seek},
  {l_s("setlocale"), io_setloc},
  {l_s("time"),      io_time},
  {l_s("tmpfile"),   io_tmpfile},
  {l_s("tmpname"),   io_tmpname},
  {l_s("write"),     io_write},
  {l_s("writeto"),   io_writeto},
  {LUA_ERRORMESSAGE, errorfb}
};


LUALIB_API int lua_iolibopen (lua_State *L) {
  int iotag = lua_newtype(L, FILEHANDLE, LUA_TUSERDATA);
  lua_newtype(L, l_s("ClosedFileHandle"), LUA_TUSERDATA);
  luaL_openl(L, iolib);
  /* predefined file handles */
  setfile(L, stdin, INFILE);
  setfile(L, stdout, OUTFILE);
  setfilebyname(L, stdin, l_s("_STDIN"));
  setfilebyname(L, stdout, l_s("_STDOUT"));
  setfilebyname(L, stderr, l_s("_STDERR"));
  /* close files when collected */
  lua_pushcfunction(L, file_collect);
  lua_settagmethod(L, iotag, l_s("gc"));
  return 0;
}

