/*
** $Id: liolib.c,v 1.35 1999/03/16 20:07:54 roberto Exp roberto $
** Standard I/O (and system) library
** See Copyright Notice in lua.h
*/


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lauxlib.h"
#include "lua.h"
#include "luadebug.h"
#include "lualib.h"


#ifndef OLD_ANSI
#include <locale.h>
#else
/* no support for locale and for strerror: fake them */
#define setlocale(a,b)	0
#define LC_ALL		0
#define LC_COLLATE	0
#define LC_CTYPE	0
#define LC_MONETARY	0
#define LC_NUMERIC	0
#define LC_TIME		0
#define strerror(e)	"(no error message provided by operating system)"
#endif


#define CLOSEDTAG(tag)	((tag)-1)


#define FINPUT		"_INPUT"
#define FOUTPUT		"_OUTPUT"

#define MODESIZE	3 /* string for file mode */

#ifdef POPEN
FILE *popen();
int pclose();
#else
/* no support for popen */
#define popen(x,y) NULL  /* that is, popen always fails */
#define pclose(x)  (-1)
#endif



static void pushresult (int i) {
  if (i)
    lua_pushuserdata(NULL);
  else {
    lua_pushnil();
    lua_pushstring(strerror(errno));
    lua_pushnumber(errno);
  }
}


/*
** {======================================================
** FILE Operations
** =======================================================
*/

static int gettag (void) {
  lua_pushusertag(stdin, LUA_ANYTAG);
  return lua_tag(lua_pop());  /* get the tag of stdin */
}


static int ishandle (lua_Object f) {
  if (lua_isuserdata(f)) {
    int tag = gettag();
    if (lua_tag(f) == CLOSEDTAG(tag))
      lua_error("cannot access a closed file");
    return lua_tag(f) == tag;
  }
  else return 0;
}


static FILE *getfilebyname (char *name) {
  lua_Object f = lua_getglobal(name);
  if (!ishandle(f))
      luaL_verror("global variable `%.50s' is not a file handle", name);
  return lua_getuserdata(f);
}


static FILE *getfile (int arg) {
  lua_Object f = lua_getparam(arg);
  return (ishandle(f)) ? lua_getuserdata(f) : NULL;
}


static FILE *getnonullfile (int arg) {
  FILE *f = getfile(arg);
  luaL_arg_check(f, arg, "invalid file handle");
  return f;
}


static FILE *getfileparam (char *name, int *arg) {
  FILE *f = getfile(*arg);
  if (f) {
    (*arg)++;
    return f;
  }
  else
    return getfilebyname(name);
}


static void rawclose (FILE *f) {
  if (f != stdin && f != stdout) {
    if (pclose(f) == -1) fclose(f);
  }
}


static void closefile (FILE *f) {
  if (f != stdin && f != stdout) {
    int tag = gettag();
    rawclose(f);
    lua_pushusertag(f, tag);
    lua_settag(CLOSEDTAG(tag));
  }
}


static void io_close (void) {
  closefile(getnonullfile(1));
}


static void gc_close (void) {
  int tag = luaL_check_int(1);
  lua_Object fh = lua_getparam(2);
  FILE *f = lua_getuserdata(fh);
  luaL_arg_check(lua_isuserdata(fh) && lua_tag(fh) == tag, 2,
                 "invalid file handle for GC");
  rawclose(f);
}


static void io_open (void) {
  FILE *f = fopen(luaL_check_string(1), luaL_check_string(2));
  if (f) lua_pushusertag(f, gettag());
  else pushresult(0);
}


static void setfile (FILE *f, char *name, int tag) {
  lua_pushusertag(f, tag);
  lua_setglobal(name);
}


static void setreturn (FILE *f, char *name) {
  int tag = gettag();
  setfile(f, name, tag);
  lua_pushusertag(f, tag);
}


static void io_readfrom (void) {
  FILE *current;
  lua_Object f = lua_getparam(1);
  if (f == LUA_NOOBJECT) {
    closefile(getfilebyname(FINPUT));
    current = stdin;
  }
  else if (lua_tag(f) == gettag())  /* deprecated option */
    current = lua_getuserdata(f);
  else {
    char *s = luaL_check_string(1);
    current = (*s == '|') ? popen(s+1, "r") : fopen(s, "r");
    if (current == NULL) {
      pushresult(0);
      return;
    }
  }
  setreturn(current, FINPUT);
}


static void io_writeto (void) {
  FILE *current;
  lua_Object f = lua_getparam(1);
  if (f == LUA_NOOBJECT) {
    closefile(getfilebyname(FOUTPUT));
    current = stdout;
  }
  else if (lua_tag(f) == gettag())  /* deprecated option */
    current = lua_getuserdata(f);
  else {
    char *s = luaL_check_string(1);
    current = (*s == '|') ? popen(s+1,"w") : fopen(s, "w");
    if (current == NULL) {
      pushresult(0);
      return;
    }
  }
  setreturn(current, FOUTPUT);
}


static void io_appendto (void) {
  FILE *fp = fopen(luaL_check_string(1), "a");
  if (fp != NULL)
    setreturn(fp, FOUTPUT);
  else
    pushresult(0);
}



