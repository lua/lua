/*
** $Id: lobject.h,v 1.52 2000/03/16 21:06:16 roberto Exp roberto $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/

#ifndef lobject_h
#define lobject_h


#include <limits.h>

#include "lua.h"


#ifdef DEBUG
#undef NDEBUG
#include <assert.h>
#define LUA_INTERNALERROR(L,s)	assert(0)
#define LUA_ASSERT(L,c,s)	assert(c)
#else
#define LUA_INTERNALERROR(L,s)	/* empty */
#define LUA_ASSERT(L,c,s)	/* empty */
#endif


#define UNUSED(x)	(void)x		/* to avoid warnings */

/*
** Define the type `number' of Lua
** GREP LUA_NUMBER to change that
*/
#ifndef LUA_NUM_TYPE
#define LUA_NUM_TYPE double
#endif

typedef LUA_NUM_TYPE Number;


/*
** type for virtual-machine instructions
** must be an unsigned with 4 bytes (see details in lopcodes.h)
*/
typedef unsigned long Instruction;


#define MAX_INT (INT_MAX-2)  /* maximum value of an int (-2 for safety) */


/* conversion of pointer to int (for hashing only) */
/* (the shift removes bits that are usually 0 because of alignment) */
#define IntPoint(L, p)	(((unsigned long)(p)) >> 3)


/*
** number of `blocks' for garbage collection: each reference to other
** objects count 1, and each 32 bytes of `raw' memory count 1; we add
** 2 to the total as a minimum (and also to count the overhead of malloc)
*/
#define numblocks(L, o,b)	((o)+((b)>>5)+2)


/*
** Lua TYPES
** WARNING: if you change the order of this enumeration,
** grep "ORDER LUA_T"
*/
typedef enum {
  TAG_USERDATA  =  0, /* default tag for userdata */
  TAG_NUMBER,	/* fixed tag for numbers */
  TAG_STRING,	/* fixed tag for strings */
  TAG_ARRAY,	/* default tag for tables (or arrays) */
  TAG_LPROTO,	/* fixed tag for Lua functions */
  TAG_CPROTO,	/* fixed tag for C functions */
  TAG_NIL,	/* last "pre-defined" tag */

  TAG_LCLOSURE,	/* Lua closure */
  TAG_CCLOSURE,	/* C closure */

  TAG_LCLMARK,	/* mark for Lua closures */
  TAG_CCLMARK,	/* mark for C closures */
  TAG_LMARK,	/* mark for Lua prototypes */
  TAG_CMARK,	/* mark for C prototypes */

  TAG_LINE
} lua_Type;

/* tags for values visible from Lua == first user-created tag */
#define NUM_TAGS	7


/*
** check whether `t' is a mark
*/
#define is_T_MARK(t)	(TAG_LCLMARK <= (t) && (t) <= TAG_CMARK)


typedef union {
  lua_CFunction f;              /* TAG_CPROTO, TAG_CMARK */
  Number n;                       /* TAG_NUMBER */
  struct TString *ts;      /* TAG_STRING, TAG_USERDATA */
  struct Proto *tf;        /* TAG_LPROTO, TAG_LMARK */
  struct Closure *cl;           /* TAG_[CL]CLOSURE, TAG_[CL]CLMARK */
  struct Hash *a;               /* TAG_ARRAY */
  int i;                        /* TAG_LINE */
} Value;


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
  char str[1];   /* \0 byte already reserved */
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





/* Macros to access structure members */
#define ttype(o)        ((o)->ttype)
#define nvalue(o)       ((o)->value.n)
#define svalue(o)       ((o)->value.ts->str)
#define tsvalue(o)      ((o)->value.ts)
#define clvalue(o)      ((o)->value.cl)
#define avalue(o)       ((o)->value.a)
#define fvalue(o)       ((o)->value.f)
#define tfvalue(o)	((o)->value.tf)

#define protovalue(o)	((o)->value.cl->consts)


/*
** Closures
*/
typedef struct Closure {
  struct Closure *next;
  int marked;
  int nelems;  /* not including the first one (always the prototype) */
  TObject consts[1];  /* at least one for prototype */
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

#define MINPOWER2	4	/* minimum size for "growing" vectors */

unsigned long luaO_power2 (unsigned long n);

#define luaO_equalObj(t1,t2)  (ttype(t1) == ttype(t2) && luaO_equalval(t1,t2))
int luaO_equalval (const TObject *t1, const TObject *t2);
int luaO_redimension (lua_State *L, int oldsize);
int luaO_str2d (const char *s, Number *result);


#endif
