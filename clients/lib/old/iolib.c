/*
** iolib.c
** Input/output library to LUA
*/

char *rcs_iolib="$Id: iolib.c,v 1.44 1996/05/03 20:10:59 roberto Exp $";

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#include "lua.h"
#include "luadebug.h"
#include "lualib.h"

static FILE *in=stdin, *out=stdout;


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
    lua_pushnumber (1);
  else
   lua_pushnil();
}

static void closeread (void)
{
  if (in != stdin)
  {
    if (pclose(in) == -1)
      fclose(in);
    in = stdin;
  }
}

static void closewrite (void)
{
  if (out != stdout)
  {
    if (pclose(out) == -1)
      fclose(out);
    out = stdout;
  }
}

/*
** Open a file to read.
** LUA interface:
**			status = readfrom (filename)
** where:
**			status = 1 -> success
**			status = nil -> error
*/
static void io_readfrom (void)
{
 if (lua_getparam (1) == LUA_NOOBJECT)	
 { /* restore standart input */
  closeread();
  lua_pushnumber (1);
 }
 else
 {
   char *s = lua_check_string(1, "readfrom");
   FILE *fp = (*s == '|') ? popen(s+1, "r") : fopen(s, "r");
   if (fp == NULL)
    lua_pushnil();
   else
   {
    closeread();
    in = fp;
    lua_pushnumber (1);
   }
 }
}


/*
** Open a file to write.
** LUA interface:
**			status = writeto (filename)
** where:
**			status = 1 -> success
**			status = nil -> error
*/
static void io_writeto (void)
{
 if (lua_getparam (1) == LUA_NOOBJECT)	/* restore standart output */
 {
  closewrite();
  lua_pushnumber (1);
 }
 else
 {
   char *s = lua_check_string(1, "writeto");
   FILE *fp = (*s == '|') ? popen(s+1,"w") : fopen(s,"w");
   if (fp == NULL)
    lua_pushnil();
   else
   {
    closewrite();
    out = fp;
    lua_pushnumber (1);
   }
 }
}


/*
** Open a file to write appended.
** LUA interface:
**			status = appendto (filename)
** where:
**			status = 1 -> success
**			status = nil -> error
*/
static void io_appendto (void)
{
 char *s = lua_check_string(1, "appendto");
 FILE *fp = fopen (s, "a");
 if (fp == NULL)
  lua_pushnil();
 else
 {
  if (out != stdout) fclose (out);
  out = fp;
  lua_pushnumber(1);
 }
}


static char getformat (char *f, int *just, int *m, int *n)
{
  int t;
  switch (*f++)
  {
    case 'q': case 'Q':
    case 's': case 'S':
    case 'i': case 'I':
      t = tolower(*(f-1));
      break;
    case 'f': case 'F': case 'g': case 'G': case 'e': case 'E':
      t = 'f';
      break;
    default:
      t = 0;  /* to avoid compiler warnings */
      lua_arg_error("read/write (format)");
  }
  *just = (*f == '<' || *f == '>' || *f == '|') ? *f++ : '>';
  if (isdigit(*f))
  {
    *m = 0;
    while (isdigit(*f))
      *m = *m*10 + (*f++ - '0');
  }
  else
    *m = -1;
  if (*f == '.')
  {
    f++;	/* skip point */
    *n = 0;
    while (isdigit(*f))
      *n = *n*10 + (*f++ - '0');
  }
  else
    *n = -1;
  return t;
}


/*
** Read a variable. On error put nil on stack.
** LUA interface:
**			variable = read ([format])
**
** O formato pode ter um dos seguintes especificadores:
**
**	s ou S -> para string
**	f ou F, g ou G, e ou E -> para reais
**	i ou I -> para inteiros
**
**	Estes especificadores podem vir seguidos de numero que representa
**	o numero de campos a serem lidos.
*/

static int read_until_char (int del)
{
  int c;
  while((c = fgetc(in)) != EOF && c != del)
    luaI_addchar(c);
  return c;
}

static void read_until_blank (void)
{
  int c;
  while((c = fgetc(in)) != EOF && !isspace(c))
    luaI_addchar(c);
  if (c != EOF) ungetc(c,in);
}

static void read_m (int m)
{
  int c;
  while (m-- && (c = fgetc(in)) != EOF)
    luaI_addchar(c);
}


