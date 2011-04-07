/*
** $Id: lobject.h,v 2.48 2011/04/05 14:24:07 roberto Exp roberto $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/


#ifndef lobject_h
#define lobject_h


#include <stdarg.h>


#include "llimits.h"
#include "lua.h"


/*
** Extra tags for non-values
*/
#define LUA_TPROTO	LUA_NUMTAGS
#define LUA_TUPVAL	(LUA_NUMTAGS+1)
#define LUA_TDEADKEY	(LUA_NUMTAGS+2)

/*
** number of all possible tags (including LUA_TNONE but excluding DEADKEY)
*/
#define LUA_TOTALTAGS	(LUA_TUPVAL+2)


/*
** tags for Tagged Values have the following use of bits:
** bits 0-3: actual tag (a LUA_T* value)
** bit 4: variant bit (for functions, means a light C function)
** bit 5: whether value is collectable
*/

/* Variant tag for light C functions */
#define BIT_ISVARIANT	(1 << 4)
#define LUA_TLCF	(LUA_TFUNCTION | BIT_ISVARIANT)


/* Bit mark for collectable types */
#define BIT_ISCOLLECTABLE	(1 << 5)

/* mark a tag as collectable */
#define ctb(t)			((t) | BIT_ISCOLLECTABLE)


/*
** Union of all collectable objects
*/
typedef union GCObject GCObject;


/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/
#define CommonHeader	GCObject *next; lu_byte tt; lu_byte marked


/*
** Common header in struct form
*/
typedef struct GCheader {
  CommonHeader;
} GCheader;



/*
** Union of all Lua values
*/
typedef union {
  GCObject *gc;    /* collectable objects */
  void *p;         /* light userdata */
  lua_Number n;    /* numbers */
  int b;           /* booleans */
  lua_CFunction f; /* light C functions */
} Value;



/*
** Tagged Values. This is the basic representation of values in Lua,
** an actual value plus a tag with its type.
*/

#define TValuefields	Value value_; int tt_

typedef struct lua_TValue {
  TValuefields;
} TValue;


/* macro defining a nil value */
#define NILCONSTANT    {NULL}, LUA_TNIL


/* raw type tag of a TValue */
#define rttype(o)	((o)->tt_)

/* type tag of a TValue (bits 0-3 for tags + variant bit) */
#define ttype(o)	(rttype(o) & 0x1F)


/* type tag of a TValue with no variants (bits 0-3) */
#define ttypenv(o)	(rttype(o) & 0x0F)


/* Macros to test type */
#define ttisnil(o)		(rttype(o) == LUA_TNIL)
#define ttisboolean(o)		(rttype(o) == LUA_TBOOLEAN)
#define ttislightuserdata(o)	(rttype(o) == LUA_TLIGHTUSERDATA)
#define ttisnumber(o)		(rttype(o) == LUA_TNUMBER)
#define ttisstring(o)		(rttype(o) == ctb(LUA_TSTRING))
#define ttistable(o)		(rttype(o) == ctb(LUA_TTABLE))
#define ttisfunction(o)		(ttypenv(o) == LUA_TFUNCTION)
#define ttisclosure(o)		(rttype(o) == ctb(LUA_TFUNCTION))
#define ttislcf(o)		(rttype(o) == LUA_TLCF)
#define ttisuserdata(o)		(rttype(o) == ctb(LUA_TUSERDATA))
#define ttisthread(o)		(rttype(o) == ctb(LUA_TTHREAD))
#define ttisdeadkey(o)		(rttype(o) == ctb(LUA_TDEADKEY))

#define ttisequal(o1,o2)	(rttype(o1) == rttype(o2))

/* Macros to access values */
#define gcvalue(o)	check_exp(iscollectable(o), (o)->value_.gc)
#define pvalue(o)	check_exp(ttislightuserdata(o), (o)->value_.p)
#define nvalue(o)	check_exp(ttisnumber(o), (o)->value_.n)
#define rawtsvalue(o)	check_exp(ttisstring(o), &(o)->value_.gc->ts)
#define tsvalue(o)	(&rawtsvalue(o)->tsv)
#define rawuvalue(o)	check_exp(ttisuserdata(o), &(o)->value_.gc->u)
#define uvalue(o)	(&rawuvalue(o)->uv)
#define clvalue(o)	check_exp(ttisclosure(o), &(o)->value_.gc->cl)
#define fvalue(o)	check_exp(ttislcf(o), (o)->value_.f)
#define hvalue(o)	check_exp(ttistable(o), &(o)->value_.gc->h)
#define bvalue(o)	check_exp(ttisboolean(o), (o)->value_.b)
#define thvalue(o)	check_exp(ttisthread(o), &(o)->value_.gc->th)

