/*
** $Id: lobject.h,v 1.1 2001/11/29 22:14:34 rieru Exp rieru $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/

#ifndef lobject_h
#define lobject_h


#include "llimits.h"
#include "lua.h"


#ifndef lua_assert
#define lua_assert(c)		/* empty */
#endif


#ifndef UNUSED
#define UNUSED(x)	((void)(x))	/* to avoid warnings */
#endif


#ifndef cast
#define cast(t, exp)	((t)(exp))
#endif


/* tags for values visible from Lua */
#define NUM_TAGS	6


typedef union {
  union TString *ts;
  union Udata *u;
  union Closure *cl;
  struct Table *h;
  struct lua_TObject *v;
  lua_Number n;		/* LUA_TNUMBER */
} Value;


typedef struct lua_TObject {
  int tt;
  Value value;
} TObject;


/* Macros to access values */
#define ttype(o)        ((o)->tt)
#define nvalue(o)       ((o)->value.n)
#define tsvalue(o)      ((o)->value.ts)
#define uvalue(o)      ((o)->value.u)
#define clvalue(o)      ((o)->value.cl)
#define hvalue(o)       ((o)->value.h)
#define vvalue(o)	((o)->value.v)


/* Macros to set values */
#define setnvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TNUMBER; _o->value.n=(x); }

#define chgnvalue(obj,x)	((obj)->value.n=(x))

#define setsvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TSTRING; _o->value.ts=(x); }

#define setuvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TUSERDATA; _o->value.u=(x); }

#define setclvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TFUNCTION; _o->value.cl=(x); }

#define sethvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TTABLE; _o->value.h=(x); }

#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

#define setupvalue(obj,x,t) \
  { TObject *_o=(obj); _o->tt=(t); _o->value.v=(x); }

#define setobj(obj1,obj2) \
  { TObject *o1=(obj1); const TObject *o2=(obj2); \
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


#define getstr(ts)	cast(char *, (ts) + 1)
#define svalue(o)       getstr(tsvalue(o))



typedef union Udata {
  union L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    struct Table *eventtable;
    void *value;
    size_t len;  /* least bit reserved for gc mark */
    union Udata *next;  /* chain for list of all udata */
  } uv;
} Udata;




/*
** Function Prototypes
*/
typedef struct Proto {
  TObject *k;  /* constants used by the function */
  int sizek;  /* size of `k' */
  struct Proto **p;  /* functions defined inside the function */
  int sizep;  /* size of `p' */
  Instruction *code;
  int sizecode;
  short nupvalues;
  short numparams;
  short is_vararg;
  short maxstacksize;
  short marked;
  struct Proto *next;
  /* debug information */
  int *lineinfo;  /* map from opcodes to source lines */
  int sizelineinfo;  /* size of `lineinfo' */
  struct LocVar *locvars;  /* information about local variables */
  int sizelocvars;
  int lineDefined;
  TString  *source;
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
  UpVal *upvals[1];
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->c.isC)



/*
** Tables
*/

typedef struct Node {
  struct Node *next;  /* for chaining */
  TObject _key;
  TObject _val;
} Node;


typedef struct Table {
  struct Table *eventtable;
  TObject *array;  /* array part */
  Node *node;
  int sizearray;  /* size of `array' array */
  lu_byte lsizenode;  /* log2 of size of `node' array */
  lu_byte weakmode;
  unsigned short flags;  /* 1<<p means tagmethod(p) is not present */ 
  Node *firstfree;  /* this position is free; all positions after it are full */
  struct Table *next;
  struct Table *mark;  /* marked tables (point to itself when not marked) */
} Table;


/* unmarked tables are represented by pointing `mark' to themselves */
#define ismarked(x)	((x)->mark != (x))


/*
** `module' operation for hashing (size is always a power of 2)
*/
#define lmod(s,size)	(cast(int, (s) & ((size)-1)))


#define twoto(x)	(1<<(x))
#define sizenode(t)	(twoto((t)->lsizenode))
#define sizearray(t)	((t)->sizearray)

/*
** informations about a call (for debugging)
*/
typedef struct CallInfo {
  struct CallInfo *prev;  /* linked list */
  StkId base;  /* base for called function */
  const Instruction **pc;  /* current pc of called function */
  int lastpc;  /* last pc traced */
  int line;  /* current line */
  int refi;  /* current index in `lineinfo' */
} CallInfo;

#define ci_func(ci)	(clvalue((ci)->base - 1))


extern const TObject luaO_nilobject;

int luaO_log2 (unsigned int x);


#define luaO_openspace(L,n,t)	cast(t *, luaO_openspaceaux(L,(n)*sizeof(t)))
void *luaO_openspaceaux (lua_State *L, size_t n);

int luaO_equalObj (const TObject *t1, const TObject *t2);
int luaO_str2d (const char *s, lua_Number *result);

void luaO_verror (lua_State *L, const char *fmt, ...);
void luaO_chunkid (char *out, const char *source, int len);


#endif