static void read_free (void)
{
  int c;
  while (isspace(c=fgetc(in)))
   ;
  if (c == EOF)
  {
    lua_pushnil();
    return;
  }
  if (c == '\"' || c == '\'')
  { /* string */
    c = read_until_char(c);
    if (c == EOF)
      lua_pushnil();
    else
      lua_pushstring(luaI_addchar(0));
  }
  else
  {
    double d;
    char dummy;
    char *s;
    luaI_addchar(c);
    read_until_blank();
    s = luaI_addchar(0);
    if (sscanf(s, "%lf %c", &d, &dummy) == 1)
      lua_pushnumber(d);
    else
     lua_pushstring(s);
  }
}

static void io_read (void)
{
  lua_Object o = lua_getparam (1);
  luaI_addchar(0);  /* initialize buffer */
  if (o == LUA_NOOBJECT) 	/* free format */
    read_free();
  else				/* formatted */
  {
    int m, dummy1, dummy2;
    switch (getformat(lua_check_string(1, "read"), &dummy1, &m, &dummy2))
    {
      case 's':
      {
        char *s;
        if (m < 0)
          read_until_blank();
        else
          read_m(m);
        s = luaI_addchar(0);
        if ((m >= 0 && strlen(s) == m) || (m < 0 && strlen(s) > 0))
          lua_pushstring(s);
        else
          lua_pushnil();
        break;
      }

      case 'i':  /* can read as float, since it makes no difference to Lua */
      case 'f':
      {
        double d;
        int result;
        if (m < 0)
          result = fscanf(in, "%lf", &d);
        else
        {
          read_m(m);
          result = sscanf(luaI_addchar(0), "%lf", &d);
        }
        if (result == 1)
          lua_pushnumber(d);
        else
          lua_pushnil();
        break;
      }
      default:
        lua_arg_error("read (format)");
    }
  }
}


/*
** Read characters until a given one. The delimiter is not read.
*/
static void io_readuntil (void)
{
 int del, c;
 lua_Object p = lua_getparam(1);
 luaI_addchar(0);  /* initialize buffer */
 if (p == LUA_NOOBJECT || lua_isnil(p))
   del = EOF;
 else
  del = *lua_check_string(1, "readuntil");
 c = read_until_char(del);
 if (c != EOF) ungetc(c,in);
 lua_pushstring(luaI_addchar(0));
}


/*
** Write a variable. On error put 0 on stack, otherwise put 1.
** LUA interface:
**			status = write (variable [,format])
**
** O formato pode ter um dos seguintes especificadores:
**
**	s ou S -> para string
**	f ou F, g ou G, e ou E -> para reais
**	i ou I -> para inteiros
**
**	Estes especificadores podem vir seguidos de:
**
**		[?][m][.n]
**
**	onde:
**		? -> indica justificacao
**			< = esquerda
**			| = centro
**			> = direita (default)
**		m -> numero maximo de campos (se exceder estoura)
**		n -> indica precisao para
**			reais -> numero de casas decimais
**			inteiros -> numero minimo de digitos
**			string -> nao se aplica
*/

static int write_fill (int n, int c)
{
  while (n--)
    if (fputc(c, out) == EOF)
      return 0;
  return 1;
}

static int write_string (char *s, int just, int m)
{
  int status;
  int l = strlen(s);
  int pre;  /* number of blanks before string */
  if (m < 0) m = l;
  else if (l > m)
  {
    write_fill(m, '*');
    return 0;
  }
  pre = (just == '<') ? 0 : (just == '>') ? m-l : (m-l)/2;
  status = write_fill(pre, ' ');
  status = status && fprintf(out, "%s", s) >= 0;
  status = status && write_fill(m-(l+pre), ' ');
  return status;
}

static int write_quoted (int just, int m)
{
  luaI_addchar(0);
  luaI_addquoted(lua_check_string(1, "write"));
  return write_string(luaI_addchar(0), just, m);
}

static int write_float (int just, int m, int n)
{
  char buffer[100];
  lua_Object p = lua_getparam(1);
  float number;
  if (!lua_isnumber(p)) return 0;
  number = lua_getnumber(p);
  if (n >= 0)
    sprintf(buffer, "%.*f", n, number);
  else
    sprintf(buffer, "%g", number);
  return write_string(buffer, just, m);
}


static int write_int (int just, int m, int n)
{
  char buffer[100];
  lua_Object p = lua_getparam(1);
  int number;
  if (!lua_isnumber(p)) return 0;
  number = (int)lua_getnumber(p);
  if (n >= 0)
    sprintf(buffer, "%.*d", n, number);
  else
    sprintf(buffer, "%d", number);
  return write_string(buffer, just, m);
}


