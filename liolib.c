/*
** $Id: liolib.c,v 1.56 2000/01/19 12:00:45 roberto Exp roberto $
** Standard I/O (and system) library
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LUA_REENTRANT

#include "lauxlib.h"
#include "lua.h"
#include "luadebug.h"
#include "lualib.h"


#ifndef OLD_ANSI
#include <locale.h>
#else
/* no support for locale and for strerror: fake them */
#define setlocale(a,b)	((void)a, strcmp((b),"C")==0?"C":NULL)
#define LC_ALL		0
#define LC_COLLATE	0
#define LC_CTYPE	0
#define LC_MONETARY	0
#define LC_NUMERIC	0
#define LC_TIME		0
#define strerror(e)	"(no error message provided by operating system)"
#endif


#define IOTAG	1

#define FIRSTARG	2  /* 1st is upvalue */

#define CLOSEDTAG(L, tag)	((tag)-1)  /* assume that CLOSEDTAG = iotag-1 */


#define FINPUT		"_INPUT"
#define FOUTPUT		"_OUTPUT"


#ifdef POPEN
/* FILE *popen();
int pclose(); */
#define CLOSEFILE(L, f)    ((pclose(f) == -1) ? fclose(f) : 0)
#else
/* no support for popen */
#define popen(x,y) NULL  /* that is, popen always fails */
#define CLOSEFILE(L, f)    (fclose(f))
#endif



static void pushresult (lua_State *L, int i) {
  if (i)
    lua_pushuserdata(L, NULL);
  else {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    lua_pushnumber(L, errno);
  }
}


/*
** {======================================================
** FILE Operations
** =======================================================
*/

static int gettag (lua_State *L) {
  return (int)lua_getnumber(L, lua_getparam(L, IOTAG));
}


static FILE *gethandle (lua_State *L, lua_Object f) {
  if (lua_isuserdata(L, f)) {
    int ftag = lua_tag(L, f);
    int iotag = gettag(L);
    if (ftag == iotag)
      return (FILE *)lua_getuserdata(L, f);
    if (ftag == CLOSEDTAG(L, iotag))
      lua_error(L, "cannot access a closed file");
    /* else go through */
  }
  return NULL;
}


static FILE *getfilebyname (lua_State *L, const char *name) {
  FILE *handle = gethandle(L, lua_rawgetglobal(L, name));
  if (!handle)
    luaL_verror(L, "global variable `%.50s' is not a file handle", name);
  return handle;
}


static FILE *getfile (lua_State *L, int arg) {
  return gethandle(L, lua_getparam(L, arg));
}


static FILE *getnonullfile (lua_State *L, int arg) {
  FILE *f = getfile(L, arg);
  luaL_arg_check(L, f, arg, "invalid file handle");
  return f;
}


static FILE *getfileparam (lua_State *L, const char *name, int *arg) {
  FILE *f = getfile(L, *arg);
  if (f) {
    (*arg)++;
    return f;
  }
  else
    return getfilebyname(L, name);
}


static int closefile (lua_State *L, FILE *f) {
  if (f == stdin || f == stdout)
    return 1;
  else {
    int tag = gettag(L);
    lua_pushusertag(L, f, tag);
    lua_settag(L, CLOSEDTAG(L, tag));
    return (CLOSEFILE(L, f) == 0);
  }
}


static void io_close (lua_State *L) {
  pushresult(L, closefile(L, getnonullfile(L, FIRSTARG)));
}


static void gc_close (lua_State *L) {
  FILE *f = getnonullfile(L, FIRSTARG);
  if (f != stdin && f != stdout && f != stderr) {
    CLOSEFILE(L, f);
  }
}


static void io_open (lua_State *L) {
  FILE *f = fopen(luaL_check_string(L, FIRSTARG), luaL_check_string(L, FIRSTARG+1));
  if (f) lua_pushusertag(L, f, gettag(L));
  else pushresult(L, 0);
}


static void setfile (lua_State *L, FILE *f, const char *name, int tag) {
  lua_pushusertag(L, f, tag);
  lua_setglobal(L, name);
}


static void setreturn (lua_State *L, FILE *f, const char *name) {
  if (f == NULL)
    pushresult(L, 0);
  else {
    int tag = gettag(L);
    setfile(L, f, name, tag);
    lua_pushusertag(L, f, tag);
  }
}


