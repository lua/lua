/*
** iolib.c
** Input/output library to LUA
**
** Waldemar Celes Filho
** TeCGraf - PUC-Rio
** 19 May 93
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#ifdef __GNUC__
#include <floatingpoint.h>
#endif

#include "lua.h"

static FILE *in=NULL, *out=NULL;

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
 if (o == NULL)			/* restore standart input */
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
 if (o == NULL)			/* restore standart output */
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
 if (o == NULL)			/* free format */
 {
  int c;
  char s[256];
  while (isspace(c=fgetc(in)))
   ;
  if (c == '\"')
  {
   if (fscanf (in, "%[^\"]\"", s) != 1)
   {
    lua_pushnil ();
    return;
   }
  }
  else if (c == '\'')
  {
   if (fscanf (in, "%[^\']\'", s) != 1)
   {
    lua_pushnil ();
    return;
   }
  }
  else
  {
   char *ptr;
   double d;
   ungetc (c, in);
   if (fscanf (in, "%s", s) != 1)
   {
    lua_pushnil ();
    return;
   }
   d = strtod (s, &ptr);
   if (!(*ptr))
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
   fscanf (in, f, s);
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
     float f;
     sscanf (s, "%f", &f);
     lua_pushnumber(f);
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
     fscanf (in, "%ld", &l);
     lua_pushnumber(l);
    }
    break;
    case 'f': case 'g': case 'e':
    {
     float f;
     fscanf (in, "%f", &f);
     lua_pushnumber(f);
    }
    break;
    default: 
    {
     char s[256];
     fscanf (in, "%s", s);
     lua_pushstring(s);
    }
    break;
   }
  }
 }
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
 static char buffer[512];
 static char f[80];
 char *string = &buffer[255];
 char t, j='r';
 int  m=0, n=0, l;
 while (isspace(*e)) e++;
 t = *e++;
 if (*e == '<' || *e == '|' || *e == '>') j = *e++;
 while (isdigit(*e))
  m = m*10 + (*e++ - '0');
 e++;	/* skip point */
 while (isdigit(*e))
  n = n*10 + (*e++ - '0');

 sprintf(f,"%%");
 if (j == '<' || j == '|') sprintf(strchr(f,0),"-");
 if (m != 0)   sprintf(strchr(f,0),"%d", m);
 if (n != 0)   sprintf(strchr(f,0),".%d", n);
 sprintf(strchr(f,0), "%c", t);
 switch (tolower(t))
 {
  case 'i': t = 'i';
   sprintf (string, f, (long int)lua_getnumber(o));
  break;
  case 'f': case 'g': case 'e': t = 'f';
   sprintf (string, f, (float)lua_getnumber(o));
  break;
  case 's': t = 's';
   sprintf (string, f, lua_getstring(o));
  break;
  default: return "";
 }
 l = strlen(string);
 if (m!=0 && l>m)
 {
  int i;
  for (i=0; i<m; i++)
   string[i] = '*';
  string[i] = 0;
 }
 else if (m!=0 && j=='|')
 {
  int i=l-1;
  while (isspace(string[i])) i--;
  string -= (m-i) / 2;
  i=0;
  while (string[i]==0) string[i++] = ' ';
  string[l] = 0;
 }
 return string;
}
static void io_write (void)
{
 lua_Object o1 = lua_getparam (1);
 lua_Object o2 = lua_getparam (2);
 if (o1 == NULL)			/* new line */
 {
  fprintf (out, "\n");
  lua_pushnumber(1);
 }
 else if (o2 == NULL)   		/* free format */
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
** Execute a executable program using "sustem".
** On error put 0 on stack, otherwise put 1.
*/
void io_execute (void)
{
 lua_Object o = lua_getparam (1);
 if (o == NULL || !lua_isstring (o))
 {
  lua_error ("incorrect argument to function 'execute`");
  lua_pushnumber (0);
 }
 else
 {
  system(lua_getstring(o));
  lua_pushnumber (1);
 }
 return;
}

/*
** Remove a file.
** On error put 0 on stack, otherwise put 1.
*/
void io_remove  (void)
{
 lua_Object o = lua_getparam (1);
 if (o == NULL || !lua_isstring (o))
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
** Open io library
*/
void iolib_open (void)
{
 in=stdin; out=stdout;
 lua_register ("readfrom", io_readfrom);
 lua_register ("writeto",  io_writeto);
 lua_register ("read",     io_read);
 lua_register ("write",    io_write);
 lua_register ("execute",  io_execute);
 lua_register ("remove",   io_remove);
}