static void io_write (void)
{
  int status = 0;
  if (lua_getparam (2) == LUA_NOOBJECT)   /* free format */
  {
    lua_Object o1 = lua_getparam(1);
    int t = lua_type(o1);
    if (t == LUA_T_NUMBER)
      status = fprintf (out, "%g", lua_getnumber(o1)) >= 0;
    else if (t == LUA_T_STRING)
      status = fprintf (out, "%s", lua_getstring(o1)) >= 0;
  }
  else					/* formated */
  {
    int just, m, n;
    switch (getformat(lua_check_string(2, "write"), &just, &m, &n))
    {
      case 's':
      {
        lua_Object p = lua_getparam(1);
        if (lua_isstring(p))
          status = write_string(lua_getstring(p), just, m);
        else
          status = 0;
        break;
      }
      case 'q':
        status = write_quoted(just, m);
        break;
      case 'f':
        status = write_float(just, m, n);
        break;
      case 'i':
        status = write_int(just, m, n);
        break;
    }
  }
  if (status)
    lua_pushnumber(status);
  else
    lua_pushnil();
}

/*
** Execute a executable program using "system".
** Return the result of execution.
*/
static void io_execute (void)
{
  lua_pushnumber(system(lua_check_string(1, "execute")));
}

/*
** Remove a file. On error return nil.
*/
static void io_remove  (void)
{
  pushresult(remove(lua_check_string(1, "remove")) == 0);
}

static void io_rename (void)
{
  char *f1 = lua_check_string(1, "rename");
  char *f2 = lua_check_string(2, "rename");
  pushresult(rename(f1, f2) == 0);
}

static void io_tmpname (void)
{
  lua_pushstring(tmpnam(NULL));
}

static void io_errorno (void)
{
/*  lua_pushstring(strerror(errno));*/
}


/*
** To get a environment variable
*/
static void io_getenv (void)
{
 char *env = getenv(lua_check_string(1, "getenv"));
 lua_pushstring(env);  /* if NULL push nil */
}

/*
** Return user formatted time stamp
*/
static void io_date (void)
{
 time_t t;
 struct tm *tm;
 char *s;
 char b[BUFSIZ];
 if (lua_getparam(1) == LUA_NOOBJECT)
  s = "%c";
 else
  s = lua_check_string(1, "date");
 time(&t); tm = localtime(&t);
 if (strftime(b,sizeof(b),s,tm))
  lua_pushstring(b);
 else
  lua_error("invalid `date' format");
}
 
/*
** To exit
*/
static void io_exit (void)
{
 lua_Object o = lua_getparam(1);
 int code = lua_isnumber(o) ? (int)lua_getnumber(o) : 1;
 exit(code);
}

/*
** To debug a lua program. Start a dialog with the user, interpreting
   lua commands until an 'cont'.
*/
static void io_debug (void)
{
  while (1)
  {
    char buffer[250];
    fprintf(stderr, "lua_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == 0) return;
    if (strcmp(buffer, "cont") == 0) return;
    lua_dostring(buffer);
  }
}


static void lua_printstack (FILE *f)
{
  int level = 0;
  lua_Object func;
  fprintf(f, "Active Stack:\n");
  while ((func = lua_stackedfunction(level++)) != LUA_NOOBJECT)
  {
    char *name;
    int currentline;
    fprintf(f, "\t");
    switch (*lua_getobjname(func, &name))
    {
      case 'g':
        fprintf(f, "function %s", name);
        break;
      case 'f':
        fprintf(f, "`%s' fallback", name);
        break;
      default:
      {
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
  lua_Object o = lua_getparam(1);
  char *s = lua_isstring(o) ? lua_getstring(o) : "(no messsage)";
  fprintf(stderr, "lua: %s\n", s);
  lua_printstack(stderr);
}


static struct lua_reg iolib[] = {
{"readfrom", io_readfrom},
{"writeto",  io_writeto},
{"appendto", io_appendto},
{"read",     io_read},
{"readuntil",io_readuntil},
{"write",    io_write},
{"execute",  io_execute},
{"remove",   io_remove},
{"rename",   io_rename},
{"tmpname",   io_tmpname},
{"ioerror",   io_errorno},
{"getenv",   io_getenv},
{"date",     io_date},
{"exit",     io_exit},
{"debug",    io_debug},
{"print_stack", errorfb}
};

void iolib_open (void)
{
  luaI_openlib(iolib, (sizeof(iolib)/sizeof(iolib[0])));
  lua_setfallback("error", errorfb);
}