static void io_readfrom (lua_State *L) {
  FILE *current;
  lua_Object f = lua_getparam(L, FIRSTARG);
  if (f == LUA_NOOBJECT) {
    if (closefile(L, getfilebyname(L, FINPUT)))
      current = stdin;
    else
      current = NULL;  /* to signal error */
  }
  else if (lua_tag(L, f) == gettag(L))  /* deprecated option */
    current = (FILE *)lua_getuserdata(L, f);
  else {
    const char *s = luaL_check_string(L, FIRSTARG);
    current = (*s == '|') ? popen(s+1, "r") : fopen(s, "r");
  }
  setreturn(L, current, FINPUT);
}


static void io_writeto (lua_State *L) {
  FILE *current;
  lua_Object f = lua_getparam(L, FIRSTARG);
  if (f == LUA_NOOBJECT) {
    if (closefile(L, getfilebyname(L, FOUTPUT)))
      current = stdout;
    else
      current = NULL;  /* to signal error */
  }
  else if (lua_tag(L, f) == gettag(L))  /* deprecated option */
    current = (FILE *)lua_getuserdata(L, f);
  else {
    const char *s = luaL_check_string(L, FIRSTARG);
    current = (*s == '|') ? popen(s+1,"w") : fopen(s, "w");
  }
  setreturn(L, current, FOUTPUT);
}


static void io_appendto (lua_State *L) {
  FILE *current = fopen(luaL_check_string(L, FIRSTARG), "a");
  setreturn(L, current, FOUTPUT);
}



/*
** {======================================================
** READ
** =======================================================
*/



#ifdef COMPAT_READPATTERN

/*
** We cannot lookahead without need, because this can lock stdin.
** This flag signals when we need to read a next char.
*/
#define NEED_OTHER (EOF-1)  /* just some flag different from EOF */


static int read_pattern (lua_State *L, FILE *f, const char *p) {
  int inskip = 0;  /* {skip} level */
  int c = NEED_OTHER;
  while (*p != '\0') {
    switch (*p) {
      case '{':
        inskip++;
        p++;
        continue;
      case '}':
        if (!inskip) lua_error(L, "unbalanced braces in read pattern");
        inskip--;
        p++;
        continue;
      default: {
        const char *ep = luaI_classend(L, p);  /* get what is next */
        int m;  /* match result */
        if (c == NEED_OTHER) c = getc(f);
        m = (c==EOF) ? 0 : luaI_singlematch(c, p, ep);
        if (m) {
          if (!inskip) luaL_addchar(L, c);
          c = NEED_OTHER;
        }
        switch (*ep) {
          case '+':  /* repetition (1 or more) */
            if (!m) goto break_while;  /* pattern fails? */
            /* else go through */
          case '*':  /* repetition (0 or more) */
            while (m) {  /* reads the same item until it fails */
              c = getc(f);
              m = (c==EOF) ? 0 : luaI_singlematch(c, p, ep);
              if (m && !inskip) luaL_addchar(L, c);
            }
            /* go through to continue reading the pattern */
          case '?':  /* optional */
            p = ep+1;  /* continues reading the pattern */
            continue;
          default:
            if (!m) goto break_while;  /* pattern fails? */
            p = ep;  /* else continues reading the pattern */
        }
      }
    }
  } break_while:
  if (c != NEED_OTHER) ungetc(c, f);
  return (*p == '\0');
}

#else

#define read_pattern(L, f,p)   (lua_error(L, "read patterns are deprecated"), 0)

#endif


static int read_number (lua_State *L, FILE *f) {
  double d;
  if (fscanf(f, "%lf", &d) == 1) {
    lua_pushnumber(L, d);
    return 1;
  }
  else return 0;  /* read fails */
}


static void read_word (lua_State *L, FILE *f) {
  int c;
  do { c = fgetc(f); } while (isspace(c));  /* skip spaces */
  while (c != EOF && !isspace(c)) {
    luaL_addchar(L, c);
    c = fgetc(f);
  }
  ungetc(c, f);
}


#define HUNK_LINE	256
#define HUNK_FILE	BUFSIZ

static int read_line (lua_State *L, FILE *f) {
  int n;
  char *b;
  do {
    b = luaL_openspace(L, HUNK_LINE);
    if (!fgets(b, HUNK_LINE, f)) return 0;  /* read fails */
    n = strlen(b);
    luaL_addsize(L, n); 
  } while (b[n-1] != '\n');
  luaL_addsize(L, -1);  /* remove '\n' */
  return 1;
}


static void read_file (lua_State *L, FILE *f) {
  int n;
  do {
    char *b = luaL_openspace(L, HUNK_FILE);
    n = fread(b, sizeof(char), HUNK_FILE, f);
    luaL_addsize(L, n);
  } while (n==HUNK_FILE);
}


static int read_chars (lua_State *L, FILE *f, int n) {
  char *b = luaL_openspace(L, n);
  int n1 = fread(b, sizeof(char), n, f);
  luaL_addsize(L, n1);
  return (n == n1);
}


