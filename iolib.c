#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#include "lua.h"
#include "luadebug.h"
#include "lualib.h"


FILE *lua_infile, *lua_outfile;


#ifdef POPEN
FILE *popen();
int pclose();
#else
#define popen(x,y) NULL  /* that is, popen always fails */
#define pclose(x)  (-1)
#endif


static void pushresult (int i)
{
  if (i)
    lua_pushuserdata(NULL);
  else {
    lua_pushnil();
#ifndef NOSTRERROR
    lua_pushstring(strerror(errno));
#else
    lua_pushstring("system unable to define the error");
#endif
  }
}


static void closefile (FILE *f)
{
  if (f == stdin || f == stdout)
    return;
  if (f == lua_infile)
    lua_infile = stdin;
  if (f == lua_outfile)
    lua_outfile = stdout;
  if (pclose(f) == -1)
    fclose(f);
}



static void io_readfrom (void)
{
  lua_Object f = lua_getparam(1);
  if (f == LUA_NOOBJECT)
    closefile(lua_infile);  /* restore standart input */
  else if (lua_isuserdata(f))
    lua_infile = lua_getuserdata(f);
  else {
    char *s = lua_check_string(1, "readfrom");
    FILE *fp = (*s == '|') ? popen(s+1, "r") : fopen(s, "r");
    if (fp)
      lua_infile = fp;
    else {
      pushresult(0);
      return;
    }
  }
  lua_pushuserdata(lua_infile);
}


static void io_writeto (void)
{
  lua_Object f = lua_getparam(1);
  if (f == LUA_NOOBJECT)
    closefile(lua_outfile);  /* restore standart output */
  else if (lua_isuserdata(f))
    lua_outfile = lua_getuserdata(f);
  else {
    char *s = lua_check_string(1, "writeto");
    FILE *fp = (*s == '|') ? popen(s+1,"w") : fopen(s,"w");
    if (fp)
      lua_outfile = fp;
    else {
      pushresult(0);
      return;
    }
  }
  lua_pushuserdata(lua_outfile);
}


static void io_appendto (void)
{
  char *s = lua_check_string(1, "appendto");
  FILE *fp = fopen (s, "a");
  if (fp != NULL) {
    lua_outfile = fp;
    lua_pushuserdata(lua_outfile);
  }
  else
    pushresult(0);
}


#define NEED_OTHER (EOF-1)  /* just some flag different from EOF */

static void io_read (void)
{
  char *buff;
  char *p = lua_opt_string(1, "[^\n]*{\n}", "read");
  int inskip = 0;  /* to control {skips} */
  int c = NEED_OTHER;
  luaI_addchar(0);
  while (*p) {
    if (*p == '{' || *p == '}') {
      inskip = (*p == '{');
      p++;
    }
    else {
      char *ep = item_end(p);  /* get what is next */
      int m;  /* match result */
      if (c == NEED_OTHER) c = getc(lua_infile);
      m = (c == EOF) ? 0 : singlematch((char)c, p);
      if (m) {
        if (!inskip) luaI_addchar(c);
        c = NEED_OTHER;
      }
      switch (*ep) {
        case '*':  /* repetition */
          if (!m) p = ep+1;  /* else stay in (repeat) the same item */
          break;
        case '?':  /* optional */
          p = ep+1;  /* continues reading the pattern */
          break;
        default:
          if (m) p = ep;  /* continues reading the pattern */
          else
            goto break_while;   /* pattern fails */
      }
    }
  } break_while:
  if (c >= 0)  /* not EOF nor NEED_OTHER? */
     ungetc(c, lua_infile);
  buff = luaI_addchar(0);
  if (*buff != 0 || *p == 0)  /* read something or did not fail? */
    lua_pushstring(buff);
}


static void io_write (void)
{
  int arg = 1;
  int status = 1;
  char *s;
  while ((s = lua_opt_string(arg++, NULL, "write")) != NULL)
    status = status && (fputs(s, lua_outfile) != EOF);
  pushresult(status);
}


static void io_execute (void)
{
  lua_pushnumber(system(lua_check_string(1, "execute")));
}


static void io_remove  (void)
{
  pushresult(remove(lua_check_string(1, "remove")) == 0);
}


static void io_rename (void)
{
  pushresult(rename(lua_check_string(1, "rename"),
                    lua_check_string(2, "rename")) == 0);
}


static void io_tmpname (void)
{
  lua_pushstring(tmpnam(NULL));
}



static void io_getenv (void)
{
  lua_pushstring(getenv(lua_check_string(1, "getenv"))); /* if NULL push nil */
}


static void io_date (void)
{
  time_t t;
  struct tm *tm;
  char *s = lua_opt_string(1, "%c", "date");
  char b[BUFSIZ];
  time(&t); tm = localtime(&t);
  if (strftime(b,sizeof(b),s,tm))
    lua_pushstring(b);
  else
    lua_error("invalid `date' format");
}
 

static void io_exit (void)
{
  lua_Object o = lua_getparam(1);
  exit(lua_isnumber(o) ? (int)lua_getnumber(o) : 1);
}


static void io_debug (void)
{
  while (1) {
    char buffer[250];
    fprintf(stderr, "lua_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == 0) return;
    if (strcmp(buffer, "cont\n") == 0) return;
    lua_dostring(buffer);
  }
}


static void lua_printstack (FILE *f)
{
  int level = 0;
  lua_Object func;
  fprintf(f, "Active Stack:\n");
  while ((func = lua_stackedfunction(level++)) != LUA_NOOBJECT) {
    char *name;
    int currentline;
    fprintf(f, "\t");
    switch (*lua_getobjname(func, &name)) {
      case 'g':
        fprintf(f, "function %s", name);
        break;
      case 'f':
        fprintf(f, "`%s' fallback", name);
        break;
      default: {
        char *filename;
        int linedefined;
        lua_funcinfo(func, &filename, &linedefined);
        if (linedefined == 0)
          fprintf(f, "main of %s", filename);
        else if (linedefined < 0)
          fprintf(f, "%s", filename);
        else
          fprintf(f, "function (%s:%d)", filename, linedefined);
      }
    }
    if ((currentline = lua_currentline(func)) > 0)
      fprintf(f, " at line %d", currentline);
    fprintf(f, "\n");
  }
}


static void errorfb (void)
{
  char *s = lua_opt_string(1, "(no messsage)", NULL);
  fprintf(stderr, "lua: %s\n", s);
  lua_printstack(stderr);
}


static struct lua_reg iolib[] = {
{"readfrom", io_readfrom},
{"writeto",  io_writeto},
{"appendto", io_appendto},
{"read",     io_read},
{"write",    io_write},
{"execute",  io_execute},
{"remove",   io_remove},
{"rename",   io_rename},
{"tmpname",   io_tmpname},
{"getenv",   io_getenv},
{"date",     io_date},
{"exit",     io_exit},
{"debug",    io_debug},
{"print_stack", errorfb}
};

void iolib_open (void)
{
  lua_infile=stdin; lua_outfile=stdout;
  luaI_openlib(iolib, (sizeof(iolib)/sizeof(iolib[0])));
  lua_setfallback("error", errorfb);
}
