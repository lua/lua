/*
** table.c
** Module to control static tables
*/

char *rcs_table="$Id: table.c,v 2.72 1997/06/17 18:09:31 roberto Exp $";

#include "luamem.h"
#include "auxlib.h"
#include "func.h"
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


#define GARBAGE_BLOCK 100


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
 if (t->u.s.varindex == NOT_USED)
 {
  if (lua_ntable == lua_maxsymbol)
    lua_maxsymbol = growvector(&lua_table, lua_maxsymbol, Symbol,
                      symbolEM, MAX_WORD);
  t->u.s.varindex = lua_ntable;
  lua_table[lua_ntable].varname = t;
  s_ttype(lua_ntable) = LUA_T_NIL;
  lua_ntable++;
 }
 return t->u.s.varindex;
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
 if (t->u.s.constindex == NOT_USED)
 {
  if (lua_nconstant == lua_maxconstant)
    lua_maxconstant = growvector(&lua_constant, lua_maxconstant, TaggedString *,
                        constantEM, MAX_WORD);
  t->u.s.constindex = lua_nconstant;
  lua_constant[lua_nconstant] = t;
  lua_nconstant++;
 }
 return t->u.s.constindex;
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


int luaI_globaldefined (char *name)
{
  return ttype(&lua_table[luaI_findsymbolbyname(name)].object) != LUA_T_NIL;
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
   case LUA_T_STRING: case LUA_T_USERDATA:
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
static long gc_block = GARBAGE_BLOCK;
static long gc_nentity = 0;  /* total of strings, arrays, etc */

static void markall (void)
{
  lua_travstack(lua_markobject); /* mark stack objects */
  lua_travsymbol(lua_markobject); /* mark symbol table objects */
  luaI_travlock(lua_markobject); /* mark locked objects */
  luaI_travfallbacks(lua_markobject);  /* mark fallbacks */
}


long lua_collectgarbage (long limit)
{
  long recovered = 0;
  Hash *freetable;
  TaggedString *freestr;
  TFunc *freefunc;
  markall();
  luaI_invalidaterefs();
  freetable = luaI_hashcollector(&recovered);
  freestr = luaI_strcollector(&recovered);
  freefunc = luaI_funccollector(&recovered);
  gc_nentity -= recovered;
  gc_block = (limit == 0) ? 2*(gc_block-recovered) : gc_nentity+limit;
  luaI_hashcallIM(freetable);
  luaI_strcallIM(freestr);
  call_nilIM();
  luaI_hashfree(freetable);
  luaI_strfree(freestr);
  luaI_funcfree(freefunc);
  return recovered;
} 


void lua_pack (void)
{
  if (++gc_nentity >= gc_block)
    lua_collectgarbage(0);
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
    next = luaI_findsymbolbyname(luaL_check_string(1)) + 1;
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
    return "tag-method";
  else if ((*name = lua_travsymbol(checkfunc)) != NULL)
    return "global";
  else return "";
}

