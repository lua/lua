/*
** inout.c
** Provide function to realise the input/output function and debugger 
** facilities.
*/

char *rcs_inout="$Id: inout.c,v 1.2 1993/12/22 21:15:16 roberto Exp $";

#include <stdio.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"

/* Exported variables */
int lua_linenumber;
int lua_debug;
int lua_debugline;

/* Internal variables */
#ifndef MAXFUNCSTACK
#define MAXFUNCSTACK 32
#endif
static struct { int file; int function; } funcstack[MAXFUNCSTACK];
static int nfuncstack=0;

static FILE *fp;
static char *st;
static void (*usererror) (char *s);

/*
** Function to set user function to handle errors.
*/
void lua_errorfunction (void (*fn) (char *s))
{
 usererror = fn;
}

/*
** Function to get the next character from the input file
*/
static int fileinput (void)
{
 int c = fgetc (fp);
 return (c == EOF ? 0 : c);
}

/*
** Function to get the next character from the input string
*/
static int stringinput (void)
{
 st++;
 return (*(st-1));
}

/*
** Function to open a file to be input unit. 
** Return 0 on success or 1 on error.
*/
int lua_openfile (char *fn)
{
 lua_linenumber = 1;
 lua_setinput (fileinput);
 fp = fopen (fn, "r");
 if (fp == NULL) return 1;
 if (lua_addfile (fn)) return 1;
 return 0;
}

/*
** Function to close an opened file
*/
void lua_closefile (void)
{
 if (fp != NULL)
 {
  lua_delfile();
  fclose (fp);
  fp = NULL;
 }
}

/*
** Function to open a string to be input unit
*/
int lua_openstring (char *s)
{
 lua_linenumber = 1;
 lua_setinput (stringinput);
 st = s;
 {
  char sn[64];
  sprintf (sn, "String: %10.10s...", s);
  if (lua_addfile (sn)) return 1;
 }
 return 0;
}

/*
** Function to close an opened string
*/
void lua_closestring (void)
{
 lua_delfile();
}

/*
** Call user function to handle error messages, if registred. Or report error
** using standard function (fprintf).
*/
void lua_error (char *s)
{
 if (usererror != NULL) usererror (s);
 else			    fprintf (stderr, "lua: %s\n", s);
}

/*
** Called to execute  SETFUNCTION opcode, this function pushs a function into
** function stack. Return 0 on success or 1 on error.
*/
int lua_pushfunction (int file, int function)
{
 if (nfuncstack >= MAXFUNCSTACK-1)
 {
  lua_error ("function stack overflow");
  return 1;
 }
 funcstack[nfuncstack].file = file;
 funcstack[nfuncstack].function = function;
 nfuncstack++;
 return 0;
}

/*
** Called to execute  RESET opcode, this function pops a function from 
** function stack.
*/
void lua_popfunction (void)
{
 nfuncstack--;
}

/*
** Report bug building a message and sending it to lua_error function.
*/
void lua_reportbug (char *s)
{
 char msg[1024];
 strcpy (msg, s);
 if (lua_debugline != 0)
 {
  int i;
  if (nfuncstack > 0)
  {
   sprintf (strchr(msg,0), 
         "\n\tin statement begining at line %d in function \"%s\" of file \"%s\"",
         lua_debugline, s_name(funcstack[nfuncstack-1].function),
  	 lua_file[funcstack[nfuncstack-1].file]);
   sprintf (strchr(msg,0), "\n\tactive stack\n");
   for (i=nfuncstack-1; i>=0; i--)
    sprintf (strchr(msg,0), "\t-> function \"%s\" of file \"%s\"\n", 
                            s_name(funcstack[i].function),
			    lua_file[funcstack[i].file]);
  }
  else
  {
   sprintf (strchr(msg,0),
         "\n\tin statement begining at line %d of file \"%s\"", 
         lua_debugline, lua_filename());
  }
 }
 lua_error (msg);
}