/*
** {======================================================
** READ
** =======================================================
*/

static int read_pattern (FILE *f, char *p) {
  int inskip = 0;  /* {skip} level */
  int c = getc(f);
  while (*p != '\0') {
    switch (*p) {
      case '{':
        inskip++;
        p++;
        continue;
      case '}':
        if (!inskip) lua_error("unbalanced braces in read pattern");
        inskip--;
        p++;
        continue;
      default: {
        char *ep;  /* get what is next */
        int m;  /* match result */
        if (c != EOF)
          m = luaI_singlematch(c, p, &ep);
        else {
          luaI_singlematch(0, p, &ep);  /* to set "ep" */
          m = 0;  /* EOF matches no pattern */
        }
        if (m) {
          if (!inskip) luaL_addchar(c);
          c = getc(f);
        }
        switch (*ep) {
          case '*':  /* repetition */
            if (!m) p = ep+1;  /* else stay in (repeat) the same item */
            continue;
          case '?':  /* optional */
            p = ep+1;  /* continues reading the pattern */
            continue;
          default:
            if (!m) {  /* pattern fails? */
              ungetc(c, f);
              return 0;
            }
            p = ep;  /* continues reading the pattern */
        }
      }
    }
  }
  ungetc(c, f);
  return 1;
}


static int read_number (FILE *f) {
  double d;
  if (fscanf(f, "%lf", &d) == 1) {
    lua_pushnumber(d);
    return 1;
  }
  else return 0;  /* read fails */
}


#define HUNK_LINE	1024
#define HUNK_FILE	BUFSIZ

static int read_line (FILE *f) {
  /* equivalent to: return read_pattern(f, "[^\n]*{\n}"); */
  int n;
  char *b;
  do {
    b = luaL_openspace(HUNK_LINE);
    if (!fgets(b, HUNK_LINE, f)) return 0;  /* read fails */
    n = strlen(b);
    luaL_addsize(n); 
  } while (b[n-1] != '\n');
  luaL_addsize(-1);  /* remove '\n' */
  return 1;
}


static void read_file (FILE *f) {
  /* equivalent to: return read_pattern(f, ".*"); */
  int n;
  do {
    char *b = luaL_openspace(HUNK_FILE);
    n = fread(b, sizeof(char), HUNK_FILE, f);
    luaL_addsize(n); 
  } while (n==HUNK_FILE);
}


static void io_read (void) {
  static char *options[] = {"*n", "*l", "*a", ".*", "*w", NULL};
  int arg = 1;
  FILE *f = getfileparam(FINPUT, &arg);
  char *p = luaL_opt_string(arg++, "*l");
  do { /* repeat for each part */
    long l;
    int success;
    luaL_resetbuffer();
    switch (luaL_findstring(p, options)) {
      case 0:  /* number */
        if (!read_number(f)) return;  /* read fails */
        continue;  /* number is already pushed; avoid the "pushstring" */
      case 1:  /* line */
        success = read_line(f);
        break;
      case 2: case 3:  /* file */
        read_file(f);
        success = 1; /* always success */
        break;
      case 4:  /* word */
        success = read_pattern(f, "{%s*}%S%S*");
        break;
      default:
        success = read_pattern(f, p);
    }
    l = luaL_getsize();
    if (!success && l==0) return;  /* read fails */
    lua_pushlstring(luaL_buffer(), l);
  } while ((p = luaL_opt_string(arg++, NULL)) != NULL);
}

/* }====================================================== */


static void io_write (void) {
  int arg = 1;
  FILE *f = getfileparam(FOUTPUT, &arg);
  int status = 1;
  char *s;
  long l;
  while ((s = luaL_opt_lstr(arg++, NULL, &l)) != NULL)
    status = status && ((long)fwrite(s, 1, l, f) == l);
  pushresult(status);
}


static void io_seek (void) {
  static int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static char *modenames[] = {"set", "cur", "end", NULL};
  FILE *f = getnonullfile(1);
  int op = luaL_findstring(luaL_opt_string(2, "cur"), modenames);
  long offset = luaL_opt_long(3, 0);
  luaL_arg_check(op != -1, 2, "invalid mode");
  op = fseek(f, offset, mode[op]);
  if (op)
    pushresult(0);  /* error */
  else
    lua_pushnumber(ftell(f));
}


static void io_flush (void) {
  FILE *f = getfile(1);
  luaL_arg_check(f || lua_getparam(1) == LUA_NOOBJECT, 1,
                 "invalid file handle");
  pushresult(fflush(f) == 0);
}

/* }====================================================== */


/*
** {======================================================
** Other O.S. Operations
** =======================================================
*/

static void io_execute (void) {
  lua_pushnumber(system(luaL_check_string(1)));
}


static void io_remove  (void) {
  pushresult(remove(luaL_check_string(1)) == 0);
}


static void io_rename (void) {
  pushresult(rename(luaL_check_string(1),
                    luaL_check_string(2)) == 0);
}


