/*
** $Id: liolib.c,v 1.29 1999/01/04 12:41:12 roberto Exp roberto $
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


#define CLOSEDTAG	2
#define IOTAG		1

#define FIRSTARG      3  /* 1st and 2nd are upvalues */

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

static int gettag (int i) {
  return (int)lua_getnumber(lua_getparam(i));
}


static int ishandler (lua_Object f) {
  if (lua_isuserdata(f)) {
    if (lua_tag(f) == gettag(CLOSEDTAG))
      lua_error("cannot access a closed file");
    return lua_tag(f) == gettag(IOTAG);
  }
  else return 0;
}

static FILE *getfilebyname (char *name) {
  lua_Object f = lua_getglobal(name);
  if (!ishandler(f))
      luaL_verror("global variable `%.50s' is not a file handle", name);
  return lua_getuserdata(f);
}


static FILE *getfile (int arg) {
  lua_Object f = lua_getparam(arg);
  return (ishandler(f)) ? lua_getuserdata(f) : NULL;
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


static void getmode (char mode, char *m) {
  m[0] = mode;
  if (*luaL_opt_string(FIRSTARG+1, "text") == 'b') {
     m[1] = 'b';
     m[2] = '\0';
  }
  else m[1] = '\0';
}


static void closefile (char *name) {
  FILE *f = getfilebyname(name);
  if (f == stdin || f == stdout) return;
  if (pclose(f) == -1)
    fclose(f);
  lua_pushobject(lua_getglobal(name));
  lua_settag(gettag(CLOSEDTAG));
}


static void setfile (FILE *f, char *name, int tag) {
  lua_pushusertag(f, tag);
  lua_setglobal(name);
}


static void setreturn (FILE *f, char *name) {
  int tag = gettag(IOTAG);
  setfile(f, name, tag);
  lua_pushusertag(f, tag);
}


static void io_readfrom (void) {
  FILE *current;
  lua_Object f = lua_getparam(FIRSTARG);
  if (f == LUA_NOOBJECT) {
    closefile(FINPUT);
    current = stdin;
  }
  else if (lua_tag(f) == gettag(IOTAG))
    current = lua_getuserdata(f);
  else {
    char *s = luaL_check_string(FIRSTARG);
    char m[MODESIZE];
    getmode('r', m);
    current = (*s == '|') ? popen(s+1, "r") : fopen(s, m);
    if (current == NULL) {
      pushresult(0);
      return;
    }
  }
  setreturn(current, FINPUT);
}


static void io_writeto (void) {
  FILE *current;
  lua_Object f = lua_getparam(FIRSTARG);
  if (f == LUA_NOOBJECT) {
    closefile(FOUTPUT);
    current = stdout;
  }
  else if (lua_tag(f) == gettag(IOTAG))
    current = lua_getuserdata(f);
  else {
    char *s = luaL_check_string(FIRSTARG);
    char m[MODESIZE];
    getmode('w', m);
    current = (*s == '|') ? popen(s+1,"w") : fopen(s, m);
    if (current == NULL) {
      pushresult(0);
      return;
    }
  }
  setreturn(current, FOUTPUT);
}


static void io_appendto (void) {
  char *s = luaL_check_string(FIRSTARG);
  char m[MODESIZE];
  FILE *fp;
  getmode('a', m);
  fp = fopen (s, m);
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
  int arg = FIRSTARG;
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
  int arg = FIRSTARG;
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
  FILE *f = getfile(FIRSTARG-1+1);
  int op = luaL_findstring(luaL_opt_string(FIRSTARG-1+2, "cur"), modenames);
  long offset = luaL_opt_long(FIRSTARG-1+3, 0);
  luaL_arg_check(f, FIRSTARG-1+1, "invalid file handler");
  luaL_arg_check(op != -1, FIRSTARG-1+2, "invalid mode");
  op = fseek(f, offset, mode[op]);
  if (op)
    pushresult(0);  /* error */
  else
    lua_pushnumber(ftell(f));
}


static void io_flush (void) {
  FILE *f = getfile(FIRSTARG);
  luaL_arg_check(f || lua_getparam(FIRSTARG) == LUA_NOOBJECT, FIRSTARG,
                 "invalid file handler");
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
    if (fgets(buffer, sizeof(buffer), stdin) == 0) return;
    if (strcmp(buffer, "cont\n") == 0) return;
    lua_dostring(buffer);
  }
}