#define l_isfalse(o)	(ttisnil(o) || (ttisboolean(o) && bvalue(o) == 0))


#define iscollectable(o)	(rttype(o) & BIT_ISCOLLECTABLE)


/* Macros for internal tests */
#define righttt(obj)		(ttype(obj) == gcvalue(obj)->gch.tt)

#define checkconsistency(obj)	lua_assert(!iscollectable(obj) || righttt(obj))

#define checkliveness(g,obj) \
  lua_assert(!iscollectable(obj) || (righttt(obj) && !isdead(g,gcvalue(obj))))


/* Macros to set values */
#define setnilvalue(obj) ((obj)->tt_=LUA_TNIL)

#define setnvalue(obj,x) \
  { TValue *io_=(obj); io_->value_.n=(x); io_->tt_=LUA_TNUMBER; }

#define setfvalue(obj,x) \
  { TValue *io_=(obj); io_->value_.f=(x); io_->tt_=LUA_TLCF; }

#define changenvalue(o,x)  check_exp((o)->tt_==LUA_TNUMBER, (o)->value_.n=(x))

#define setpvalue(obj,x) \
  { TValue *io_=(obj); io_->value_.p=(x); io_->tt_=LUA_TLIGHTUSERDATA; }

#define setbvalue(obj,x) \
  { TValue *io_=(obj); io_->value_.b=(x); io_->tt_=LUA_TBOOLEAN; }

#define setgcovalue(L,obj,x) \
  { TValue *io_=(obj); GCObject *i_g=(x); \
    io_->value_.gc=i_g; io_->tt_=ctb(gch(i_g)->tt); }

#define setsvalue(L,obj,x) \
  { TValue *io_=(obj); \
    io_->value_.gc=cast(GCObject *, (x)); io_->tt_=ctb(LUA_TSTRING); \
    checkliveness(G(L),io_); }

#define setuvalue(L,obj,x) \
  { TValue *io_=(obj); \
    io_->value_.gc=cast(GCObject *, (x)); io_->tt_=ctb(LUA_TUSERDATA); \
    checkliveness(G(L),io_); }

#define setthvalue(L,obj,x) \
  { TValue *io_=(obj); \
    io_->value_.gc=cast(GCObject *, (x)); io_->tt_=ctb(LUA_TTHREAD); \
    checkliveness(G(L),io_); }

#define setclvalue(L,obj,x) \
  { TValue *io_=(obj); \
    io_->value_.gc=cast(GCObject *, (x)); io_->tt_=ctb(LUA_TFUNCTION); \
    checkliveness(G(L),io_); }

#define sethvalue(L,obj,x) \
  { TValue *io_=(obj); \
    io_->value_.gc=cast(GCObject *, (x)); io_->tt_=ctb(LUA_TTABLE); \
    checkliveness(G(L),io_); }

#define setptvalue(L,obj,x) \
  { TValue *io_=(obj); \
    io_->value_.gc=cast(GCObject *, (x)); io_->tt_=ctb(LUA_TPROTO); \
    checkliveness(G(L),io_); }

#define setdeadvalue(obj)	((obj)->tt_=ctb(LUA_TDEADKEY))



#define setobj(L,obj1,obj2) \
	{ const TValue *o2_=(obj2); TValue *o1_=(obj1); \
	  o1_->value_ = o2_->value_; o1_->tt_=o2_->tt_; \
	  checkliveness(G(L),o1_); }


/*
** different types of assignments, according to destination
*/

/* from stack to (same) stack */
#define setobjs2s	setobj
/* to stack (not from same stack) */
#define setobj2s	setobj
#define setsvalue2s	setsvalue
#define sethvalue2s	sethvalue
#define setptvalue2s	setptvalue
/* from table to same table */
#define setobjt2t	setobj
/* to table */
#define setobj2t	setobj
/* to new object */
#define setobj2n	setobj
#define setsvalue2n	setsvalue



typedef TValue *StkId;  /* index to stack elements */


/*
** Header for string value; string bytes follow the end of this structure
*/
typedef union TString {
  L_Umaxalign dummy;  /* ensures maximum alignment for strings */
  struct {
    CommonHeader;
    lu_byte reserved;
    unsigned int hash;
    size_t len;
  } tsv;
} TString;


