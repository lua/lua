/*
** table.c
** Module to control static tables
*/

char *rcs_table="$Id: table.c,v 2.3 1994/08/03 14:15:46 celes Exp $";

#include <stdlib.h>
#include <string.h>

#include "mm.h"

#include "opcode.h"
#include "tree.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define streq(s1,s2)	(s1[0]==s2[0]&&strcmp(s1+1,s2+1)==0)

#define BUFFER_BLOCK 256

Symbol *lua_table;
static Word lua_ntable = 0;
static Long lua_maxsymbol = 0;

char **lua_constant;
static Word lua_nconstant = 0;
static Long lua_maxconstant = 0;



#define MAXFILE 	20
char  		       *lua_file[MAXFILE];
int      		lua_nfile;

/* Variables to controll garbage collection */
#define GARBAGE_BLOCK 256
Word lua_block=GARBAGE_BLOCK; /* when garbage collector will be called */
Word lua_nentity;   /* counter of new entities (strings and arrays) */
Word lua_recovered;   /* counter of recovered entities (strings and arrays) */


/*
** Initialise symbol table with internal functions
*/
static void lua_initsymbol (void)
{
 int n;
 lua_maxsymbol = BUFFER_BLOCK;
 lua_table = (Symbol *) calloc(lua_maxsymbol, sizeof(Symbol));
 if (lua_table == NULL)
 {
  lua_error ("symbol table: not enough memory");
  return;
 }
 n = lua_findsymbol("type"); 
 s_tag(n) = T_CFUNCTION; s_fvalue(n) = lua_type;
 n = lua_findsymbol("tonumber");
 s_tag(n) = T_CFUNCTION; s_fvalue(n) = lua_obj2number;
 n = lua_findsymbol("next");
 s_tag(n) = T_CFUNCTION; s_fvalue(n) = lua_next;
 n = lua_findsymbol("nextvar");
 s_tag(n) = T_CFUNCTION; s_fvalue(n) = lua_nextvar;
 n = lua_findsymbol("print");
 s_tag(n) = T_CFUNCTION; s_fvalue(n) = lua_print;
 n = lua_findsymbol("dofile");
 s_tag(n) = T_CFUNCTION; s_fvalue(n) = lua_internaldofile;
 n = lua_findsymbol("dostring");
 s_tag(n) = T_CFUNCTION; s_fvalue(n) = lua_internaldostring;
}


/*
** Initialise constant table with pre-defined constants
*/
void lua_initconstant (void)
{
 lua_maxconstant = BUFFER_BLOCK;
 lua_constant = (char **) calloc(lua_maxconstant, sizeof(char *));
 if (lua_constant == NULL)
 {
  lua_error ("constant table: not enough memory");
  return;
 }
 lua_findconstant("mark");
 lua_findconstant("nil");
 lua_findconstant("number");
 lua_findconstant("string");
 lua_findconstant("table");
 lua_findconstant("function");
 lua_findconstant("cfunction");
 lua_findconstant("userdata");
}

/*
** Given a name, search it at symbol table and return its index. If not
** found, allocate it.
** On error, return -1.
*/
int lua_findsymbol (char *s)
{
 char *n; 
 if (lua_table == NULL)
  lua_initsymbol(); 
 n = lua_varcreate(s);
 if (n == NULL)
 {
  lua_error ("create symbol: not enough memory");
  return -1;
 }
 if (indexstring(n) == UNMARKED_STRING)
 {
  if (lua_ntable == lua_maxsymbol)
  {
   lua_maxsymbol *= 2;
   if (lua_maxsymbol > MAX_WORD)
   {
    lua_error("symbol table overflow");
    return -1;
   }
   lua_table = (Symbol *)realloc(lua_table, lua_maxsymbol*sizeof(Symbol));
   if (lua_table == NULL)
   {
    lua_error ("symbol table: not enough memory");
    return -1;
   }
  }
  indexstring(n) = lua_ntable;
  s_tag(lua_ntable) = T_NIL;
  lua_ntable++;
 }
 return indexstring(n);
}


