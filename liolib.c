/*
** $Id: liolib.c,v 1.71 2000/08/22 17:47:17 roberto Exp roberto $
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
#else
/* no support for locale and for strerror: fake them */
#define setlocale(a,b)	((void)a, strcmp((b),"C")==0?"C":NULL)
#define LC_ALL		0
#define LC_COLLATE	0
#define LC_CTYPE	0
#define LC_MONETARY	0
#define LC_NUMERIC	0
#define LC_TIME		0
#define strerror(e)	"generic I/O error"
#define errno		(-1)
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

typedef struct IOCtrl {
  int ref[2];  /* ref for strings _INPUT/_OUTPUT */
  int iotag;    /* tag for file handles */
  int closedtag;  /* tag for closed handles */
} IOCtrl;



static const char *const filenames[] = {"_INPUT", "_OUTPUT"};


static int pushresult (lua_State *L, int i) {
  if (i) {
    lua_pushuserdata(L, NULL);
    return 1;
  }
  else {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    lua_pushnumber(L, errno);
    return 3;;
  }
}


/*
** {======================================================
** FILE Operations
** =======================================================
*/


static FILE *gethandle (lua_State *L, IOCtrl *ctrl, int f) {
  void *p = lua_touserdata(L, f);
  if (p != NULL) {  /* is `f' a userdata ? */
    int ftag = lua_tag(L, f);
    if (ftag == ctrl->iotag)  /* does it have the correct tag? */
      return (FILE *)p;
    else if (ftag == ctrl->closedtag)
      lua_error(L, "cannot access a closed file");
    /* else go through */
  }
  return NULL;
}


static FILE *getnonullfile (lua_State *L, IOCtrl *ctrl, int arg) {
  FILE *f = gethandle(L, ctrl, arg);
  luaL_arg_check(L, f, arg, "invalid file handle");
  return f;
}


static FILE *getfilebyref (lua_State *L, IOCtrl *ctrl, int inout) {
  FILE *f;
  lua_getglobals(L);
  lua_getref(L, ctrl->ref[inout]);
  lua_rawget(L);
  f = gethandle(L, ctrl, -1);
  lua_settop(L, -1);  /* remove global */
  if (f == NULL)
    luaL_verror(L, "global variable `%.10s' is not a file handle",
                filenames[inout]);
  return f;
}


static void setfilebyname (lua_State *L, IOCtrl *ctrl, FILE *f,
                           const char *name) {
  lua_pushusertag(L, f, ctrl->iotag);
  lua_setglobal(L, name);
}


#define setfile(L,ctrl,f,inout)	(setfilebyname(L,ctrl,f,filenames[inout]))


static int setreturn (lua_State *L, IOCtrl *ctrl, FILE *f, int inout) {
  if (f == NULL)
    return pushresult(L, 0);
  else {
    setfile(L, ctrl, f, inout);
    lua_pushusertag(L, f, ctrl->iotag);
    return 1;
  }
}


static int closefile (lua_State *L, IOCtrl *ctrl, FILE *f) {
  if (f == stdin || f == stdout || f == stderr)
    return 1;
  else {
    lua_pushusertag(L, f, ctrl->iotag);
    lua_settag(L, ctrl->closedtag);
    return (CLOSEFILE(L, f) == 0);
  }
}


static int io_close (lua_State *L) {
  IOCtrl *ctrl = (IOCtrl *)lua_touserdata(L, 1);
  return pushresult(L, closefile(L, ctrl, getnonullfile(L, ctrl, 2)));
}


static int file_collect (lua_State *L) {
  IOCtrl *ctrl = (IOCtrl *)lua_touserdata(L, 1);
  if (ctrl == (IOCtrl *)lua_touserdata(L, 2)) {
    /* collectig `ctrl' itself */
    lua_unref(L, ctrl->ref[INFILE]);
    lua_unref(L, ctrl->ref[OUTFILE]);
    free(ctrl);
  }
  else {  /* collecting a file: Close it */
    FILE *f = getnonullfile(L, ctrl, 2);
    if (f != stdin && f != stdout && f != stderr)
      CLOSEFILE(L, f);
  }
  return 0;
}


static int io_open (lua_State *L) {
  IOCtrl *ctrl = (IOCtrl *)lua_touserdata(L, 1);
  FILE *f = fopen(luaL_check_string(L, 2), luaL_check_string(L, 3));
  if (f) {
    lua_pushusertag(L, f, ctrl->iotag);
    return 1;
  }
  else
    return pushresult(L, 0);
}