/* get the actual string (array of bytes) from a TString */
#define getstr(ts)	cast(const char *, (ts) + 1)

/* get the actual string (array of bytes) from a Lua value */
#define svalue(o)       getstr(rawtsvalue(o))


/*
** Header for userdata; memory area follows the end of this structure
*/
typedef union Udata {
  L_Umaxalign dummy;  /* ensures maximum alignment for `local' udata */
  struct {
    CommonHeader;
    struct Table *metatable;
    struct Table *env;
    size_t len;
  } uv;
} Udata;



/*
** Description of an upvalue for function prototypes
*/
typedef struct Upvaldesc {
  TString *name;  /* upvalue name (for debug information) */
  lu_byte instack;  /* whether it is in stack */
  lu_byte idx;  /* index of upvalue (in stack or in outer function's list) */
} Upvaldesc;


/*
** Description of a local variable for function prototypes
** (used for debug information)
*/
typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;


/*
** Function Prototypes
*/
typedef struct Proto {
  CommonHeader;
  TValue *k;  /* constants used by the function */
  Instruction *code;
  struct Proto **p;  /* functions defined inside the function */
  int *lineinfo;  /* map from opcodes to source lines */
  LocVar *locvars;  /* information about local variables */
  Upvaldesc *upvalues;  /* upvalue information */
  union Closure *cache;  /* last created closure with this prototype */
  TString  *source;
  int sizeupvalues;  /* size of 'upvalues' */
  int sizek;  /* size of `k' */
  int sizecode;
  int sizelineinfo;
  int sizep;  /* size of `p' */
  int sizelocvars;
  int linedefined;
  int lastlinedefined;
  GCObject *gclist;
  lu_byte numparams;  /* number of fixed parameters */
  lu_byte is_vararg;
  lu_byte maxstacksize;  /* maximum stack used by this function */
} Proto;



/*
** Lua Upvalues
*/
typedef struct UpVal {
  CommonHeader;
  TValue *v;  /* points to stack or to its own value */
  union {
    TValue value;  /* the value (when closed) */
    struct {  /* double linked list (when open) */
      struct UpVal *prev;
      struct UpVal *next;
    } l;
  } u;
} UpVal;


/*
** Closures
*/

#define ClosureHeader \
	CommonHeader; lu_byte isC; lu_byte nupvalues; GCObject *gclist

typedef struct CClosure {
  ClosureHeader;
  lua_CFunction f;
  TValue upvalue[1];  /* list of upvalues */
} CClosure;


typedef struct LClosure {
  ClosureHeader;
  struct Proto *p;
  UpVal *upvals[1];  /* list of upvalues */
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;


#define isLfunction(o)	(ttisclosure(o) && !clvalue(o)->c.isC)

#define getproto(o)	(clvalue(o)->l.p)


/*
** Tables
*/

typedef union TKey {
  struct {
    TValuefields;
    struct Node *next;  /* for chaining */
  } nk;
  TValue tvk;
} TKey;


typedef struct Node {
  TValue i_val;
  TKey i_key;
} Node;


typedef struct Table {
  CommonHeader;
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */
  lu_byte lsizenode;  /* log2 of size of `node' array */
  struct Table *metatable;
  TValue *array;  /* array part */
  Node *node;
  Node *lastfree;  /* any free position is before this position */
  GCObject *gclist;
  int sizearray;  /* size of `array' array */
} Table;



/*
** `module' operation for hashing (size is always a power of 2)
*/
#define lmod(s,size) \
	(check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))


#define twoto(x)	(1<<(x))
#define sizenode(t)	(twoto((t)->lsizenode))


/*
** (address of) a fixed nil value
*/
#define luaO_nilobject		(&luaO_nilobject_)


LUAI_DDEC const TValue luaO_nilobject_;

LUAI_FUNC int luaO_int2fb (unsigned int x);
LUAI_FUNC int luaO_fb2int (int x);
LUAI_FUNC int luaO_ceillog2 (unsigned int x);
LUAI_FUNC lua_Number luaO_arith (int op, lua_Number v1, lua_Number v2);
LUAI_FUNC int luaO_rawequalObj (const TValue *t1, const TValue *t2);
LUAI_FUNC int luaO_str2d (const char *s, size_t len, lua_Number *result);
LUAI_FUNC int luaO_hexavalue (int c);
LUAI_FUNC const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
LUAI_FUNC const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
LUAI_FUNC void luaO_chunkid (char *out, const char *source, size_t len);


#endif

