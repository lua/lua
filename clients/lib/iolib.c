/*
** iolib.c
** Input/output library to LUA
*/

char *rcs_iolib="$Id: iolib.c,v 1.21 1995/02/06 19:36:13 roberto Exp $";

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "lua.h"
#include "lualib.h"

static FILE *in=stdin, *out=stdout;

/*
** Open a file to read.
** LUA interface:
**			status = readfrom (filename)
** where:
**			status = 1 -> success
**			status = 0 -> error
*/
static void io_readfrom (void)
{
 lua_Object o = lua_getparam (1);
 if (o == LUA_NOOBJECT)			/* restore standart input */
 {
  if (in != stdin)
  {
   fclose (in);
   in = stdin;
  }
  lua_pushnumber (1);
 }
 else
 {
  if (!lua_isstring (o))
  {
   lua_error ("incorrect argument to function 'readfrom`");
   lua_pushnumber (0);
  }
  else
  {
   FILE *fp = fopen (lua_getstring(o),"r");
   if (fp == NULL)
   {
    lua_pushnumber (0);
   }
   else
   {
    if (in != stdin) fclose (in);
    in = fp;
    lua_pushnumber (1);
   }
  }
 }
}


/*
** Open a file to write.
** LUA interface:
**			status = writeto (filename)
** where:
**			status = 1 -> success
**			status = 0 -> error
*/
static void io_writeto (void)
{
 lua_Object o = lua_getparam (1);
 if (o == LUA_NOOBJECT)			/* restore standart output */
 {
  if (out != stdout)
  {
   fclose (out);
   out = stdout;
  }
  lua_pushnumber (1);
 }
 else
 {
  if (!lua_isstring (o))
  {
   lua_error ("incorrect argument to function 'writeto`");
   lua_pushnumber (0);
  }
  else
  {
   FILE *fp = fopen (lua_getstring(o),"w");
   if (fp == NULL)
   {
    lua_pushnumber (0);
   }
   else
   {
    if (out != stdout) fclose (out);
    out = fp;
    lua_pushnumber (1);
   }
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
**			status = 0 -> error
*/
static void io_appendto (void)
{
 lua_Object o = lua_getparam (1);
 if (o == LUA_NOOBJECT)			/* restore standart output */
 {
  if (out != stdout)
  {
   fclose (out);
   out = stdout;
  }
  lua_pushnumber (1);
 }
 else
 {
  if (!lua_isstring (o))
  {
   lua_error ("incorrect argument to function 'appendto`");
   lua_pushnumber (0);
  }
  else
  {
   int r;
   FILE *fp;
   struct stat st;
   if (stat(lua_getstring(o), &st) == -1) r = 1;
   else                                   r = 2;
   fp = fopen (lua_getstring(o),"a");
   if (fp == NULL)
   {
    lua_pushnumber (0);
   }
   else
   {
    if (out != stdout) fclose (out);
    out = fp;
    lua_pushnumber (r);
   }
  }
 }
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
static void io_read (void)
{
 lua_Object o = lua_getparam (1);
 if (o == LUA_NOOBJECT || !lua_isstring(o))	/* free format */
 {
  int c;
  char s[256];
  while (isspace(c=fgetc(in)))
   ;
  if (c == '\"')
  {
   int n=0;
   while((c = fgetc(in)) != '\"')
   {
    if (c == EOF)
    {
     lua_pushnil ();
     return;
    }
    s[n++] = c;
   }
   s[n] = 0;
  }
  else if (c == '\'')
  {
   int n=0;
   while((c = fgetc(in)) != '\'')
   {
    if (c == EOF)
    {
     lua_pushnil ();
     return;
    }
    s[n++] = c;
   }
   s[n] = 0;
  }
  else
  {
   double d;
   ungetc (c, in);
   if (fscanf (in, "%s", s) != 1)
   {
    lua_pushnil ();
    return;
   }
   if (sscanf(s, "%lf %*c", &d) == 1)
   {
    lua_pushnumber (d);
    return;
   }
  }
  lua_pushstring (s);
  return;
 }
 else				/* formatted */
 {
  char *e = lua_getstring(o);
  char t;
  int  m=0;
  while (isspace(*e)) e++;
  t = *e++;
  while (isdigit(*e))
   m = m*10 + (*e++ - '0');
  
  if (m > 0)
  {
   char f[80];
   char s[256];
   sprintf (f, "%%%ds", m);
   if (fgets (s, m, in) == NULL)
   {
    lua_pushnil();
    return;
   }
   else
   {
    if (s[strlen(s)-1] == '\n')
     s[strlen(s)-1] = 0;
   }
   switch (tolower(t))
   {
    case 'i':
    {
     long int l;
     sscanf (s, "%ld", &l);
     lua_pushnumber(l);
    }
    break;
    case 'f': case 'g': case 'e':
    {
     float fl;
     sscanf (s, "%f", &fl);
     lua_pushnumber(fl);
    }
    break;
    default: 
     lua_pushstring(s); 
    break;
   }
  }
  else
  {
   switch (tolower(t))
   {
    case 'i':
    {
     long int l;
     if (fscanf (in, "%ld", &l) == EOF)
       lua_pushnil();
       else lua_pushnumber(l);
    }
    break;
    case 'f': case 'g': case 'e':
    {
     float f;
     if (fscanf (in, "%f", &f) == EOF)
       lua_pushnil();
       else lua_pushnumber(f);
    }
    break;
    default: 
    {
     char s[256];
     if (fscanf (in, "%s", s) == EOF)
       lua_pushnil();
       else lua_pushstring(s);
    }
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
 int n=255,m=0;
 int c,d;
 char *s;
 lua_Object lo = lua_getparam(1);
 if (!lua_isstring(lo))
  d = EOF; 
 else
  d = *lua_getstring(lo);
 
 s = (char *)malloc(n+1);
 while((c = fgetc(in)) != EOF && c != d)
 {
  if (m==n)
  {
   n *= 2;
   s = (char *)realloc(s, n+1);
  }
  s[m++] = c;
 }
 if (c != EOF) ungetc(c,in);
 s[m] = 0;
 lua_pushstring(s);
 free(s);
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
static char *buildformat (char *e, lua_Object o)
{
 static char buffer[2048];
 static char f[80];
 char *string = &buffer[255];
 char *fstart=e, *fspace, *send;
 char t, j='r';
 int  m=0, n=-1, l;
 while (isspace(*e)) e++;
 fspace = e;
 t = *e++;
 if (*e == '<' || *e == '|' || *e == '>') j = *e++;
 while (isdigit(*e))
  m = m*10 + (*e++ - '0');
 if (*e == '.') e++;	/* skip point */
 while (isdigit(*e))
  if (n < 0) n = (*e++ - '0');
  else       n = n*10 + (*e++ - '0');

 sprintf(f,"%%");
 if (j == '<' || j == '|') sprintf(strchr(f,0),"-");
 if (m >  0)   sprintf(strchr(f,0),"%d", m);
 if (n >= 0)   sprintf(strchr(f,0),".%d", n);
 switch (t)
 {
  case 'i': case 'I': t = 'd';
   sprintf(strchr(f,0), "%c", t);
   sprintf (string, f, (long int)lua_getnumber(o));
  break;
  case 'f': case 'g': case 'e': case 'G': case 'E': 
   sprintf(strchr(f,0), "%c", t);
   sprintf (string, f, (float)lua_getnumber(o));
  break;
  case 'F': t = 'f';
   sprintf(strchr(f,0), "%c", t);
   sprintf (string, f, (float)lua_getnumber(o));
  break;
  case 's': case 'S': t = 's';
   sprintf(strchr(f,0), "%c", t);
   sprintf (string, f, lua_getstring(o));
  break;
  default: return "";
 }
 l = strlen(string);
 send = string+l;
 if (m!=0 && l>m)
 {
  int i;
  for (i=0; i<m; i++)
   string[i] = '*';
  string[i] = 0;
 }
 else if (m!=0 && j=='|')
 {
  int k;
  int i=l-1;
  while (isspace(string[i]) || string[i]==0) i--;
  string -= (m-i)/2;
  for(k=0; k<(m-i)/2; k++)
   string[k] = ' ';
 }
 /* add space characteres */
 while (fspace != fstart)
 {
  string--;
  fspace--;
  *string = *fspace; 
 }
 while (isspace(*e)) *send++ = *e++;
 *send = 0;
 return string;
}
static void io_write (void)
{
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 if (o1 == LUA_NOOBJECT)			/* new line */
 {
  fprintf (out, "\n");
  lua_pushnumber(1);
 }
 else if (o2 == LUA_NOOBJECT)   		/* free format */
 {
  int status=0;
  if (lua_isnumber(o1))
   status = fprintf (out, "%g", lua_getnumber(o1));
  else if (lua_isstring(o1))
   status = fprintf (out, "%s", lua_getstring(o1));
  lua_pushnumber(status);
 }
 else					/* formated */
 {
  if (!lua_isstring(o2))
  { 
   lua_error ("incorrect format to function `write'"); 
   lua_pushnumber(0);
   return;
  }
  lua_pushnumber(fprintf (out, "%s", buildformat(lua_getstring(o2),o1)));
 }
}

/*
** Execute a executable program using "system".
** Return the result of execution.
*/
static void io_execute (void)
{
 lua_Object o = lua_getparam (1);
 if (o == LUA_NOOBJECT || !lua_isstring (o))
 {
  lua_error ("incorrect argument to function 'execute`");
  lua_pushnumber (0);
 }
 else
 {
  int res = system(lua_getstring(o));
  lua_pushnumber (res);
 }
 return;
}

/*
** Remove a file.
** On error put 0 on stack, otherwise put 1.
*/
static void io_remove  (void)
{
 lua_Object o = lua_getparam (1);
 if (o == LUA_NOOBJECT || !lua_isstring (o))
 {
  lua_error ("incorrect argument to function 'execute`");
  lua_pushnumber (0);
 }
 else
 {
  if (remove(lua_getstring(o)) == 0)
   lua_pushnumber (1);
  else
   lua_pushnumber (0);
 }
 return;
}


/*
** To get a environment variables
*/
static void io_getenv (void)
{
 lua_Object s = lua_getparam(1);
 if (!lua_isstring(s))
  lua_pushnil();
 else
 {
  char *env = getenv(lua_getstring(s));
  if (env == NULL) lua_pushnil();
  else             lua_pushstring(env); 
 }
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
  printf("%s\n", lua_getstring(o));
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
}
