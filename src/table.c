/*
** table.c
** Module to control static tables
*/

char *rcs_table="$Id: table.c,v 2.1 1994/04/20 22:07:57 celes Exp $";

#include <stdlib.h>
#include <string.h>

#include "mm.h"

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define streq(s1,s2)	(s1[0]==s2[0]&&strcmp(s1+1,s2+1)==0)

#ifndef MAXSYMBOL
#define MAXSYMBOL	512
#endif
static Symbol  		tablebuffer[MAXSYMBOL] = {
                                    {"type",{T_CFUNCTION,{lua_type}}},
                                    {"tonumber",{T_CFUNCTION,{lua_obj2number}}},
                                    {"next",{T_CFUNCTION,{lua_next}}},
                                    {"nextvar",{T_CFUNCTION,{lua_nextvar}}},
                                    {"print",{T_CFUNCTION,{lua_print}}},
                                    {"dofile",{T_CFUNCTION,{lua_internaldofile}}},
                                    {"dostring",{T_CFUNCTION,{lua_internaldostring}}}
                                                 };
Symbol	       	       *lua_table=tablebuffer;
Word   	 		lua_ntable=7;

struct List
{
 Symbol *s;
 struct List *next;
};

static struct List o6={ tablebuffer+6, 0};
static struct List o5={ tablebuffer+5, &o6 };
static struct List o4={ tablebuffer+4, &o5 };
static struct List o3={ tablebuffer+3, &o4 };
static struct List o2={ tablebuffer+2, &o3 };
static struct List o1={ tablebuffer+1, &o2 };
static struct List o0={ tablebuffer+0, &o1 };
static struct List *searchlist=&o0;

#ifndef MAXCONSTANT
#define MAXCONSTANT	256
#endif
/* pre-defined constants need garbage collection extra byte */ 
static char tm[] = " mark";
static char ti[] = " nil";
static char tn[] = " number";
static char ts[] = " string";
static char tt[] = " table";
static char tf[] = " function";
static char tc[] = " cfunction";
static char tu[] = " userdata";
static char  	       *constantbuffer[MAXCONSTANT] = {tm+1, ti+1,
						       tn+1, ts+1,
						       tt+1, tf+1,
						       tc+1, tu+1
                                                      };
char  	      	      **lua_constant = constantbuffer;
Word    		lua_nconstant=T_USERDATA+1;

#ifndef MAXSTRING
#define MAXSTRING	512
#endif
static char 	       *stringbuffer[MAXSTRING];
char  		      **lua_string = stringbuffer;
Word    		lua_nstring=0;

#define MAXFILE 	20
char  		       *lua_file[MAXFILE];
int      		lua_nfile;


#define markstring(s)   (*((s)-1))


/* Variables to controll garbage collection */
Word lua_block=10; /* to check when garbage collector will be called */
Word lua_nentity;   /* counter of new entities (strings and arrays) */


/*
** Given a name, search it at symbol table and return its index. If not
** found, allocate at end of table, checking oveflow and return its index.
** On error, return -1.
*/
int lua_findsymbol (char *s)
{
 struct List *l, *p;
 for (p=NULL, l=searchlist; l!=NULL; p=l, l=l->next)
  if (streq(s,l->s->name))
  {
   if (p!=NULL)
   {
    p->next = l->next;
    l->next = searchlist;
    searchlist = l;
   }
   return (l->s-lua_table);
  }

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
 s_tag(lua_ntable) = T_NIL;
 p = malloc(sizeof(*p)); 
 p->s = lua_table+lua_ntable;
 p->next = searchlist;
 searchlist = p;

 return lua_ntable++;
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
 if (tag(o) == T_STRING)
  markstring (svalue(o)) = 1;
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

 lua_stringcollector();
 lua_hashcollector();

 lua_nentity = 0;      /* reset counter */
} 

/*
** Garbage collection to atrings.
** Delete all unmarked strings
*/
void lua_stringcollector (void)
{
 int i, j;
 for (i=j=0; i<lua_nstring; i++)
  if (markstring(lua_string[i]) == 1)
  {
   lua_string[j++] = lua_string[i];
   markstring(lua_string[i]) = 0;
  }
  else
  {
   free (lua_string[i]-1);
  }
 lua_nstring = j;
}

/*
** Allocate a new string at string table. The given string is already 
** allocated with mark space and the function puts it at the end of the
** table, checking overflow, and returns its own pointer, or NULL on error.
*/
char *lua_createstring (char *s)
{
 int i;
 if (s == NULL) return NULL;
 
 for (i=0; i<lua_nstring; i++)
  if (streq(s,lua_string[i]))
  {
   free(s-1);
   return lua_string[i];
  }

 if (lua_nentity == lua_block || lua_nstring >= MAXSTRING-1)
 {
  lua_pack ();
  if (lua_nstring >= MAXSTRING-1)
  {
   lua_error ("string table overflow");
   return NULL;
  }
 } 
 lua_string[lua_nstring++] = s;
 lua_nentity++;
 return s;
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
  while (index < lua_ntable && tag(&s_object(index)) == T_NIL) index++;
  
  if (index == lua_ntable)
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
