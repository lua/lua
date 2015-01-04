/*
** iolib.c
** Input/output library to LUA
*/

char *rcs_iolib="$Id: iolib.c,v 1.29 1995/11/10 18:32:59 roberto Exp $";

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

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
**			status = 2 -> success (already exist)
**			status = 1 -> success (new file)
**			status = nil -> error
*/
static void io_appendto (void)
{
 char *s = lua_check_string(1, "appendto");
 struct stat st;
 int r = (stat(s, &st) == -1) ? 1 : 2;
 FILE *fp = fopen (s, "a");
 if (fp == NULL)
  lua_pushnil();
 else
 {
  if (out != stdout) fclose (out);
  out = fp;
  lua_pushnumber (r);
 }
}


static char getformat (char *f, int *just, int *m, int *n)
{
  int t;
  switch (*f++)
  {
    case 's': case 'S':
      t = 's';
      break;
    case 'f': case 'F': case 'g': case 'G': case 'e': case 'E':
      t = 'f';
      break;
    case 'i': case 'I':
      t = 'i';
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


static char *add_char (int c)
{
  static char *buff = NULL;
  static int max = 0;
  static int n = 0;
  if (n >= max)
  {
    if (max == 0)
    {
      max = 100;
      buff = (char *)malloc(max);
    }
    else
    {
      max *= 2;
      buff = (char *)realloc(buff, max);
    }
    if (buff == NULL)
      lua_error("memory overflow");
  }
  buff[n++] = c;
  if (c == 0)
    n = 0;  /* prepare for next string */
  return buff;
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
    add_char(c);
  return c;
}

static int read_until_blank (void)
{
  int c;
  while((c = fgetc(in)) != EOF && !isspace(c))
    add_char(c);
  return c;
}

static void read_m (int m)
{
  int c;
  while (m-- && (c = fgetc(in)) != EOF)
    add_char(c);
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
    {
      add_char(0);  /* to be ready for next time */
      lua_pushnil();
    }
    else
      lua_pushstring(add_char(0));
  }
  else
  {
    double d;
    char dummy;
    char *s;
    add_char(c);
    read_until_blank();
    s = add_char(0);
    if (sscanf(s, "%lf %c", &d, &dummy) == 1)
      lua_pushnumber(d);
    else
     lua_pushstring(s);
  }
}

static void io_read (void)
{
  lua_Object o = lua_getparam (1);
  if (o == LUA_NOOBJECT) 	/* free format */
    read_free();
  else				/* formatted */
  {
    int m, dummy1, dummy2;
    switch (getformat(lua_check_string(1, "read"), &dummy1, &m, &dummy2))
    {
      case 's':
        if (m < 0)
          read_until_blank();
        else
          read_m(m);
        lua_pushstring(add_char(0));
        break;

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
          result = sscanf(add_char(0), "%lf", &d);
        }
        if (result == 1)
          lua_pushnumber(d);
        else
          lua_pushnil();
        break;
      }
    }
  }
}


/*
** Read characters until a given one. The delimiter is not read.
*/
static void io_readuntil (void)
{
 int del = *lua_check_string(1, "readuntil");
 int c = read_until_char(del);
 if (c != EOF) ungetc(c,in);
 lua_pushstring(add_char(0));
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
    if (lua_isnumber(o1))
      status = fprintf (out, "%g", lua_getnumber(o1)) >= 0;
    else if (lua_isstring(o1))
      status = fprintf (out, "%s", lua_getstring(o1)) >= 0;
  }
  else					/* formated */
  {
    int just, m, n;
    switch (getformat (lua_check_string(2, "write"), &just, &m, &n))
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
 if (remove(lua_check_string(1, "remove")) == 0)
   lua_pushnumber (1);
 else
   lua_pushnil();
}


/*
** To get a environment variables
*/
static void io_getenv (void)
{
 char *env = getenv(lua_check_string(1, "getenv"));
 if (env == NULL) lua_pushnil();
 else             lua_pushstring(env); 
}

/*
** Return time: hour, min, sec
*/
static void io_time (void)
{
 time_t t;
 struct tm *s;
 
 time(&t);
 s = localtime(&t);
 lua_pushnumber(s->tm_hour);
 lua_pushnumber(s->tm_min);
 lua_pushnumber(s->tm_sec);
}
 
/*
** Return date: dd, mm, yyyy
*/
static void io_date (void)
{
 time_t t;
 struct tm *s;
 
 time(&t);
 s = localtime(&t);
 lua_pushnumber(s->tm_mday);
 lua_pushnumber(s->tm_mon+1);
 lua_pushnumber(s->tm_year+1900);
}
 
/*
** Beep
*/
static void io_beep (void)
{
 printf("\a");
}
 
/*
** To exit
*/
static void io_exit (void)
{
 lua_Object o = lua_getparam(1);
 if (lua_isstring(o))
  fprintf(stderr, "%s\n", lua_getstring(o));
 exit(1);
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
    if (gets(buffer) == 0) return;
    if (strcmp(buffer, "cont") == 0) return;
    lua_dostring(buffer);
  }
}


void lua_printstack (FILE *f)
{
  int level = 0;
  lua_Object func;
  fprintf(f, "Active Stack:\n");
  while ((func = lua_stackedfunction(level++)) != LUA_NOOBJECT)
  {
    char *name;
    int currentline;
    fprintf(f, "\t");
    switch (*getobjname(func, &name))
    {
      case 'g':
        fprintf(f, "function %s", name);
        break;
      case 'f':
        fprintf(f, "fallback %s", name);
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
      fprintf(f, "  at line %d", currentline);
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


/*
** Open io library
*/
void iolib_open (void)
{
 lua_register ("readfrom", io_readfrom);
 lua_register ("writeto",  io_writeto);
 lua_register ("appendto", io_appendto);
 lua_register ("read",     io_read);
 lua_register ("readuntil",io_readuntil);
 lua_register ("write",    io_write);
 lua_register ("execute",  io_execute);
 lua_register ("remove",   io_remove);
 lua_register ("getenv",   io_getenv);
 lua_register ("time",     io_time);
 lua_register ("date",     io_date);
 lua_register ("beep",     io_beep);
 lua_register ("exit",     io_exit);
 lua_register ("debug",    io_debug);
 lua_register ("print_stack", errorfb);
 lua_setfallback("error", errorfb);
}