#define MESSAGESIZE	150
#define MAXMESSAGE	(MESSAGESIZE*10)

static void errorfb (void) {
  char buff[MAXMESSAGE];
  int level = 1;  /* skip level 0 (it's this function) */
  lua_Object func;
  sprintf(buff, "lua: %.200s\n", lua_getstring(lua_getparam(1)));
  while ((func = lua_stackedfunction(level++)) != LUA_NOOBJECT) {
    char *name;
    int currentline;
    char *chunkname;
    int linedefined;
    lua_funcinfo(func, &chunkname, &linedefined);
    if (level == 2) strcat(buff, "Active Stack:\n");
    strcat(buff, "\t");
    if (strlen(buff) > MAXMESSAGE-MESSAGESIZE) {
      strcat(buff, "...\n");
      break;  /* buffer is full */
    }
    switch (*lua_getobjname(func, &name)) {
      case 'g':
        sprintf(buff+strlen(buff), "function %.50s", name);
        break;
      case 't':
        sprintf(buff+strlen(buff), "`%.50s' tag method", name);
        break;
      default: {
        if (linedefined == 0)
          sprintf(buff+strlen(buff), "main of %.50s", chunkname);
        else if (linedefined < 0)
          sprintf(buff+strlen(buff), "%.50s", chunkname);
        else
          sprintf(buff+strlen(buff), "function (%.50s:%d)",
                  chunkname, linedefined);
        chunkname = NULL;
      }
    }
    if ((currentline = lua_currentline(func)) > 0)
      sprintf(buff+strlen(buff), " at line %d", currentline);
    if (chunkname)
      sprintf(buff+strlen(buff), " [in chunk %.50s]", chunkname);
    strcat(buff, "\n");
  }
  lua_pushstring(buff);
  lua_call("_ALERT");
}



static struct luaL_reg iolib[] = {
  {"setlocale", setloc},
  {"execute",  io_execute},
  {"remove",   io_remove},
  {"rename",   io_rename},
  {"tmpname",   io_tmpname},
  {"getenv",   io_getenv},
  {"date",     io_date},
  {"clock",     io_clock},
  {"exit",     io_exit},
  {"debug",    io_debug},
  {"_ERRORMESSAGE", errorfb}
};

static struct luaL_reg iolibtag[] = {
  {"readfrom", io_readfrom},
  {"writeto",  io_writeto},
  {"appendto", io_appendto},
  {"flush",     io_flush},
  {"read",     io_read},
  {"seek",     io_seek},
  {"write",    io_write}
};

static void openwithtags (void) {
  int iotag = lua_newtag();
  int closedtag = lua_newtag();
  int i;
  for (i=0; i<sizeof(iolibtag)/sizeof(iolibtag[0]); i++) {
    /* put both tags as upvalues for these functions */
    lua_pushnumber(iotag);
    lua_pushnumber(closedtag);
    lua_pushcclosure(iolibtag[i].func, 2);
    lua_setglobal(iolibtag[i].name);
  }
  setfile(stdin, FINPUT, iotag);
  setfile(stdout, FOUTPUT, iotag);
  setfile(stdin, "_STDIN", iotag);
  setfile(stdout, "_STDOUT", iotag);
  setfile(stderr, "_STDERR", iotag);
}

void lua_iolibopen (void) {
  luaL_openlib(iolib, (sizeof(iolib)/sizeof(iolib[0])));
  openwithtags();
}