static void io_tmpname (void) {
  lua_pushstring(tmpnam(NULL));
}



static void io_getenv (void) {
  lua_pushstring(getenv(luaL_check_string(1)));  /* if NULL push nil */
}


static void io_clock (void) {
  lua_pushnumber(((double)clock())/CLOCKS_PER_SEC);
}


static void io_date (void) {
  char b[256];
  char *s = luaL_opt_string(1, "%c");
  struct tm *tm;
  time_t t;
  time(&t); tm = localtime(&t);
  if (strftime(b,sizeof(b),s,tm))
    lua_pushstring(b);
  else
    lua_error("invalid `date' format");
}


static void setloc (void) {
  static int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY, LC_NUMERIC,
                      LC_TIME};
  static char *catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  int op = luaL_findstring(luaL_opt_string(2, "all"), catnames);
  luaL_arg_check(op != -1, 2, "invalid option");
  lua_pushstring(setlocale(cat[op], luaL_check_string(1)));
}


static void io_exit (void) {
  lua_Object o = lua_getparam(1);
  exit(lua_isnumber(o) ? (int)lua_getnumber(o) : 1);
}

/* }====================================================== */



static void io_debug (void) {
  for (;;) {
    char buffer[250];
    fprintf(stderr, "lua_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
        strcmp(buffer, "cont\n") == 0)
      return;
    lua_dostring(buffer);
  }
}



#define MESSAGESIZE	150
#define MAXMESSAGE	(MESSAGESIZE*10)


#define MAXSRC		40


static void errorfb (void) {
  char buff[MAXMESSAGE];
  int level = 1;  /* skip level 0 (it's this function) */
  lua_Object func;
  sprintf(buff, "lua error: %.200s\n", lua_getstring(lua_getparam(1)));
  while ((func = lua_stackedfunction(level++)) != LUA_NOOBJECT) {
    char *name;
    int currentline;
    char *chunkname;
    char buffchunk[MAXSRC];
    int linedefined;
    lua_funcinfo(func, &chunkname, &linedefined);
    luaL_chunkid(buffchunk, chunkname, sizeof(buffchunk));
    if (level == 2) strcat(buff, "Active Stack:\n");
    strcat(buff, "\t");
    if (strlen(buff) > MAXMESSAGE-MESSAGESIZE) {
      strcat(buff, "...\n");
      break;  /* buffer is full */
    }
    switch (*lua_getobjname(func, &name)) {
      case 'g':
        sprintf(buff+strlen(buff), "function `%.50s'", name);
        break;
      case 't':
        sprintf(buff+strlen(buff), "`%.50s' tag method", name);
        break;
      default: {
        if (linedefined == 0)
          sprintf(buff+strlen(buff), "main of %.50s", buffchunk);
        else if (linedefined < 0)
          sprintf(buff+strlen(buff), "%.50s", buffchunk);
        else
          sprintf(buff+strlen(buff), "function <%d:%.50s>",
                  linedefined, buffchunk);
        chunkname = NULL;
      }
    }
    if ((currentline = lua_currentline(func)) > 0)
      sprintf(buff+strlen(buff), " at line %d", currentline);
    if (chunkname)
      sprintf(buff+strlen(buff), " [%.50s]", buffchunk);
    strcat(buff, "\n");
  }
  func = lua_rawgetglobal("_ALERT");
  if (lua_isfunction(func)) {  /* avoid error loop if _ALERT is not defined */
    lua_pushstring(buff);
    lua_callfunction(func);
  }
}



static struct luaL_reg iolib[] = {
  {"_ERRORMESSAGE", errorfb},
  {"appendto", io_appendto},
  {"clock",     io_clock},
  {"closefile",   io_close},
  {"date",     io_date},
  {"debug",    io_debug},
  {"execute",  io_execute},
  {"exit",     io_exit},
  {"flush",     io_flush},
  {"getenv",   io_getenv},
  {"openfile",   io_open},
  {"read",     io_read},
  {"readfrom", io_readfrom},
  {"remove",   io_remove},
  {"rename",   io_rename},
  {"seek",     io_seek},
  {"setlocale", setloc},
  {"tmpname",   io_tmpname},
  {"write",    io_write},
  {"writeto",  io_writeto}
};


void lua_iolibopen (void) {
  int iotag = lua_newtag();
  lua_newtag();  /* alloc CLOSEDTAG: assume that CLOSEDTAG = iotag-1 */
  /* predefined file handles */
  setfile(stdin, FINPUT, iotag);
  setfile(stdout, FOUTPUT, iotag);
  setfile(stdin, "_STDIN", iotag);
  setfile(stdout, "_STDOUT", iotag);
  setfile(stderr, "_STDERR", iotag);
  /* make sure stdin (with its tag) won't be collected */
  lua_pushusertag(stdin, iotag); lua_ref(1);
  /* close file when collected */
  lua_pushnumber(iotag);
  lua_pushcclosure(gc_close, 1); 
  lua_settagmethod(iotag, "gc");
  /* register lib functions */
  luaL_openlib(iolib, (sizeof(iolib)/sizeof(iolib[0])));
}

