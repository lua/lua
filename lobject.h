/*
** $Id: $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/

#ifndef lobject_h
#define lobject_h


#include "lua.h"


#include <limits.h>

#ifndef real
#define real float
#endif

#define Byte lua_Byte	/* some systems have Byte as a predefined type */
typedef unsigned char  Byte;  /* unsigned 8 bits */

#define Word lua_Word	/* some systems have Word as a predefined type */
typedef unsigned short Word;  /* unsigned 16 bits */

#define MAX_WORD  (USHRT_MAX-2)  /* maximum value of a word (-2 for safety) */
#define MAX_INT   (INT_MAX-2)  /* maximum value of a int (-2 for safety) */

typedef unsigned int IntPoint; /* unsigned with same size as a pointer (for hashing) */




/*
** String headers for string table
*/

#define NOT_USED  0xFFFE

typedef struct TaggedString {
  int tag;  /* if != LUA_T_STRING, this is a userdata */
  union {
    unsigned long hash;
    struct TaggedString *next;
  } uu;
  union {
    struct {
      Word varindex;  /* != NOT_USED  if this is a symbol */
      Word constindex;  /* hint to reuse constant indexes */
    } s;
    void *v;  /* if this is a userdata, here is its value */
  } u;
  int marked;   /* for garbage collection; never collect (nor change) if > 1 */
  char str[1];   /* \0 byte already reserved */
} TaggedString;



/*
** generic header for garbage collector lists
*/
typedef struct GCnode {
  struct GCnode *next;
  int marked;
} GCnode;


/*
** Function Prototypes
*/
typedef struct TProtoFunc {
  GCnode head;
  Byte *code;  /* ends with opcode ENDCODE */
  int lineDefined;
  TaggedString  *fileName;
  struct TObject *consts;
  int nconsts;
  struct LocVar *locvars;  /* ends with line = -1 */
  int nupvalues;
} TProtoFunc;

typedef struct LocVar {
  TaggedString *varname;           /* NULL signals end of scope */
  int line;
} LocVar;




/*
** Lua TYPES
** WARNING: if you change the order of this enumeration,
** grep "ORDER LUA_T"
*/
typedef enum {
  LUA_T_NIL      = -10,
  LUA_T_NUMBER   = -9,
  LUA_T_STRING   = -8,
  LUA_T_ARRAY    = -7,  /* array==table */
  LUA_T_PROTO    = -6,
  LUA_T_FUNCTION = -5,
  LUA_T_CFUNCTION= -4,
  LUA_T_MARK     = -3,
  LUA_T_CMARK    = -2,
  LUA_T_LINE     = -1,
  LUA_T_USERDATA = 0
} lua_Type;

#define NUM_TYPES 11


typedef union {
  lua_CFunction f;
  real n;
  TaggedString *ts;
  TProtoFunc *tf;
  struct Closure *cl;
  struct Hash *a;
  int i;
} Value;

typedef struct TObject {
  lua_Type ttype;
  Value value;
} TObject;


/* Macros to access structure members */
#define ttype(o)        ((o)->ttype)
#define nvalue(o)       ((o)->value.n)
#define svalue(o)       ((o)->value.ts->str)
#define tsvalue(o)      ((o)->value.ts)
#define clvalue(o)      ((o)->value.cl)
#define avalue(o)       ((o)->value.a)
#define fvalue(o)       ((o)->value.f)
#define tfvalue(o)	((o)->value.tf)


/*
** Closures
*/
typedef struct Closure {
  GCnode head;
  TObject consts[1];  /* at least one for prototype */
} Closure;



typedef struct node {
  TObject ref;
  TObject val;
} Node;

typedef struct Hash {
  GCnode head;
  Node *node;
  int nhash;
  int nuse;
  int htag;
} Hash;


extern long luaO_nentities;
extern char *luaO_typenames[];

int luaO_equalObj (TObject *t1, TObject *t2);
int luaO_redimension (int oldsize);
int luaO_findstring (char *name, char *list[]);


#endif
