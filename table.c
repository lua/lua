/*
** table.c
** Module to control static tables
** TeCGraf - PUC-Rio
** 11 May 93
*/

#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define streq(s1,s2)	(strcmp(s1,s2)==0)

#ifndef MAXSYMBOL
#define MAXSYMBOL	512
#endif
static Symbol  		tablebuffer[MAXSYMBOL] = {
                                    {"type",{T_CFUNCTION,{lua_type}}},
                                    {"tonumber",{T_CFUNCTION,{lua_obj2number}}},
                                    {"next",{T_CFUNCTION,{lua_next}}},
                                    {"nextvar",{T_CFUNCTION,{lua_nextvar}}},
                                    {"print",{T_CFUNCTION,{lua_print}}}
                                                 };
Symbol	       	       *lua_table=tablebuffer;
Word   	 		lua_ntable=5;

#ifndef MAXCONSTANT
#define MAXCONSTANT	256
#endif
static char  	       *constantbuffer[MAXCONSTANT] = {"mark","nil","number",
                                                       "string","table",
                                                       "function","cfunction"
                                                      };
char  	      	      **lua_constant = constantbuffer;
Word    		lua_nconstant=T_CFUNCTION+1;

#ifndef MAXSTRING
#define MAXSTRING	512
#endif
static char 	       *stringbuffer[MAXSTRING];
char  		      **lua_string = stringbuffer;
Word    		lua_nstring=0;

#ifndef MAXARRAY
#define MAXARRAY	512
#endif
static Hash             *arraybuffer[MAXARRAY];
Hash  	              **lua_array = arraybuffer;
Word    		lua_narray=0;

#define MAXFILE 	20
char  		       *lua_file[MAXFILE];
int      		lua_nfile;


/*
** Given a name, search it at symbol table and return its index. If not
** found, allocate at end of table, checking oveflow and return its index.
** On error, return -1.
*/
int lua_findsymbol (char *s)
{
 int i;
 for (i=0; i<lua_ntable; i++)
  if (streq(s,s_name(i)))
   return i;
 if (lua_ntable >= MAXSYMBOL-1)
 {
  lua_error ("symbol table overflow");
  return -1;
 }
 s_name(lua_ntable) = strdup(s);
 if (s_name(lua_ntable) == NULL)
 {
  lua_error ("not enough memory");
  return -1;
 }
 s_tag(lua_ntable++) = T_NIL;
 
 return (lua_ntable-1);
}

/*
** Given a constant string, eliminate its delimeters (" or '), search it at 
** constant table and return its index. If not found, allocate at end of 
** the table, checking oveflow and return its index.
**
** For each allocation, the function allocate a extra char to be used to
** mark used string (it's necessary to deal with constant and string 
** uniformily). The function store at the table the second position allocated,
** that represents the beginning of the real string. On error, return -1.
** 
*/
int lua_findenclosedconstant (char *s)
{
 int i, j, l=strlen(s);
 char *c = calloc (l, sizeof(char)); 	/* make a copy */
 
 c++;		/* create mark space */

 /* introduce scape characters */ 
 for (i=1,j=0; i<l-1; i++)
 {
  if (s[i] == '\\')
  {
   switch (s[++i])
   {
    case 'n': c[j++] = '\n'; break;
    case 't': c[j++] = '\t'; break;
    case 'r': c[j++] = '\r'; break;
    default : c[j++] = '\\'; c[j++] = c[i]; break;
   }
  }
  else
   c[j++] = s[i];
 }
 c[j++] = 0;
 
 for (i=0; i<lua_nconstant; i++)
  if (streq(c,lua_constant[i]))
  {
   free (c-1);
   return i;
  }
 if (lua_nconstant >= MAXCONSTANT-1)
 {
  lua_error ("lua: constant string table overflow"); 
  return -1;
 }
 lua_constant[lua_nconstant++] = c;
 return (lua_nconstant-1);
}

