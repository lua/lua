#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#include "lua.h"
#include "auxlib.h"
#include "luadebug.h"
#include "lualib.h"


FILE *lua_infile, *lua_outfile;

int lua_tagio;


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
    lua_pushstring("O.S. unable to define the error");
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
  else if (lua_tag(f) == lua_tagio)
    lua_infile = lua_getuserdata(f);
  else {
    char *s = luaL_check_string(1);
    FILE *fp = (*s == '|') ? popen(s+1, "r") : fopen(s, "r");
    if (fp)
      lua_infile = fp;
    else {
      pushresult(0);
      return;
    }
  }
  lua_pushusertag(lua_infile, lua_tagio);
}


static void io_writeto (void)
{
  lua_Object f = lua_getparam(1);
  if (f == LUA_NOOBJECT)
    closefile(lua_outfile);  /* restore standart output */
  else if (lua_tag(f) == lua_tagio)
    lua_outfile = lua_getuserdata(f);
  else {
    char *s = luaL_check_string(1);
    FILE *fp = (*s == '|') ? popen(s+1,"w") : fopen(s,"w");
    if (fp)
      lua_outfile = fp;
    else {
      pushresult(0);
      return;
    }
  }
  lua_pushusertag(lua_outfile, lua_tagio);
}


static void io_appendto (void)
{
  char *s = luaL_check_string(1);
  FILE *fp = fopen (s, "a");
  if (fp != NULL) {
    lua_outfile = fp;
    lua_pushusertag(lua_outfile, lua_tagio);
  }
  else
    pushresult(0);
}


#define NEED_OTHER (EOF-1)  /* just some flag different from EOF */

static void io_read (void)
{
  char *buff;
  char *p = luaL_opt_string(1, "[^\n]*{\n}");
  int inskip = 0;  /* to control {skips} */
  int c = NEED_OTHER;
  luaI_emptybuff();
  while (*p) {
    if (*p == '{') {
      inskip++;
      p++;
    }
    else if (*p == '}') {
      if (inskip == 0)
        lua_error("unbalanced braces in read pattern");
      inskip--;
      p++;
    }
    else {
      char *ep = luaL_item_end(p);  /* get what is next */
      int m;  /* match result */
      if (c == NEED_OTHER) c = getc(lua_infile);
      m = (c == EOF) ? 0 : luaL_singlematch((char)c, p);
      if (m) {
        if (inskip == 0) luaI_addchar(c);
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
  while ((s = luaL_opt_string(arg++, NULL)) != NULL)
    status = status && (fputs(s, lua_outfile) != EOF);
  pushresult(status);
}


static void io_execute (void)
{
  lua_pushnumber(system(luaL_check_string(1)));
}


static void io_remove  (void)
{
  pushresult(remove(luaL_check_string(1)) == 0);
}


static void io_rename (void)
{
  pushresult(rename(luaL_check_string(1),
                    luaL_check_string(2)) == 0);
}


static void io_tmpname (void)
{
  lua_pushstring(tmpnam(NULL));
}



static void io_getenv (void)
{
  lua_pushstring(getenv(luaL_check_string(1))); /* if NULL push nil */
}


static void io_date (void)
{
  time_t t;
  struct tm *tm;
  char *s = luaL_opt_string(1, "%c");
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
  int level = 1;  /* skip level 0 (it's this function) */
  lua_Object func;
  while ((func = lua_stackedfunction(level++)) != LUA_NOOBJECT) {
    char *name;
    int currentline;
    fprintf(f, (level==2) ? "Active Stack:\n\t" : "\t");
    switch (*lua_getobjname(func, &name)) {
      case 'g':
        fprintf(f, "function %s", name);
        break;
      case 'f':
        fprintf(f, "`%s' tag method", name);
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
  fprintf(stderr, "lua: %s\n", lua_getstring(lua_getparam(1)));
  lua_printstack(stderr);
}



static struct luaL_reg iolib[] = {
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
  lua_tagio = lua_newtag();
  lua_infile=stdin; lua_outfile=stdout;
  luaL_openlib(iolib, (sizeof(iolib)/sizeof(iolib[0])));
  lua_seterrormethod(errorfb);
}
