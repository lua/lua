/*
** $Id: lobject.h,v 1.60 2000/04/10 19:20:24 roberto Exp roberto $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/

#ifndef lobject_h
#define lobject_h


#include "llimits.h"
#include "lua.h"


#ifdef DEBUG
#undef NDEBUG
#include <assert.h>
#define LUA_INTERNALERROR(L,s)	assert(((void)s,0))
#define LUA_ASSERT(L,c,s)	assert(((void)s,(c)))
#else
#define LUA_INTERNALERROR(L,s)	/* empty */
#define LUA_ASSERT(L,c,s)	/* empty */
#endif


#ifdef DEBUG
/* to avoid warnings, and make sure value is really unused */
#define UNUSED(x)	(x=0, (void)(x))
#else
#define UNUSED(x)	((void)(x))	/* to avoid warnings */
#endif


/*
** Lua TYPES
** WARNING: if you change the order of this enumeration,
** grep "ORDER LUA_T"
*/
typedef enum {
  TAG_USERDATA  =  0, /* default tag for userdata */
  TAG_NUMBER,	/* fixed tag for numbers */
  TAG_STRING,	/* fixed tag for strings */
  TAG_TABLE,	/* default tag for tables */
  TAG_LCLOSURE,	/* fixed tag for Lua closures */
  TAG_CCLOSURE,	/* fixed tag for C closures */
  TAG_NIL,	/* last "pre-defined" tag */

  TAG_LMARK,	/* mark for Lua closures */
  TAG_CMARK,	/* mark for C closures */

  TAG_LINE
} lua_Type;

/* tags for values visible from Lua == first user-created tag */
#define NUM_TAGS	7


/*
** check whether `t' is a mark
*/
#define is_T_MARK(t)	((t) == TAG_LMARK || (t) == TAG_CMARK)


typedef union {
  struct TString *ts;	/* TAG_STRING, TAG_USERDATA */
  struct Closure *cl;	/* TAG_[CL]CLOSURE, TAG_[CL]MARK */
  struct Hash *a;	/* TAG_TABLE */
  Number n;		/* TAG_NUMBER */
  int i;		/* TAG_LINE */
} Value;


/* Macros to access values */
#define ttype(o)        ((o)->ttype)
#define nvalue(o)       ((o)->value.n)
#define svalue(o)       ((o)->value.ts->str)
#define tsvalue(o)      ((o)->value.ts)
#define clvalue(o)      ((o)->value.cl)
#define avalue(o)       ((o)->value.a)


typedef struct TObject {
  lua_Type ttype;
  Value value;
} TObject;


typedef struct GlobalVar {
  TObject value;
  struct GlobalVar *next;
  struct TString *name;
} GlobalVar;


/*
** String headers for string table
*/
typedef struct TString {
  union {
    struct {  /* for strings */
      GlobalVar *gv;  /* eventual global value with this name */
      long len;
    } s;
    struct {  /* for userdata */
      int tag;
      void *value;
    } d;
  } u;
  struct TString *nexthash;  /* chain for hash table */
  unsigned long hash;
  int constindex;  /* hint to reuse constants (= -1 if this is a userdata) */
  unsigned char marked;
  char str[1];   /* variable length string!! must be the last field! */
} TString;


/*
** Function Prototypes
*/
typedef struct Proto {
  struct Proto *next;
  int marked;
  struct TString **kstr;  /* strings used by the function */
  int nkstr;  /* size of `kstr' */
  Number *knum;  /* Number numbers used by the function */
  int nknum;  /* size of `knum' */
  struct Proto **kproto;  /* functions defined inside the function */
  int nkproto;  /* size of `kproto' */
  Instruction *code;  /* ends with opcode ENDCODE */
  int lineDefined;
  TString  *source;
  int numparams;
  int is_vararg;
  int maxstacksize;
  struct LocVar *locvars;  /* ends with line = -1 */
} Proto;


typedef struct LocVar {
  TString *varname;           /* NULL signals end of scope */
  int line;
} LocVar;


/*
** Closures
*/
typedef struct Closure {
  struct Closure *next;
  int marked;
  union {
    lua_CFunction c;  /* C functions */
    struct Proto *l;  /* Lua functions */
  } f;
  int nelems;
  TObject consts[1];
} Closure;


typedef struct Node {
  TObject key;
  TObject val;
  struct Node *next;  /* for chaining */
} Node;

typedef struct Hash {
  int htag;
  Node *node;
  int size;
  Node *firstfree;  /* this position is free; all positions after it are full */
  struct Hash *next;
  int marked;
} Hash;


extern const char *const luaO_typenames[];
extern const TObject luaO_nilobject;

#define luaO_typename(o)        luaO_typenames[ttype(o)]

unsigned long luaO_power2 (unsigned long n);

int luaO_equalObj (const TObject *t1, const TObject *t2);
int luaO_redimension (lua_State *L, int oldsize);
int luaO_str2d (const char *s, Number *result);


#endif
