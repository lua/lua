/*
** inout.c
** Provide function to realise the input/output function and debugger 
** facilities.
** Also provides some predefined lua functions.
*/

char *rcs_inout="$Id: inout.c,v 2.20 1995/05/16 17:23:58 roberto Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "tree.h"
#include "lua.h"


#ifndef MAXFUNCSTACK
#define MAXFUNCSTACK 100
#endif

#define MAXMESSAGE MAXFUNCSTACK*80


/* Exported variables */
Word lua_linenumber;
Bool lua_debug;
Word lua_debugline = 0;


/* Internal variables */

typedef struct FuncStackNode {
  struct FuncStackNode *next;
  char *file;
  Word function;
  Word line;
} FuncStackNode;
 
static FuncStackNode *funcStack = NULL;
static Word nfuncstack=0;

static FILE *fp;
static char *st;

/*
** Function to get the next character from the input file
*/
static int fileinput (void)
{
 return fgetc (fp);
}

/*
** Function to get the next character from the input string
*/
static int stringinput (void)
{
 return *st++;
}

/*
** Function to open a file to be input unit. 
** Return 0 on success or error message on error.
*/
char *lua_openfile (char *fn)
{
 lua_linenumber = 1;
 lua_setinput (fileinput);
 fp = fopen (fn, "r");
 if (fp == NULL)
 {
   static char buff[255];
   sprintf(buff, "unable to open file %.230s", fn);
   return buff;
 }
 return lua_addfile (fn);
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
char *lua_openstring (char *s)
{
 lua_linenumber = 1;
 lua_setinput (stringinput);
 st = s;
 {
  char sn[64];
  sprintf (sn, "String: %10.10s...", s);
  return lua_addfile (sn);
 }
}

/*
** Function to close an opened string
*/
void lua_closestring (void)
{
 lua_delfile();
}


/*
** Called to execute  SETFUNCTION opcode, this function pushs a function into
** function stack.
*/
void lua_pushfunction (char *file, Word function)
{
 FuncStackNode *newNode;
 if (nfuncstack++ >= MAXFUNCSTACK)
 {
  lua_error("function stack overflow");
 }
 newNode = new(FuncStackNode);
 newNode->function = function;
 newNode->file = file;
 newNode->line= lua_debugline;
 newNode->next = funcStack;
 funcStack = newNode;
}

/*
** Called to execute RESET opcode, this function pops a function from 
** function stack.
*/
void lua_popfunction (void)
{
 FuncStackNode *temp = funcStack;
 if (temp == NULL) return;
 --nfuncstack;
 lua_debugline = temp->line;
 funcStack = temp->next;
 luaI_free(temp);
}

/*
** Report bug building a message and pushing it on the stack.
*/
void luaI_reportbug (char *s, int err)
{
  char msg[MAXMESSAGE];
  strcpy (msg, s);
 if (lua_debugline != 0)
 {
  if (funcStack)
  {
   FuncStackNode *func = funcStack;
   int line = lua_debugline;
   sprintf (strchr(msg,0), "\n\tactive stack:\n");
   do
   {
     sprintf (strchr(msg,0),
       "\t-> function \"%s\" at file \"%s\":%u\n", 
              lua_constant[func->function]->str, func->file, line);
     line = func->line;
     func = func->next;
     if (err) lua_popfunction();
   } while (func);
  }
  else
  {
   sprintf (strchr(msg,0),
         "\n\tin statement begining at line %u of file \"%s\"", 
         lua_debugline, lua_filename());
  }
 }
 lua_pushstring(msg);
}

 
/*
** Internal function: do a string
*/
void lua_internaldostring (void)
{
 lua_Object obj = lua_getparam (1);
 if (lua_isstring(obj) && !lua_dostring(lua_getstring(obj)))
  lua_pushnumber(1);
 else
  lua_pushnil();
}

/*
** Internal function: do a file
*/
void lua_internaldofile (void)
{
 lua_Object obj = lua_getparam (1);
 if (lua_isstring(obj) && !lua_dofile(lua_getstring(obj)))
  lua_pushnumber(1);
 else
  lua_pushnil();
}
 
/*
** Internal function: print object values
*/
void lua_print (void)
{
 int i=1;
 lua_Object obj;
 while ((obj=lua_getparam (i++)) != LUA_NOOBJECT)
 {
  if      (lua_isnumber(obj))    printf("%g\n",lua_getnumber(obj));
  else if (lua_isstring(obj))    printf("%s\n",lua_getstring(obj));
  else if (lua_isfunction(obj))  printf("function: %p\n",(luaI_Address(obj))->value.tf);
  else if (lua_iscfunction(obj)) printf("cfunction: %p\n",lua_getcfunction(obj)
);
  else if (lua_isuserdata(obj))  printf("userdata: %p\n",lua_getuserdata(obj));
  else if (lua_istable(obj))     printf("table: %p\n",avalue(luaI_Address(obj)));
  else if (lua_isnil(obj))       printf("nil\n");
  else                           printf("invalid value to print\n");
 }
}


/*
** Internal function: return an object type.
*/
void luaI_type (void)
{
  lua_Object o = lua_getparam(1);
  int t;
  if (o == LUA_NOOBJECT)
    lua_error("no parameter to function 'type'");
  t = lua_type(o);
  switch (t)
  {
    case LUA_T_NIL :
      lua_pushliteral("nil");
      break;
    case LUA_T_NUMBER :
      lua_pushliteral("number");
      break;
    case LUA_T_STRING :
      lua_pushliteral("string");
      break;
    case LUA_T_ARRAY :
      lua_pushliteral("table");
      break;
    case LUA_T_FUNCTION :
      lua_pushliteral("function");
      break;
    case LUA_T_CFUNCTION :
      lua_pushliteral("cfunction");
      break;
    default :
      lua_pushliteral("userdata");
      break;
  }
  lua_pushnumber(t);
}
 
/*
** Internal function: convert an object to a number
*/
void lua_obj2number (void)
{
  lua_Object o = lua_getparam(1);
  if (lua_isnumber(o))
    lua_pushobject(o);
  else if (lua_isstring(o))
  {
    char c;
    float f;
    if (sscanf(lua_getstring(o),"%f %c",&f,&c) == 1)
      lua_pushnumber(f);
    else
      lua_pushnil();
  }
  else
    lua_pushnil();
}


void luaI_error (void)
{
  char *s = lua_getstring(lua_getparam(1));
  if (s == NULL) s = "(no message)";
  lua_error(s);
}

void luaI_getstack (void)
{
  char *s = lua_getstring(lua_getparam(1));
  if (s == NULL) s = "";
  luaI_reportbug(s, 0);
}

