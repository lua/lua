/*
** $Id: lobject.h,v 1.139 2002/07/01 17:06:58 roberto Exp roberto $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/

#ifndef lobject_h
#define lobject_h


#include "llimits.h"
#include "lua.h"


/* tags for values visible from Lua */
#define NUM_TAGS	LUA_TFUNCTION


typedef union {
  void *p;
  union TString *ts;
  union Udata *u;
  union Closure *cl;
  struct Table *h;
  lua_Number n;
  int b;
} Value;


typedef struct lua_TObject {
  int tt;
  Value value;
} TObject;


/* Macros to access values */
#define ttype(o)	((o)->tt)
#define pvalue(o)	check_exp(ttype(o)==LUA_TLIGHTUSERDATA, (o)->value.p)
#define nvalue(o)	check_exp(ttype(o)==LUA_TNUMBER, (o)->value.n)
#define tsvalue(o)	check_exp(ttype(o)==LUA_TSTRING, (o)->value.ts)
#define uvalue(o)	check_exp(ttype(o)==LUA_TUSERDATA, (o)->value.u)
#define clvalue(o)	check_exp(ttype(o)==LUA_TFUNCTION, (o)->value.cl)
#define hvalue(o)	check_exp(ttype(o)==LUA_TTABLE, (o)->value.h)
#define bvalue(o)	check_exp(ttype(o)==LUA_TBOOLEAN, (o)->value.b)

#define l_isfalse(o)	(ttype(o) == LUA_TNIL || \
			(ttype(o) == LUA_TBOOLEAN && bvalue(o) == 0))

/* Macros to set values */
#define setnvalue(obj,x) \
  { TObject *i_o=(obj); i_o->tt=LUA_TNUMBER; i_o->value.n=(x); }

#define chgnvalue(obj,x) \
	check_exp(ttype(obj)==LUA_TNUMBER, (obj)->value.n=(x))

#define setpvalue(obj,x) \
  { TObject *i_o=(obj); i_o->tt=LUA_TLIGHTUSERDATA; i_o->value.p=(x); }

#define setbvalue(obj,x) \
  { TObject *i_o=(obj); i_o->tt=LUA_TBOOLEAN; i_o->value.b=(x); }

#define setsvalue(obj,x) \
  { TObject *i_o=(obj); i_o->tt=LUA_TSTRING; i_o->value.ts=(x); }

#define setuvalue(obj,x) \
  { TObject *i_o=(obj); i_o->tt=LUA_TUSERDATA; i_o->value.u=(x); }

#define setclvalue(obj,x) \
  { TObject *i_o=(obj); i_o->tt=LUA_TFUNCTION; i_o->value.cl=(x); }

#define sethvalue(obj,x) \
  { TObject *i_o=(obj); i_o->tt=LUA_TTABLE; i_o->value.h=(x); }

#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

#define setobj(obj1,obj2) \
  { const TObject *o2=(obj2); TObject *o1=(obj1); \
    o1->tt=o2->tt; o1->value = o2->value; }

#define setttype(obj, tt) (ttype(obj) = (tt))



typedef TObject *StkId;  /* index to stack elements */


/*
** String headers for string table
*/
typedef union TString {
  union L_Umaxalign dummy;  /* ensures maximum alignment for strings */
  struct {
    lu_hash hash;
    size_t len;
    int marked;
    union TString *nexthash;  /* chain for hash table */
  } tsv;
} TString;


#define getstr(ts)	cast(const char *, (ts) + 1)
#define svalue(o)       getstr(tsvalue(o))



typedef union Udata {
  union L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    struct Table *metatable;
    union Udata *next;  /* chain for list of all udata */
    size_t len;  /* least 2 bits reserved for gc mark */
  } uv;
} Udata;




/*
** Function Prototypes
*/
typedef struct Proto {
  TObject *k;  /* constants used by the function */
  Instruction *code;
  struct Proto **p;  /* functions defined inside the function */
  struct Proto *next;
  int *lineinfo;  /* map from opcodes to source lines */
  struct LocVar *locvars;  /* information about local variables */
  TString  *source;
  int sizek;  /* size of `k' */
  int sizecode;
  int sizep;  /* size of `p' */
  int sizelocvars;
  int lineDefined;
  lu_byte nupvalues;
  lu_byte numparams;
  lu_byte is_vararg;
  lu_byte maxstacksize;
  lu_byte marked;
} Proto;


typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;



/*
** Upvalues
*/

typedef struct UpVal {
  TObject *v;  /* points to stack or to its own value */
  struct UpVal *next;
  TObject value;  /* the value (when closed) */
} UpVal;


/*
** Closures
*/

typedef struct CClosure {
  lu_byte isC;  /* 0 for Lua functions, 1 for C functions */
  lu_byte nupvalues;
  lu_byte marked;
  union Closure *next;
  lua_CFunction f;
  TObject upvalue[1];
} CClosure;


typedef struct LClosure {
  lu_byte isC;
  lu_byte nupvalues;
  lu_byte marked;
  union Closure *next;  /* first four fields must be equal to CClosure!! */
  struct Proto *p;
  TObject g;  /* global table for this closure */
  UpVal *upvals[1];
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)
#define isLfunction(o)	(ttype(o) == LUA_TFUNCTION && !clvalue(o)->c.isC)


/*
** Tables
*/

typedef struct Node {
  TObject i_key;
  TObject i_val;
  struct Node *next;  /* for chaining */
} Node;


typedef struct Table {
  struct Table *metatable;
  TObject *array;  /* array part */
  Node *node;
  Node *firstfree;  /* this position is free; all positions after it are full */
  struct Table *next;
  struct Table *mark;  /* marked tables (point to itself when not marked) */
  int sizearray;  /* size of `array' array */
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */ 
  lu_byte lsizenode;  /* log2 of size of `node' array */
} Table;



/*
** `module' operation for hashing (size is always a power of 2)
*/
#define lmod(s,size) \
	check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1))))


#define twoto(x)	(1<<(x))
#define sizenode(t)	(twoto((t)->lsizenode))
#define sizearray(t)	((t)->sizearray)



extern const TObject luaO_nilobject;

int luaO_log2 (unsigned int x);


#define luaO_openspace(L,n,t)	cast(t *, luaO_openspaceaux(L,(n)*sizeof(t)))
void *luaO_openspaceaux (lua_State *L, size_t n);

int luaO_rawequalObj (const TObject *t1, const TObject *t2);
int luaO_str2d (const char *s, lua_Number *result);

const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp);
const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
void luaO_chunkid (char *out, const char *source, int len);


#endif
