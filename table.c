/*
** table.c
** Module to control static tables
*/

char *rcs_table="$Id: table.c,v 2.65 1997/03/31 14:17:09 roberto Exp roberto $";

#include "luamem.h"
#include "auxlib.h"
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
Word lua_ntable = 0;
static Long lua_maxsymbol = 0;

TaggedString **lua_constant = NULL;
Word lua_nconstant = 0;
static Long lua_maxconstant = 0;


#define GARBAGE_BLOCK 50


void luaI_initsymbol (void)
{
  lua_maxsymbol = BUFFER_BLOCK;
  lua_table = newvector(lua_maxsymbol, Symbol);
  luaI_predefine();
}


/*
** Initialise constant table with pre-defined constants
*/
void luaI_initconstant (void)
{
 lua_maxconstant = BUFFER_BLOCK;
 lua_constant = newvector(lua_maxconstant, TaggedString *);
 /* pre-register mem error messages, to avoid loop when error arises */
 luaI_findconstantbyname(tableEM);
 luaI_findconstantbyname(memEM);
}


/*
** Given a name, search it at symbol table and return its index. If not
** found, allocate it.
*/
Word luaI_findsymbol (TaggedString *t)
{
 if (t->varindex == NOT_USED)
 {
  if (lua_ntable == lua_maxsymbol)
    lua_maxsymbol = growvector(&lua_table, lua_maxsymbol, Symbol,
                      symbolEM, MAX_WORD);
  t->varindex = lua_ntable;
  lua_table[lua_ntable].varname = t;
  s_ttype(lua_ntable) = LUA_T_NIL;
  lua_ntable++;
 }
 return t->varindex;
}


Word luaI_findsymbolbyname (char *name)
{
  return luaI_findsymbol(luaI_createfixedstring(name));
}


/*
** Given a tree node, check it is has a correspondent constant index. If not,
** allocate it.
*/
Word luaI_findconstant (TaggedString *t)
{
 if (t->constindex == NOT_USED)
 {
  if (lua_nconstant == lua_maxconstant)
    lua_maxconstant = growvector(&lua_constant, lua_maxconstant, TaggedString *,
                        constantEM, MAX_WORD);
  t->constindex = lua_nconstant;
  lua_constant[lua_nconstant] = t;
  lua_nconstant++;
 }
 return t->constindex;
}


Word  luaI_findconstantbyname (char *name)
{
  return luaI_findconstant(luaI_createfixedstring(name));
}

TaggedString *luaI_createfixedstring (char *name)
{
  TaggedString *ts = lua_createstring(name);
  if (!ts->marked)
    ts->marked = 2;  /* avoid GC */
  return ts;
}


/*
** Traverse symbol table objects
*/
static char *lua_travsymbol (int (*fn)(TObject *))
{
 Word i;
 for (i=0; i<lua_ntable; i++)
  if (fn(&s_object(i)))
    return lua_table[i].varname->str;
 return NULL;
}


/*
** Mark an object if it is a string or a unmarked array.
*/
int lua_markobject (TObject *o)
{/* if already marked, does not change mark value */
 if (ttype(o) == LUA_T_USERDATA ||
     (ttype(o) == LUA_T_STRING && !tsvalue(o)->marked))
   tsvalue(o)->marked = 1;
 else if (ttype(o) == LUA_T_ARRAY)
   lua_hashmark (avalue(o));
 else if ((o->ttype == LUA_T_FUNCTION || o->ttype == LUA_T_MARK)
           && !o->value.tf->marked)
   o->value.tf->marked = 1;
 return 0;
}

/*
* returns 0 if the object is going to be (garbage) collected
*/
int luaI_ismarked (TObject *o)
{
  switch (o->ttype)
  {
   case LUA_T_STRING:
     return o->value.ts->marked;
   case LUA_T_FUNCTION:
    return o->value.tf->marked;
   case LUA_T_ARRAY:
    return o->value.a->mark;
   default:  /* nil, number, cfunction, or user data */
    return 1;
  }
}


static void call_nilIM (void)
{ /* signals end of garbage collection */
  TObject t;
  ttype(&t) = LUA_T_NIL;
  luaI_gcIM(&t);  /* end of list */
}

/*
** Garbage collection. 
** Delete all unused strings and arrays.
*/
Long luaI_collectgarbage (void)
{
  Long recovered = 0;
  lua_travstack(lua_markobject); /* mark stack objects */
  lua_travsymbol(lua_markobject); /* mark symbol table objects */
  luaI_travlock(lua_markobject); /* mark locked objects */
  luaI_travfallbacks(lua_markobject);  /* mark fallbacks */
  luaI_hashcallIM();
  luaI_strcallIM();
  call_nilIM();
  luaI_invalidaterefs();
  recovered += lua_strcollector();
  recovered += lua_hashcollector();
  recovered += luaI_funccollector();
  return recovered;
} 

void lua_pack (void)
{
  static unsigned long block = GARBAGE_BLOCK;
  static unsigned long nentity = 0;  /* total of strings, arrays, etc */
  unsigned long recovered = 0;
  if (nentity++ < block) return;
  recovered = luaI_collectgarbage();
  block = 2*(block-recovered);
  nentity -= recovered;
} 


/*
** Internal function: return next global variable
*/
void luaI_nextvar (void)
{
  Word next;
  if (lua_isnil(lua_getparam(1)))
    next = 0;
  else 
    next = luaI_findsymbolbyname(luaL_check_string(1, "nextvar")) + 1;
  while (next < lua_ntable && s_ttype(next) == LUA_T_NIL)
    next++;
  if (next < lua_ntable) {
    lua_pushstring(lua_table[next].varname->str);
    luaI_pushobject(&s_object(next));
  }
}


static TObject *functofind;
static int checkfunc (TObject *o)
{
  if (o->ttype == LUA_T_FUNCTION)
    return
       ((functofind->ttype == LUA_T_FUNCTION || functofind->ttype == LUA_T_MARK)
            && (functofind->value.tf == o->value.tf));
  if (o->ttype == LUA_T_CFUNCTION)
    return
       ((functofind->ttype == LUA_T_CFUNCTION ||
         functofind->ttype == LUA_T_CMARK) &&
         (functofind->value.f == o->value.f));
  return 0;
}


char *lua_getobjname (lua_Object o, char **name)
{ /* try to find a name for given function */
  functofind = luaI_Address(o);
  if ((*name = luaI_travfallbacks(checkfunc)) != NULL)
    return "fallback";
  else if ((*name = lua_travsymbol(checkfunc)) != NULL)
    return "global";
  else return "";
}

