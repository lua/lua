/*
** table.c
** Module to control static tables
*/

char *rcs_table="$Id: table.c,v 2.28 1995/01/18 20:15:54 celes Exp $";

#include <string.h>

#include "mem.h"
#include "opcode.h"
#include "tree.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"
#include "fallback.h"


#define BUFFER_BLOCK 256

Symbol *lua_table;
static Word lua_ntable = 0;
static Long lua_maxsymbol = 0;

TaggedString **lua_constant;
static Word lua_nconstant = 0;
static Long lua_maxconstant = 0;



#define MAXFILE 	20
char  		       *lua_file[MAXFILE];
int      		lua_nfile;

#define GARBAGE_BLOCK 256
#define MIN_GARBAGE_BLOCK 10

static void lua_nextvar (void);
static void setglobal (void);
static void getglobal (void);

/*
** Initialise symbol table with internal functions
*/
static void lua_initsymbol (void)
{
 Word n;
 lua_maxsymbol = BUFFER_BLOCK;
 lua_table = newvector(lua_maxsymbol, Symbol);
 n = luaI_findsymbolbyname("next");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = lua_next;
 n = luaI_findsymbolbyname("dofile");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = lua_internaldofile;
 n = luaI_findsymbolbyname("setglobal");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = setglobal;
 n = luaI_findsymbolbyname("getglobal");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = getglobal;
 n = luaI_findsymbolbyname("nextvar");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = lua_nextvar;
 n = luaI_findsymbolbyname("type"); 
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = luaI_type;
 n = luaI_findsymbolbyname("tonumber");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = lua_obj2number;
 n = luaI_findsymbolbyname("print");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = lua_print;
 n = luaI_findsymbolbyname("dostring");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = lua_internaldostring;
 n = luaI_findsymbolbyname("setfallback");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = luaI_setfallback;
 n = luaI_findsymbolbyname("error");
 s_tag(n) = LUA_T_CFUNCTION; s_fvalue(n) = luaI_error;
}


/*
** Initialise constant table with pre-defined constants
*/
void lua_initconstant (void)
{
 lua_maxconstant = BUFFER_BLOCK;
 lua_constant = newvector(lua_maxconstant, TaggedString *);
}


/*
** Given a name, search it at symbol table and return its index. If not
** found, allocate it.
*/
Word luaI_findsymbol (TreeNode *t)
{
 if (lua_table == NULL)
  lua_initsymbol(); 
 if (t->varindex == NOT_USED)
 {
  if (lua_ntable == lua_maxsymbol)
  {
   if (lua_maxsymbol >= MAX_WORD)
     lua_error("symbol table overflow");
   lua_maxsymbol *= 2;
   if (lua_maxsymbol >= MAX_WORD)
     lua_maxsymbol = MAX_WORD; 
   lua_table = growvector(lua_table, lua_maxsymbol, Symbol);
  }
  t->varindex = lua_ntable;
  s_tag(lua_ntable) = LUA_T_NIL;
  lua_ntable++;
 }
 return t->varindex;
}


Word luaI_findsymbolbyname (char *name)
{
  return luaI_findsymbol(lua_constcreate(name));
}


/*
** Given a name, search it at constant table and return its index. If not
** found, allocate it.
** On error, return -1.
*/
Word luaI_findconstant (TreeNode *t)
{
 if (lua_constant == NULL)
  lua_initconstant();
 if (t->constindex == NOT_USED)
 {
  if (lua_nconstant == lua_maxconstant)
  {
   if (lua_maxconstant >= MAX_WORD)
     lua_error("constant table overflow");
   lua_maxconstant *= 2;
   if (lua_maxconstant >= MAX_WORD)
     lua_maxconstant = MAX_WORD;
   lua_constant = growvector(lua_constant, lua_maxconstant, TaggedString *);
  }
  t->constindex = lua_nconstant;
  lua_constant[lua_nconstant] = &(t->ts);
  lua_nconstant++;
 }
 return t->constindex;
}


/*
** Traverse symbol table objects
*/
void lua_travsymbol (void (*fn)(Object *))
{
 Word i;
 for (i=0; i<lua_ntable; i++)
  fn(&s_object(i));
}


/*
** Mark an object if it is a string or a unmarked array.
*/
void lua_markobject (Object *o)
{
 if (tag(o) == LUA_T_STRING && !tsvalue(o)->marked)
   tsvalue(o)->marked = 1;
 else if (tag(o) == LUA_T_ARRAY)
   lua_hashmark (avalue(o));
}


/*
** Garbage collection. 
** Delete all unused strings and arrays.
*/
void lua_pack (void)
{
  static Long block = GARBAGE_BLOCK; /* when garbage collector will be called */
  static Long nentity = 0;  /* counter of new entities (strings and arrays) */
  Long recovered = 0;
  if (nentity++ < block) return;
  lua_travstack(lua_markobject); /* mark stack objects */
  lua_travsymbol(lua_markobject); /* mark symbol table objects */
  luaI_travlock(lua_markobject); /* mark locked objects */
  recovered += lua_strcollector();
  recovered += lua_hashcollector();
  nentity = 0;				/* reset counter */
  block=(16*block-7*recovered)/12;	/* adapt block size */
  if (block < MIN_GARBAGE_BLOCK) block = MIN_GARBAGE_BLOCK;
} 


/*
** Add a file name at file table, checking overflow. This function also set
** the external variable "lua_filename" with the function filename set.
** Return 0 on success or error message on error.
*/
char *lua_addfile (char *fn)
{
 if (lua_nfile >= MAXFILE)
   return "too many files";
 if ((lua_file[lua_nfile++] = luaI_strdup (fn)) == NULL)
   return "not enough memory";
 return NULL;
}

/*
** Delete a file from file stack
*/
int lua_delfile (void)
{
 luaI_free(lua_file[--lua_nfile]); 
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
static void lua_nextvar (void)
{
 char *varname;
 TreeNode *next;
 lua_Object o = lua_getparam(1);
 if (o == LUA_NOOBJECT)
   lua_reportbug("too few arguments to function `nextvar'");
 if (lua_getparam(2) != LUA_NOOBJECT)
   lua_reportbug("too many arguments to function `nextvar'");
 if (lua_isnil(o))
   varname = NULL;
 else if (!lua_isstring(o))
 {
   lua_reportbug("incorrect argument to function `nextvar'"); 
   return;  /* to avoid warnings */
 }
 else
   varname = lua_getstring(o);
 next = lua_varnext(varname);
 if (next == NULL)
 {
  lua_pushnil();
  lua_pushnil();
 }
 else
 {
  Object name;
  tag(&name) = LUA_T_STRING;
  tsvalue(&name) = &(next->ts);
  luaI_pushobject(&name);
  luaI_pushobject(&s_object(next->varindex));
 }
}


static void setglobal (void)
{
  lua_Object name = lua_getparam(1);
  lua_Object value = lua_getparam(2);
  if (!lua_isstring(name))
    lua_reportbug("incorrect argument to function `setglobal'");
  lua_pushobject(value);
  lua_storeglobal(lua_getstring(name));
}


static void getglobal (void)
{
  lua_Object name = lua_getparam(1);
  if (!lua_isstring(name))
    lua_reportbug("incorrect argument to function `getglobal'");
  lua_pushobject(lua_getglobal(lua_getstring(name)));
}