static void io_read (lua_State *L) {
  int arg = FIRSTARG;
  FILE *f = getfileparam(L, FINPUT, &arg);
  lua_Object op = lua_getparam(L, arg);
  do {  /* repeat for each part */
    long l;
    int success;
    luaL_resetbuffer(L);
    if (lua_isnumber(L, op))
      success = read_chars(L, f, (int)lua_getnumber(L, op));
    else {
      const char *p = luaL_opt_string(L, arg, "*l");
      if (p[0] != '*')
        success = read_pattern(L, f, p);  /* deprecated! */
      else {
        switch (p[1]) {
          case 'n':  /* number */
            if (!read_number(L, f)) return;  /* read fails */
            continue;  /* number is already pushed; avoid the "pushstring" */
          case 'l':  /* line */
            success = read_line(L, f);
            break;
          case 'a':  /* file */
            read_file(L, f);
            success = 1; /* always success */
            break;
          case 'w':  /* word */
            read_word(L, f);
            success = 0;  /* must read something to succeed */
            break;
          default:
            luaL_argerror(L, arg, "invalid format");
            success = 0;  /* to avoid warnings */
        }
      }
    }
    l = luaL_getsize(L);
    if (!success && l==0) return;  /* read fails */
    lua_pushlstring(L, luaL_buffer(L), l);
  } while ((op = lua_getparam(L, ++arg)) != LUA_NOOBJECT);
}

/* }====================================================== */


static void io_write (lua_State *L) {
  int arg = FIRSTARG;
  FILE *f = getfileparam(L, FOUTPUT, &arg);
  int status = 1;
  lua_Object o;
  while ((o = lua_getparam(L, arg)) != LUA_NOOBJECT) {
    if (lua_type(L, o)[2] == 'm') {  /* nuMber? */  /* LUA_NUMBER */
      /* optimization: could be done exactly as for strings */
      status = status && fprintf(f, "%.16g", lua_getnumber(L, o)) > 0;
    }
    else {
      long l;
      const char *s = luaL_check_lstr(L, arg, &l);
      status = status && ((long)fwrite(s, sizeof(char), l, f) == l);
    }
    arg++;
  }
  pushresult(L, status);
}


static void io_seek (lua_State *L) {
  static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  FILE *f = getnonullfile(L, FIRSTARG);
  int op = luaL_findstring(luaL_opt_string(L, FIRSTARG+1, "cur"), modenames);
  long offset = luaL_opt_long(L, FIRSTARG+2, 0);
  luaL_arg_check(L, op != -1, FIRSTARG+1, "invalid mode");
  op = fseek(f, offset, mode[op]);
  if (op)
    pushresult(L, 0);  /* error */
  else
    lua_pushnumber(L, ftell(f));
}


static void io_flush (lua_State *L) {
  FILE *f = getfile(L, FIRSTARG);
  luaL_arg_check(L, f || lua_getparam(L, FIRSTARG) == LUA_NOOBJECT, FIRSTARG,
                 "invalid file handle");
  pushresult(L, fflush(f) == 0);
}

/* }====================================================== */


/*
** {======================================================
** Other O.S. Operations
** =======================================================
*/

static void io_execute (lua_State *L) {
  lua_pushnumber(L, system(luaL_check_string(L, 1)));
}


static void io_remove (lua_State *L) {
  pushresult(L, remove(luaL_check_string(L, 1)) == 0);
}


static void io_rename (lua_State *L) {
  pushresult(L, rename(luaL_check_string(L, 1),
                    luaL_check_string(L, 2)) == 0);
}


static void io_tmpname (lua_State *L) {
  lua_pushstring(L, tmpnam(NULL));
}



static void io_getenv (lua_State *L) {
  lua_pushstring(L, getenv(luaL_check_string(L, 1)));  /* if NULL push nil */
}


static void io_clock (lua_State *L) {
  lua_pushnumber(L, ((double)clock())/CLOCKS_PER_SEC);
}


static void io_date (lua_State *L) {
  char b[256];
  const char *s = luaL_opt_string(L, 1, "%c");
  struct tm *stm;
  time_t t;
  time(&t); stm = localtime(&t);
  if (strftime(b, sizeof(b), s, stm))
    lua_pushstring(L, b);
  else
    lua_error(L, "invalid `date' format");
}


static void setloc (lua_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  int op = luaL_findstring(luaL_opt_string(L, 2, "all"), catnames);
  luaL_arg_check(L, op != -1, 2, "invalid option");
  lua_pushstring(L, setlocale(cat[op], luaL_check_string(L, 1)));
}