static int io_fromto (lua_State *L, int inout, const char *mode) {
  IOCtrl *ctrl = (IOCtrl *)lua_touserdata(L, 1);
  FILE *current;
  if (lua_isnull(L, 2)) {
    closefile(L, ctrl, getfilebyref(L, ctrl, inout));
    current = (inout == 0) ? stdin : stdout;    
  }
  else if (lua_tag(L, 2) == ctrl->iotag)  /* deprecated option */
    current = (FILE *)lua_touserdata(L, 2);
  else {
    const char *s = luaL_check_string(L, 2);
    current = (*s == '|') ? popen(s+1, mode) : fopen(s, mode);
  }
  return setreturn(L, ctrl, current, inout);
}


static int io_readfrom (lua_State *L) {
  return io_fromto(L, INFILE, "r");
}


static int io_writeto (lua_State *L) {
  return io_fromto(L, OUTFILE, "w");
}


static int io_appendto (lua_State *L) {
  IOCtrl *ctrl = (IOCtrl *)lua_touserdata(L, 1);
  FILE *current = fopen(luaL_check_string(L, 2), "a");
  return setreturn(L, ctrl, current, OUTFILE);
}



/*
** {======================================================
** READ
** =======================================================
*/



#ifdef LUA_COMPAT_READPATTERN

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

#define read_pattern(L, f, p) (lua_error(L, "read patterns are deprecated"), 0)

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
  for (;;) {
    b = luaL_openspace(L, HUNK_LINE);
    if (!fgets(b, HUNK_LINE, f)) return 0;  /* read fails */
    n = strlen(b);
    if (b[n-1] != '\n')
      luaL_addsize(L, n); 
    else {
      luaL_addsize(L, n-1);  /* do not add the `\n' */
      break;
    }
  }
  return 1;
}


static void read_file (lua_State *L, FILE *f) {
  size_t n;
  do {
    char *b = luaL_openspace(L, HUNK_FILE);
    n = fread(b, sizeof(char), HUNK_FILE, f);
    luaL_addsize(L, n);
  } while (n==HUNK_FILE);
}


static int read_chars (lua_State *L, FILE *f, size_t n) {
  char *b = luaL_openspace(L, n);
  size_t n1 = fread(b, sizeof(char), n, f);
  luaL_addsize(L, n1);
  return (n == n1);
}


static int io_read (lua_State *L) {
  IOCtrl *ctrl = (IOCtrl *)lua_touserdata(L, 1);
  int lastarg = lua_gettop(L);
  int firstarg = 2;
  FILE *f = gethandle(L, ctrl, firstarg);
  int n = 0;
  if (f) firstarg++;
  else f = getfilebyref(L, ctrl, INFILE);  /* get _INPUT */
  do {  /* repeat for each part */
    size_t l;
    int success;
    luaL_resetbuffer(L);
    if (lua_isnumber(L, firstarg+n))
      success = read_chars(L, f, (size_t)lua_tonumber(L, firstarg+n));
    else {
      const char *p = luaL_opt_string(L, firstarg+n, "*l");
      if (p[0] != '*')
        success = read_pattern(L, f, p);  /* deprecated! */
      else {
        switch (p[1]) {
          case 'n':  /* number */
            if (!read_number(L, f)) return n;  /* read fails */
            n++;
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
            luaL_argerror(L, firstarg+n, "invalid format");
            success = 0;  /* to avoid warnings */
        }
      }
    }
    l = luaL_getsize(L);
    if (!success && l==0) return n;  /* read fails */
    lua_pushlstring(L, luaL_buffer(L), l);
    n++;
  } while (firstarg+n <= lastarg);
  return n;
}

/* }====================================================== */


