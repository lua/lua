/*
** table.c
** Module to control static tables
*/

char *rcs_table="$Id: table.c,v 2.38 1995/11/03 15:30:50 roberto Exp $";

/*#include <string.h>*/

#include "mem.h"
#include "opcode.h"
#include "tree.h"
#include "hash.h"
#include "table.h"
#include "inout.h"
#include "lua.h"
#include "fallback.h"
#include "luadebug.h"


#define BUFFER_BLOCK 256

Symbol *lua_table = NULL;
static Word lua_ntable = 0;
static Long lua_maxsymbol = 0;

TaggedString **lua_constant = NULL;
static Word lua_nconstant = 0;
static Long lua_maxconstant = 0;


#define GARBAGE_BLOCK 1024
#define MIN_GARBAGE_BLOCK (GARBAGE_BLOCK/2)

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
** Given a tree node, check it is has a correspondent constant index. If not,
** allocate it.
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


Word  luaI_findconstantbyname (char *name)
{
  return luaI_findconstant(lua_constcreate(name));
}


/*
** Traverse symbol table objects
*/
static char *lua_travsymbol (int (*fn)(Object *))
{
 Word i;
 for (i=0; i<lua_ntable; i++)
  if (fn(&s_object(i)))
    return luaI_nodebysymbol(i)->ts.str;
 return NULL;
}


/*
** Mark an object if it is a string or a unmarked array.
*/
int lua_markobject (Object *o)
{
 if (tag(o) == LUA_T_STRING && !tsvalue(o)->marked)
   tsvalue(o)->marked = 1;
 else if (tag(o) == LUA_T_ARRAY)
   lua_hashmark (avalue(o));
 else if ((o->tag == LUA_T_FUNCTION || o->tag == LUA_T_MARK)
           && !o->value.tf->marked)
   o->value.tf->marked = 1;
 return 0;
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
  luaI_travfallbacks(lua_markobject);  /* mark fallbacks */
  recovered += lua_strcollector();
  recovered += lua_hashcollector();
  recovered += luaI_funccollector();
  nentity = 0;				/* reset counter */
  block=(16*block-7*recovered)/12;	/* adapt block size */
  if (block < MIN_GARBAGE_BLOCK) block = MIN_GARBAGE_BLOCK;
} 


/*
** Internal function: return next global variable
*/
static void lua_nextvar (void)
{
 Word next;
 lua_Object o = lua_getparam(1);
 if (o == LUA_NOOBJECT)
   lua_error("too few arguments to function `nextvar'");
 if (lua_getparam(2) != LUA_NOOBJECT)
   lua_error("too many arguments to function `nextvar'");
 if (lua_isnil(o))
   next = 0;
 else if (!lua_isstring(o))
 {
   lua_error("incorrect argument to function `nextvar'"); 
   return;  /* to avoid warnings */
 }
 else
   next = luaI_findsymbolbyname(lua_getstring(o)) + 1;
 while (next < lua_ntable && s_tag(next) == LUA_T_NIL) next++;
 if (next >= lua_ntable)
 {
  lua_pushnil();
  lua_pushnil();
 }
 else
 {
  TreeNode *t = luaI_nodebysymbol(next);
  Object name;
  tag(&name) = LUA_T_STRING;
  tsvalue(&name) = &(t->ts);
  luaI_pushobject(&name);
  luaI_pushobject(&s_object(next));
 }
}


static void setglobal (void)
{
  lua_Object name = lua_getparam(1);
  lua_Object value = lua_getparam(2);
  if (!lua_isstring(name))
    lua_error("incorrect argument to function `setglobal'");
  lua_pushobject(value);
  lua_storeglobal(lua_getstring(name));
}


static void getglobal (void)
{
  lua_Object name = lua_getparam(1);
  if (!lua_isstring(name))
    lua_error("incorrect argument to function `getglobal'");
  lua_pushobject(lua_getglobal(lua_getstring(name)));
}


static Object *functofind;
static int checkfunc (Object *o)
{
  if (o->tag == LUA_T_FUNCTION)
    return
       ((functofind->tag == LUA_T_FUNCTION || functofind->tag == LUA_T_MARK)
            && (functofind->value.tf == o->value.tf));
  if (o->tag == LUA_T_CFUNCTION)
    return
       ((functofind->tag == LUA_T_CFUNCTION || functofind->tag == LUA_T_CMARK)
            && (functofind->value.f == o->value.f));
  return 0;
}


char *getobjname (lua_Object o, char **name)
{ /* try to find a name for given function */
  functofind = luaI_Address(o);
  if ((*name = luaI_travfallbacks(checkfunc)) != NULL)
    return "fallback";
  else if ((*name = lua_travsymbol(checkfunc)) != NULL)
    return "global";
  else return "";
}

