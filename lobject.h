/*
** $Id: lobject.h,v 1.99 2001/02/23 20:32:32 roberto Exp roberto $
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


/* ttype for closures active in the stack */
#define LUA_TMARK	6


/* tags for values visible from Lua == first user-created tag */
#define NUM_TAGS	6


/* check whether `t' is a mark */
#define is_T_MARK(t)	(ttype(t) == LUA_TMARK)



typedef union {
  struct TString *ts;
  struct Closure *cl;
  struct Hash *h;
  struct CallInfo *info;
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
#define clvalue(o)      ((o)->value.cl)
#define hvalue(o)       ((o)->value.h)
#define infovalue(o)	((o)->value.info)


/* Macros to set values */
#define setnvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TNUMBER; _o->value.n=(x); }

#define setsvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TSTRING; _o->value.ts=(x); }

#define setuvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TUSERDATA; _o->value.ts=(x); }

#define setclvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TFUNCTION; _o->value.cl=(x); }

#define sethvalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TTABLE; _o->value.h=(x); }

#define setivalue(obj,x) \
  { TObject *_o=(obj); _o->tt=LUA_TMARK; _o->value.info=(x); }

#define setnilvalue(obj) ((obj)->tt=LUA_TNIL)

#define setobj(obj1,obj2) \
  { TObject *o1=(obj1); const TObject *o2=(obj2); \
    o1->tt=o2->tt; o1->value = o2->value; }



/*
** String headers for string table
*/

typedef struct TString {
  union {
    struct {  /* for strings */
      lu_hash hash;
      int constindex;  /* hint to reuse constants */
    } s;
    struct {  /* for userdata */
      int tag;
      void *value;
    } d;
  } u;
  size_t len;
  int marked;
  struct TString *nexthash;  /* chain for hash table */
} TString;


/*
** type equivalent to TString, but with maximum alignment requirements
*/
union L_UTString {
  TString ts;
  union L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
};



#define getstr(ts)	((l_char *)((union L_UTString *)(ts) + 1))
#define svalue(o)       getstr(tsvalue(o))


/*
** Function Prototypes
*/
typedef struct Proto {
  lua_Number *knum;  /* numbers used by the function */
  int sizeknum;  /* size of `knum' */
  struct TString **kstr;  /* strings used by the function */
  int sizekstr;  /* size of `kstr' */
  struct Proto **kproto;  /* functions defined inside the function */
  int sizekproto;  /* size of `kproto' */
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
** Closures
*/
typedef struct Closure {
  int isC;  /* 0 for Lua functions, 1 for C functions */
  int nupvalues;
  union {
    lua_CFunction c;  /* C functions */
    struct Proto *l;  /* Lua functions */
  } f;
  struct Closure *next;
  struct Closure *mark;  /* marked closures (point to itself when not marked) */
  TObject upvalue[1];
} Closure;


#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->isC)


typedef struct Node {
  struct Node *next;  /* for chaining */
  int key_tt;  /* (break object to save padding space) */
  Value key_value;
  TObject val;
} Node;


typedef struct Hash {
  Node *node;
  int htag;
  int size;
  Node *firstfree;  /* this position is free; all positions after it are full */
  struct Hash *next;
  struct Hash *mark;  /* marked tables (point to itself when not marked) */
} Hash;


/* unmarked tables and closures are represented by pointing `mark' to
** themselves
*/
#define ismarked(x)	((x)->mark != (x))


/*
** `module' operation for hashing (size is always a power of 2)
*/
#define lmod(s,size)	((int)((s) & ((size)-1)))


/*
** informations about a call (for debugging)
*/
typedef struct CallInfo {
  struct Closure *func;  /* function being called */
  const Instruction **pc;  /* current pc of called function */
  int lastpc;  /* last pc traced */
  int line;  /* current line */
  int refi;  /* current index in `lineinfo' */
} CallInfo;


extern const TObject luaO_nilobject;


#define luaO_openspace(L,n,t)	((t *)luaO_openspaceaux(L,(n)*sizeof(t)))
void *luaO_openspaceaux (lua_State *L, size_t n);

int luaO_equalObj (const TObject *t1, const TObject *t2);
int luaO_str2d (const l_char *s, lua_Number *result);

void luaO_verror (lua_State *L, const l_char *fmt, ...);
void luaO_chunkid (l_char *out, const l_char *source, int len);


#endif