static int io_write (lua_State *L) {
  int lastarg = lua_gettop(L);
  IOCtrl *ctrl = (IOCtrl *)lua_touserdata(L, 1);
  int arg = 2;
  int status = 1;
  FILE *f = gethandle(L, ctrl, arg);
  if (f) arg++;
  else f = getfilebyref(L, ctrl, OUTFILE);  /* get _OUTPUT */
  for (; arg <=  lastarg; arg++) {
    if (lua_type(L, arg)[2] == 'm') {  /* nuMber? */  /* LUA_NUMBER */
      /* optimization: could be done exactly as for strings */
      status = status && fprintf(f, "%.16g", lua_tonumber(L, arg)) > 0;
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
  IOCtrl *ctrl = (IOCtrl *)lua_touserdata(L, 1);
  FILE *f = getnonullfile(L, ctrl, 2);
  int op = luaL_findstring(luaL_opt_string(L, 3, "cur"), modenames);
  long offset = luaL_opt_long(L, 4, 0);
  luaL_arg_check(L, op != -1, 3, "invalid mode");
  op = fseek(f, offset, mode[op]);
  if (op)
    return pushresult(L, 0);  /* error */
  else {
    lua_pushnumber(L, ftell(f));
    return 1;
  }
}


static int io_flush (lua_State *L) {
  IOCtrl *ctrl = (IOCtrl *)lua_touserdata(L, 1);
  FILE *f = gethandle(L, ctrl, 2);
  luaL_arg_check(L, f || lua_isnull(L, 2), 2, "invalid file handle");
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
  lua_pushstring(L, tmpnam(NULL));
  return 1;
}



static int io_getenv (lua_State *L) {
  lua_pushstring(L, getenv(luaL_check_string(L, 1)));  /* if NULL push nil */
  return 1;
}


static int io_clock (lua_State *L) {
  lua_pushnumber(L, ((double)clock())/CLOCKS_PER_SEC);
  return 1;
}


static int io_date (lua_State *L) {
  char b[256];
  const char *s = luaL_opt_string(L, 1, "%c");
  struct tm *stm;
  time_t t;
  time(&t); stm = localtime(&t);
  if (strftime(b, sizeof(b), s, stm))
    lua_pushstring(L, b);
  else
    lua_error(L, "invalid `date' format");
  return 1;
}


static int setloc (lua_State *L) {
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



#define MESSAGESIZE	150
#define MAXMESSAGE (MESSAGESIZE*10)


static int errorfb (lua_State *L) {
  char buff[MAXMESSAGE];
  int level = 1;  /* skip level 0 (it's this function) */
  lua_Debug ar;
  sprintf(buff, "error: %.200s\n", lua_tostring(L, 1));
  while (lua_getstack(L, level++, &ar)) {
    char buffchunk[60];
    lua_getinfo(L, "Snl", &ar);
    luaL_chunkid(buffchunk, ar.source, sizeof(buffchunk));
    if (level == 2) strcat(buff, "stack traceback:\n");
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
  lua_getglobals(L);
  lua_pushstring(L, LUA_ALERT);
  lua_rawget(L);
  if (lua_isfunction(L, -1)) {  /* avoid loop if _ALERT is not defined */
    lua_pushstring(L, buff);
    lua_call(L, 1, 0);
  }
  return 0;
}



static const struct luaL_reg iolib[] = {
  {LUA_ERRORMESSAGE, errorfb},
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


static void openwithcontrol (lua_State *L) {
  IOCtrl *ctrl = (IOCtrl *)malloc(sizeof(IOCtrl));
  unsigned int i;
  int ctrltag = lua_newtag(L);
  ctrl->iotag = lua_newtag(L);
  ctrl->closedtag = lua_newtag(L);
  for (i=0; i<sizeof(iolibtag)/sizeof(iolibtag[0]); i++) {
    /* put `ctrl' as upvalue for these functions */
    lua_pushusertag(L, ctrl, ctrltag);
    lua_pushcclosure(L, iolibtag[i].func, 1);
    lua_setglobal(L, iolibtag[i].name);
  }
  /* create references to variable names */
  lua_pushstring(L, filenames[INFILE]);
  ctrl->ref[INFILE] = lua_ref(L, 1);
  lua_pushstring(L, filenames[OUTFILE]);
  ctrl->ref[OUTFILE] = lua_ref(L, 1);
  /* predefined file handles */
  setfile(L, ctrl, stdin, INFILE);
  setfile(L, ctrl, stdout, OUTFILE);
  setfilebyname(L, ctrl, stdin, "_STDIN");
  setfilebyname(L, ctrl, stdout, "_STDOUT");
  setfilebyname(L, ctrl, stderr, "_STDERR");
  /* delete `ctrl' when collected */
  lua_pushusertag(L, ctrl, ctrltag);
  lua_pushcclosure(L, file_collect, 1); 
  lua_settagmethod(L, ctrltag, "gc");
  /* close files when collected */
  lua_copytagmethods(L, ctrl->iotag, ctrltag);
}


void lua_iolibopen (lua_State *L) {
  luaL_openl(L, iolib);
  openwithcontrol(L);
}