static void io_exit (lua_State *L) {
  exit(luaL_opt_int(L, 1, EXIT_SUCCESS));
}

/* }====================================================== */



static void io_debug (lua_State *L) {
  for (;;) {
    char buffer[250];
    fprintf(stderr, "lua_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
        strcmp(buffer, "cont\n") == 0)
      return;
    lua_dostring(L, buffer);
  }
}



#define MESSAGESIZE	150
#define MAXMESSAGE (MESSAGESIZE*10)


#define MAXSRC		60


static void errorfb (lua_State *L) {
  char buff[MAXMESSAGE];
  int level = 1;  /* skip level 0 (it's this function) */
  lua_Dbgactreg ar;
  lua_Object alertfunc = lua_rawgetglobal(L, "_ALERT");
  sprintf(buff, "error: %.200s\n", lua_getstring(L, lua_getparam(L, 1)));
  while (lua_getstack(L, level++, &ar)) {
    char buffchunk[MAXSRC];
    lua_getinfo(L, "Snl", &ar);
    luaL_chunkid(buffchunk, ar.source, sizeof(buffchunk));
    if (level == 2) strcat(buff, "Active Stack:\n");
    strcat(buff, "  ");
    if (strlen(buff) > MAXMESSAGE-MESSAGESIZE) {
      strcat(buff, "...\n");
      break;  /* buffer is full */
    }
    switch (*ar.namewhat) {
      case 'g':  case 'l':  /* global, local */
        sprintf(buff+strlen(buff), "function `%.50s'", ar.name);
        break;
      case 'f':  /* field */
        sprintf(buff+strlen(buff), "method `%.50s'", ar.name);
        break;
      case 't':  /* tag method */
        sprintf(buff+strlen(buff), "`%.50s' tag method", ar.name);
        break;
      default: {
        if (*ar.what == 'm')  /* main? */
          sprintf(buff+strlen(buff), "main of %.70s", buffchunk);
        else if (*ar.what == 'C')  /* C function? */
          sprintf(buff+strlen(buff), "%.70s", buffchunk);
        else
          sprintf(buff+strlen(buff), "function <%d:%.70s>",
                  ar.linedefined, buffchunk);
        ar.source = NULL;
      }
    }
    if (ar.currentline > 0)
      sprintf(buff+strlen(buff), " at line %d", ar.currentline);
    if (ar.source)
      sprintf(buff+strlen(buff), " [%.70s]", buffchunk);
    strcat(buff, "\n");
  }
  if (lua_isfunction(L, alertfunc)) {  /* avoid loop if _ALERT is not defined */
    lua_pushstring(L, buff);
    lua_callfunction(L, alertfunc);
  }
}



static const struct luaL_reg iolib[] = {
  {"_ERRORMESSAGE", errorfb},
  {"clock",     io_clock},
  {"date",     io_date},
  {"debug",    io_debug},
  {"execute",  io_execute},
  {"exit",     io_exit},
  {"getenv",   io_getenv},
  {"remove",   io_remove},
  {"rename",   io_rename},
  {"setlocale", setloc},
  {"tmpname",   io_tmpname}
};


static const struct luaL_reg iolibtag[] = {
  {"appendto", io_appendto},
  {"closefile",   io_close},
  {"flush",     io_flush},
  {"openfile",   io_open},
  {"read",     io_read},
  {"readfrom", io_readfrom},
  {"seek",     io_seek},
  {"write",    io_write},
  {"writeto",  io_writeto}
};


static void openwithtags (lua_State *L) {
  unsigned int i;
  int iotag = lua_newtag(L);
  lua_newtag(L);  /* alloc CLOSEDTAG: assume that CLOSEDTAG = iotag-1 */
  for (i=0; i<sizeof(iolibtag)/sizeof(iolibtag[0]); i++) {
    /* put iotag as upvalue for these functions */
    lua_pushnumber(L, iotag);
    lua_pushcclosure(L, iolibtag[i].func, 1);
    lua_setglobal(L, iolibtag[i].name);
  }
  /* predefined file handles */
  setfile(L, stdin, FINPUT, iotag);
  setfile(L, stdout, FOUTPUT, iotag);
  setfile(L, stdin, "_STDIN", iotag);
  setfile(L, stdout, "_STDOUT", iotag);
  setfile(L, stderr, "_STDERR", iotag);
  /* close file when collected */
  lua_pushnumber(L, iotag);
  lua_pushcclosure(L, gc_close, 1); 
  lua_settagmethod(L, iotag, "gc");
}

void lua_iolibopen (lua_State *L) {
  luaL_openl(L, iolib);
  openwithtags(L);
}

