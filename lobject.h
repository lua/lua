/*
** $Id: lobject.h,v 1.93 2001/02/02 15:13:05 roberto Exp roberto $
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
#define svalue(o)       (tsvalue(o)->str)


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

/*
** most `malloc' libraries allocate memory in blocks of 8 bytes. TSPACK
** tries to make sizeof(TString) a multiple of this granularity, to reduce
** waste of space.
*/
#define TSPACK	((int)sizeof(int))

typedef struct TString {
  union {
    struct {  /* for strings */
      luint32 hash;
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
  char str[TSPACK];   /* variable length string!! must be the last field! */
} TString;


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
  union {
    lua_CFunction c;  /* C functions */
    struct Proto *l;  /* Lua functions */
  } f;
  struct Closure *next;
  struct Closure *mark;  /* marked closures (point to itself when not marked) */
  int nupvalues;
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
** "module" operation (size is always a power of 2) */
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


luint32 luaO_power2 (luint32 n);
char *luaO_openspace (lua_State *L, size_t n);

int luaO_equalObj (const TObject *t1, const TObject *t2);
int luaO_str2d (const char *s, lua_Number *result);

void luaO_verror (lua_State *L, const char *fmt, ...);
void luaO_chunkid (char *out, const char *source, int len);


#endif