/*
** Given a name, search it at constant table and return its index. If not
** found, allocate it.
** On error, return -1.
*/
int lua_findconstant (char *s)
{
 char *n;
 if (lua_constant == NULL)
  lua_initconstant();
 n = lua_constcreate(s);
 if (n == NULL)
 {
  lua_error ("create constant: not enough memory");
  return -1;
 }
 if (indexstring(n) == UNMARKED_STRING)
 {
  if (lua_nconstant == lua_maxconstant)
  {
   lua_maxconstant *= 2;
   if (lua_maxconstant > MAX_WORD)
   {
    lua_error("constant table overflow");
    return -1;
   }
   lua_constant = (char**)realloc(lua_constant,lua_maxconstant*sizeof(char*));
   if (lua_constant == NULL)
   {
    lua_error ("constant table: not enough memory");
    return -1;
   }
  }
  indexstring(n) = lua_nconstant;
  lua_constant[lua_nconstant] = n;
  lua_nconstant++;
 }
 return indexstring(n);
}


/*
** Traverse symbol table objects
*/
void lua_travsymbol (void (*fn)(Object *))
{
 int i;
 for (i=0; i<lua_ntable; i++)
  fn(&s_object(i));
}


/*
** Mark an object if it is a string or a unmarked array.
*/
void lua_markobject (Object *o)
{
 if (tag(o) == T_STRING && indexstring(svalue(o)) == UNMARKED_STRING)
  indexstring(svalue(o)) = MARKED_STRING;
 else if (tag(o) == T_ARRAY)
  lua_hashmark (avalue(o));
}


/*
** Garbage collection. 
** Delete all unused strings and arrays.
*/
void lua_pack (void)
{
 /* mark stack strings */
 lua_travstack(lua_markobject);
 
 /* mark symbol table strings */
 lua_travsymbol(lua_markobject);

 lua_recovered=0; 

 lua_strcollector();
 lua_hashcollector();

printf("lua_pack: lua_block=%d lua_recovered=%d %%=%.2f\n",lua_block,lua_recovered,100.0*lua_recovered/lua_block);

 lua_nentity = 0;				/* reset counter */
 lua_block=2*lua_block-3*lua_recovered/2;	/* adapt block size */
} 


/*
** If the string isn't allocated, allocate a new string at string tree.
*/
char *lua_createstring (char *s)
{
 if (s == NULL) return NULL;
 
 return lua_strcreate(s);
}


/*
** Add a file name at file table, checking overflow. This function also set
** the external variable "lua_filename" with the function filename set.
** Return 0 on success or 1 on error.
*/
int lua_addfile (char *fn)
{
 if (lua_nfile >= MAXFILE-1)
 {
  lua_error ("too many files");
  return 1;
 }
 if ((lua_file[lua_nfile++] = strdup (fn)) == NULL)
 {
  lua_error ("not enough memory");
  return 1;
 }
 return 0;
}

/*
** Delete a file from file stack
*/
int lua_delfile (void)
{
 lua_nfile--; 
 return 1;
}

/*
** Return the last file name set.
*/
char *lua_filename (void)
{
 return lua_file[lua_nfile-1];
}

/*
** Internal function: return next global variable
*/
void lua_nextvar (void)
{
 char *varname, *next;
 Object *o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `nextvar'"); return; }
 if (lua_getparam (2) != NULL)
 { lua_error ("too many arguments to function `nextvar'"); return; }
 if (tag(o) == T_NIL)
 {
  varname = 0;
 }
 else if (tag(o) != T_STRING) 
 { 
  lua_error ("incorrect argument to function `nextvar'"); 
  return;
 }
 else
 {
  varname = svalue(o);
 }
 next = lua_varnext(varname);
 if (next == NULL)
 {
  lua_pushnil();
  lua_pushnil();
 }
 else
 {
  Object name;
  tag(&name) = T_STRING;
  svalue(&name) = next;
  if (lua_pushobject (&name)) return;
  if (lua_pushobject (&s_object(indexstring(next)))) return;
 }
}
