/*
** $Id: lobject.h,v 1.85 2000/12/28 12:55:41 roberto Exp roberto $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/

#ifndef lobject_h
#define lobject_h


#include "llimits.h"
#include "lua.h"


#ifdef LUA_DEBUG
#undef NDEBUG
#include <assert.h>
#define LUA_INTERNALERROR(s)	assert(((void)s,0))
#define LUA_ASSERT(c,s)		assert(((void)s,(c)))
#else
#define LUA_INTERNALERROR(s)	/* empty */
#define LUA_ASSERT(c,s)		/* empty */
#endif


#ifdef LUA_DEBUG
/* to avoid warnings, and make sure value is really unused */
#define UNUSED(x)	(x=0, (void)(x))
#else
#define UNUSED(x)	((void)(x))	/* to avoid warnings */
#endif


/* mark for closures active in the stack */
#define LUA_TMARK	6


/* tags for values visible from Lua == first user-created tag */
#define NUM_TAGS	6


/* check whether `t' is a mark */
#define is_T_MARK(t)	(ttype(t) == LUA_TMARK)


typedef union {
  void *v;
  lua_Number n;		/* LUA_TNUMBER */
} Value;


typedef struct lua_TObject {
  int tt;
  Value value;
} TObject;


/* Macros to access values */
#define ttype(o)        ((o)->tt)
#define nvalue(o)       ((o)->value.n)
#define tsvalue(o)      ((struct TString *)(o)->value.v)
#define clvalue(o)      ((struct Closure *)(o)->value.v)
#define hvalue(o)       ((struct Hash *)(o)->value.v)
#define infovalue(o)	((struct CallInfo *)(o)->value.v)
#define svalue(o)       (tsvalue(o)->str)


/* Macros to set values */
#define setnvalue(obj,x) \
  { TObject *o=(obj); o->tt=LUA_TNUMBER; o->value.n=(x); }

#define setsvalue(obj,x) \
  { TObject *o=(obj); struct TString *v=(x); \
    o->tt=LUA_TSTRING; o->value.v=v; }

#define setuvalue(obj,x) \
  { TObject *o=(obj); struct TString *v=(x); \
    o->tt=LUA_TUSERDATA; o->value.v=v; }

#define setclvalue(obj,x) \
  { TObject *o=(obj); struct Closure *v=(x); \
    o->tt=LUA_TFUNCTION; o->value.v=v; }

#define sethvalue(obj,x) \
  { TObject *o=(obj); struct Hash *v=(x); \
    o->tt=LUA_TTABLE; o->value.v=v; }

#define setivalue(obj,x) \
  { TObject *o=(obj); struct CallInfo *v=(x); \
    o->tt=LUA_TMARK; o->value.v=v; }

#define setnilvalue(obj) { (obj)->tt=LUA_TNIL; }

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
  struct TString *nexthash;  /* chain for hash table */
  int marked;
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
  union {
    lua_CFunction c;  /* C functions */
    struct Proto *l;  /* Lua functions */
  } f;
  struct Closure *next;
  struct Closure *mark;  /* marked closures (point to itself when not marked) */
  short isC;  /* 0 for Lua functions, 1 for C functions */
  short nupvalues;
  TObject upvalue[1];
} Closure;


#define iscfunction(o)	(ttype(o) == LUA_TFUNCTION && clvalue(o)->isC)


typedef struct Node {
  TObject key;
  TObject val;
  struct Node *next;  /* for chaining */
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
extern const char *const luaO_typenames[];


#define luaO_typename(o)	(luaO_typenames[ttype(o)])


luint32 luaO_power2 (luint32 n);
char *luaO_openspace (lua_State *L, size_t n);

int luaO_equalObj (const TObject *t1, const TObject *t2);
int luaO_str2d (const char *s, lua_Number *result);

void luaO_verror (lua_State *L, const char *fmt, ...);
void luaO_chunkid (char *out, const char *source, int len);


#endif