/*
** Given a constant string, search it at constant table and return its index.
** If not found, allocate at end of the table, checking oveflow and return 
** its index.
**
** For each allocation, the function allocate a extra char to be used to
** mark used string (it's necessary to deal with constant and string 
** uniformily). The function store at the table the second position allocated,
** that represents the beginning of the real string. On error, return -1.
** 
*/
int lua_findconstant (char *s)
{
 int i;
 for (i=0; i<lua_nconstant; i++)
  if (streq(s,lua_constant[i]))
   return i;
 if (lua_nconstant >= MAXCONSTANT-1)
 {
  lua_error ("lua: constant string table overflow"); 
  return -1;
 }
 {
  char *c = calloc(strlen(s)+2,sizeof(char));
  c++;		/* create mark space */
  lua_constant[lua_nconstant++] = strcpy(c,s);
 }
 return (lua_nconstant-1);
}


/*
** Mark an object if it is a string or a unmarked array.
*/
void lua_markobject (Object *o)
{
 if (tag(o) == T_STRING)
  lua_markstring (svalue(o)) = 1;
 else if (tag(o) == T_ARRAY && markarray(avalue(o)) == 0)
   lua_hashmark (avalue(o));
}

/*
** Mark all strings and arrays used by any object stored at symbol table.
*/
static void lua_marktable (void)
{
 int i;
 for (i=0; i<lua_ntable; i++)
  lua_markobject (&s_object(i));
} 

/*
** Simulate a garbage colection. When string table or array table overflows,
** this function check if all allocated strings and arrays are in use. If
** there are unused ones, pack (compress) the tables.
*/
static void lua_pack (void)
{
 lua_markstack ();
 lua_marktable ();
 
 { /* pack string */
  int i, j;
  for (i=j=0; i<lua_nstring; i++)
   if (lua_markstring(lua_string[i]) == 1)
   {
    lua_string[j++] = lua_string[i];
    lua_markstring(lua_string[i]) = 0;
   }
   else
   {
    free (lua_string[i]-1);
   }
  lua_nstring = j;
 }

 { /* pack array */
  int i, j;
  for (i=j=0; i<lua_narray; i++)
   if (markarray(lua_array[i]) == 1)
   {
    lua_array[j++] = lua_array[i];
    markarray(lua_array[i]) = 0;
   }
   else
   {
    lua_hashdelete (lua_array[i]);
   }
  lua_narray = j;
 }
}

/*
** Allocate a new string at string table. The given string is already 
** allocated with mark space and the function puts it at the end of the
** table, checking overflow, and returns its own pointer, or NULL on error.
*/
char *lua_createstring (char *s)
{
 if (s == NULL) return NULL;
 
 if (lua_nstring >= MAXSTRING-1)
 {
  lua_pack ();
  if (lua_nstring >= MAXSTRING-1)
  {
   lua_error ("string table overflow");
   return NULL;
  }
 } 
 lua_string[lua_nstring++] = s;
 return s;
}

/*
** Allocate a new array, already created, at array table. The function puts 
** it at the end of the table, checking overflow, and returns its own pointer,
** or NULL on error.
*/
void *lua_createarray (void *a)
{
 if (a == NULL) return NULL;
 
 if (lua_narray >= MAXARRAY-1)
 {
  lua_pack ();
  if (lua_narray >= MAXARRAY-1)
  {
   lua_error ("indexed table overflow");
   return NULL;
  }
 } 
 lua_array[lua_narray++] = a;
 return a;
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
 int index;
 Object *o = lua_getparam (1);
 if (o == NULL)
 { lua_error ("too few arguments to function `nextvar'"); return; }
 if (lua_getparam (2) != NULL)
 { lua_error ("too many arguments to function `nextvar'"); return; }
 if (tag(o) == T_NIL)
 {
  index = 0;
 }
 else if (tag(o) != T_STRING) 
 { 
  lua_error ("incorrect argument to function `nextvar'"); 
  return;
 }
 else
 {
  for (index=0; index<lua_ntable; index++)
   if (streq(s_name(index),svalue(o))) break;
  if (index == lua_ntable) 
  {
   lua_error ("name not found in function `nextvar'");
   return;
  }
  index++;
  while (index < lua_ntable-1 && tag(&s_object(index)) == T_NIL) index++;
  
  if (index == lua_ntable-1)
  {
   lua_pushnil();
   lua_pushnil();
   return;
  }
 }
 {
  Object name;
  tag(&name) = T_STRING;
  svalue(&name) = lua_createstring(lua_strdup(s_name(index)));
  if (lua_pushobject (&name)) return;
  if (lua_pushobject (&s_object(index))) return;
 }
}
