#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#include "lua.h"
#include "auxlib.h"
#include "luadebug.h"
#include "lualib.h"


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



static FILE *getfile (char *name)
{
  lua_Object f = lua_getglobal(name);
  if (!lua_isuserdata(f) || lua_tag(f) != lua_tagio)
    luaL_verror("global variable %s is not a file handle", name);
  return lua_getuserdata(f);
}


static void closefile (char *name)
{
  FILE *f = getfile(name);
  if (f == stdin || f == stdout) return;
  if (pclose(f) == -1)
    fclose(f);
}


static void setfile (FILE *f, char *name)
{
  lua_pushusertag(f, lua_tagio);
  lua_setglobal(name);
}


static void setreturn (FILE *f, char *name)
{
  setfile(f, name);
  lua_pushusertag(f, lua_tagio);
}


static void io_readfrom (void)
{
  FILE *current;
  lua_Object f = lua_getparam(1);
  if (f == LUA_NOOBJECT) {
    closefile("_INPUT");
    current = stdin;
  }
  else if (lua_tag(f) == lua_tagio)
    current = lua_getuserdata(f);
  else {
    char *s = luaL_check_string(1);
    current = (*s == '|') ? popen(s+1, "r") : fopen(s, "r");
    if (current == NULL) {
      pushresult(0);
      return;
    }
  }
  setreturn(current, "_INPUT");
}


static void io_writeto (void)
{
  FILE *current;
  lua_Object f = lua_getparam(1);
  if (f == LUA_NOOBJECT) {
    closefile("_OUTPUT");
    current = stdout;
  }
  else if (lua_tag(f) == lua_tagio)
    current = lua_getuserdata(f);
  else {
    char *s = luaL_check_string(1);
    current = (*s == '|') ? popen(s+1,"w") : fopen(s,"w");
    if (current == NULL) {
      pushresult(0);
      return;
    }
  }
  setreturn(current, "_OUTPUT");
}


static void io_appendto (void)
{
  char *s = luaL_check_string(1);
  FILE *fp = fopen (s, "a");
  if (fp != NULL)
    setreturn(fp, "_OUTPUT");
  else
    pushresult(0);
}


#define NEED_OTHER (EOF-1)  /* just some flag different from EOF */

static void io_read (void)
{
  FILE *f = getfile("_INPUT");
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
      if (c == NEED_OTHER) c = getc(f);
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
     ungetc(c, f);
  buff = luaI_addchar(0);
  if (*buff != 0 || *p == 0)  /* read something or did not fail? */
    lua_pushstring(buff);
}


static void io_write (void)
{
  FILE *f = getfile("_OUTPUT");
  int arg = 1;
  int status = 1;
  char *s;
  while ((s = luaL_opt_string(arg++, NULL)) != NULL)
    status = status && (fputs(s, f) != EOF);
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
    char *filename;
    int linedefined;
    lua_funcinfo(func, &filename, &linedefined);
    fprintf(f, (level==2) ? "Active Stack:\n\t" : "\t");
    switch (*lua_getobjname(func, &name)) {
      case 'g':
        fprintf(f, "function %s", name);
        break;
      case 't':
        fprintf(f, "`%s' tag method", name);
        break;
      default: {
        if (linedefined == 0)
          fprintf(f, "main of %s", filename);
        else if (linedefined < 0)
          fprintf(f, "%s", filename);
        else
          fprintf(f, "function (%s:%d)", filename, linedefined);
        filename = NULL;
      }
    }
    if ((currentline = lua_currentline(func)) > 0)
      fprintf(f, " at line %d", currentline);
    if (filename)
      fprintf(f, " [in file %s]", filename);
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
  setfile(stdin, "_INPUT");
  setfile(stdout, "_OUTPUT");
  setfile(stdin, "_STDIN");
  setfile(stdout, "_STDOUT");
  setfile(stderr, "_STDERR");
  luaL_openlib(iolib, (sizeof(iolib)/sizeof(iolib[0])));
  lua_pushcfunction(errorfb);
  lua_seterrormethod();
}
