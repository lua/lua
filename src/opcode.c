/*
** opcode.c
** TecCGraf - PUC-Rio
*/

char *rcs_opcode="$Id: opcode.c,v 3.34 1995/02/06 19:35:09 roberto Exp $";

#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mem.h"
#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"
#include "fallback.h"

#define tonumber(o) ((tag(o) != LUA_T_NUMBER) && (lua_tonumber(o) != 0))
#define tostring(o) ((tag(o) != LUA_T_STRING) && (lua_tostring(o) != 0))


#define STACK_BUFFER (STACKGAP+128)

typedef int StkId;  /* index to stack elements */

static Long    maxstack = 0L;
static Object *stack = NULL;
static Object *top = NULL;


/* macros to convert from lua_Object to (Object *) and back */
 
#define Address(lo)     ((lo)+stack-1)
#define Ref(st)         ((st)-stack+1)
 

static StkId CBase = 0;  /* when Lua calls C or C calls Lua, points to */
                          /* the first slot after the last parameter. */
static int CnResults = 0; /* when Lua calls C, has the number of parameters; */
                         /* when C calls Lua, has the number of results. */

static  jmp_buf *errorJmp = NULL; /* current error recover point */


static StkId lua_execute (Byte *pc, StkId base);
static void do_call (Object *func, StkId base, int nResults, StkId whereRes);



Object *luaI_Address (lua_Object o)
{
  return Address(o);
}


/*
** Error messages
*/

static void lua_message (char *s)
{
  lua_pushstring(s);
  do_call(&luaI_fallBacks[FB_ERROR].function, (top-stack)-1, 0, (top-stack)-1);
}

/*
** Reports an error, and jumps up to the available recover label
*/
void lua_error (char *s)
{
  if (s) lua_message(s);
  if (errorJmp)
    longjmp(*errorJmp, 1);
  else
  {
    fprintf (stderr, "lua: exit(1). Unable to recover\n");
    exit(1);
  }
}


/*
** Init stack
*/
static void lua_initstack (void)
{
 maxstack = STACK_BUFFER;
 stack = newvector(maxstack, Object);
 top = stack;
}


/*
** Check stack overflow and, if necessary, realloc vector
*/
#define lua_checkstack(n)  if ((Long)(n) > maxstack) checkstack(n)

static void checkstack (StkId n)
{
 StkId t;
 if (stack == NULL)
   lua_initstack();
 if (maxstack >= MAX_INT)
   lua_error("stack size overflow");
 t = top-stack;
 maxstack *= 2;
 if (maxstack >= MAX_INT)
   maxstack = MAX_INT;
 stack = growvector(stack, maxstack, Object);
 top = stack + t;
}


/*
** Concatenate two given strings. Return the new string pointer.
*/
static char *lua_strconc (char *l, char *r)
{
 static char *buffer = NULL;
 static int buffer_size = 0;
 int nl = strlen(l);
 int n = nl+strlen(r)+1;
 if (n > buffer_size)
  {
   buffer_size = n;
   if (buffer != NULL)
     luaI_free(buffer);
   buffer = newvector(buffer_size, char);
  }
  strcpy(buffer,l);
  strcpy(buffer+nl, r);
  return buffer;
}


/*
** Convert, if possible, to a number object.
** Return 0 if success, not 0 if error.
*/
static int lua_tonumber (Object *obj)
{
 float t;
 char c;
 if (tag(obj) != LUA_T_STRING)
   return 1;
 else if (sscanf(svalue(obj), "%f %c",&t, &c) == 1)
 {
   nvalue(obj) = t;
   tag(obj) = LUA_T_NUMBER;
   return 0;
 }
 else
   return 2;
}


/*
** Convert, if possible, to a string tag
** Return 0 in success or not 0 on error.
*/
static int lua_tostring (Object *obj)
{
 static char s[256];
 if (tag(obj) != LUA_T_NUMBER)
   return 1;
 if ((int) nvalue(obj) == nvalue(obj))
   sprintf (s, "%d", (int) nvalue(obj));
 else
   sprintf (s, "%g", nvalue(obj));
 tsvalue(obj) = lua_createstring(s);
 if (tsvalue(obj) == NULL)
  return 1;
 tag(obj) = LUA_T_STRING;
 return 0;
}


/*
** Adjust stack. Set top to the given value, pushing NILs if needed.
*/
static void adjust_top (StkId newtop)
{
  Object *nt = stack+newtop;
  while (top < nt) tag(top++) = LUA_T_NIL;
  top = nt;  /* top could be bigger than newtop */
}


static void adjustC (int nParams)
{
  adjust_top(CBase+nParams);
}


/*
** Call a C function. CBase will point to the top of the stack,
** and CnResults is the number of parameters. Returns an index
** to the first result from C.
*/
static StkId callC (lua_CFunction func, StkId base)
{
  StkId oldBase = CBase;
  int oldCnResults = CnResults;
  StkId firstResult;
  CnResults = (top-stack) - base;
  /* incorporate parameters on the stack */
  CBase = base+CnResults;
  (*func)();
  firstResult = CBase;
  CBase = oldBase;
  CnResults = oldCnResults;
  return firstResult;
}

/*
** Call the fallback for invalid functions (see do_call)
*/
static void call_funcFB (Object *func, StkId base, int nResults, StkId whereRes)
{
  StkId i;
  /* open space for first parameter (func) */
  for (i=top-stack; i>base; i--)
    stack[i] = stack[i-1];
  top++;
  stack[base] = *func;
  do_call(&luaI_fallBacks[FB_FUNCTION].function, base, nResults, whereRes);
}


/*
** Call a function (C or Lua). The parameters must be on the stack,
** between [stack+base,top). When returns, the results are on the stack,
** between [stack+whereRes,top). The number of results is nResults, unless
** nResults=MULT_RET.
*/
static void do_call (Object *func, StkId base, int nResults, StkId whereRes)
{
  StkId firstResult;
  if (tag(func) == LUA_T_CFUNCTION)
    firstResult = callC(fvalue(func), base);
  else if (tag(func) == LUA_T_FUNCTION)
    firstResult = lua_execute(bvalue(func), base);
  else
  { /* func is not a function */
    call_funcFB(func, base, nResults, whereRes);
    return;
  }
  /* adjust the number of results */
  if (nResults != MULT_RET && top - (stack+firstResult) != nResults)
    adjust_top(firstResult+nResults);
  /* move results to the given position */
  if (firstResult != whereRes)
  {
    int i;
    nResults = top - (stack+firstResult);  /* actual number of results */
    for (i=0; i<nResults; i++)
      *(stack+whereRes+i) = *(stack+firstResult+i);
    top -= firstResult-whereRes;
  }
}


/*
** Function to index a table. Receives the table at top-2 and the index
** at top-1.
*/
static void pushsubscript (void)
{
  if (tag(top-2) != LUA_T_ARRAY)
    do_call(&luaI_fallBacks[FB_GETTABLE].function, (top-stack)-2, 1, (top-stack)-2);
  else 
  {
    Object *h = lua_hashget(avalue(top-2), top-1);
    if (h == NULL || tag(h) == LUA_T_NIL)
      do_call(&luaI_fallBacks[FB_INDEX].function, (top-stack)-2, 1, (top-stack)-2);
    else
    {
      --top;
      *(top-1) = *h;
    }
  }
}


/*
** Function to store indexed based on values at the top
*/
static void storesubscript (void)
{
 if (tag(top-3) != LUA_T_ARRAY)
   do_call(&luaI_fallBacks[FB_SETTABLE].function, (top-stack)-3, 0, (top-stack)-3);
 else
 {
  Object *h = lua_hashdefine (avalue(top-3), top-2);
  *h = *(top-1);
  top -= 3;
 }
}


/*
** Traverse all objects on stack
*/
void lua_travstack (void (*fn)(Object *))
{
 Object *o;
 for (o = top-1; o >= stack; o--)
  fn (o);
}


/*
** Execute a protected call. If function is null compiles the pre-set input.
** Leave nResults on the stack.
*/
static int do_protectedrun (Object *function, int nResults)
{
  jmp_buf myErrorJmp;
  int status;
  StkId oldCBase = CBase;
  jmp_buf *oldErr = errorJmp;
  errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp) == 0)
  {
    do_call(function, CBase, nResults, CBase);
    CnResults = (top-stack) - CBase;  /* number of results */
    CBase += CnResults;  /* incorporate results on the stack */
    status = 0;
  }
  else
  {
    CBase = oldCBase;
    top = stack+CBase;
    status = 1;
  }
  errorJmp = oldErr;
  return status;
}


static int do_protectedmain (void)
{
  Byte *code = NULL;
  int status;
  StkId oldCBase = CBase;
  jmp_buf myErrorJmp;
  jmp_buf *oldErr = errorJmp;
  errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp) == 0)
  {
    Object f;
    lua_parse(&code);
    tag(&f) = LUA_T_FUNCTION; bvalue(&f) = code;
    do_call(&f, CBase, 0, CBase);
    status = 0;
  }
  else
    status = 1;
  if (code)
    luaI_free(code);
  errorJmp = oldErr;
  CBase = oldCBase;
  top = stack+CBase;
  return status;
}


/*
** Execute the given lua function. Return 0 on success or 1 on error.
*/
int lua_callfunction (lua_Object function)
{
  if (function == LUA_NOOBJECT)
    return 1;
  else
    return do_protectedrun (Address(function), MULT_RET);
}


int lua_call (char *funcname)
{
 Word n = luaI_findsymbolbyname(funcname);
 return do_protectedrun(&s_object(n), MULT_RET);
}


/*
** Open file, generate opcode and execute global statement. Return 0 on
** success or 1 on error.
*/
int lua_dofile (char *filename)
{
  int status;
  char *message = lua_openfile (filename);
  if (message)
  {
    lua_message(message);
    return 1;
  }
  status = do_protectedmain();
  lua_closefile();
  return status;
}

/*
** Generate opcode stored on string and execute global statement. Return 0 on
** success or 1 on error.
*/
int lua_dostring (char *string)
{
  int status;
  char *message = lua_openstring(string);
  if (message)
  {
    lua_message(message);
    return 1;
  }
  status = do_protectedmain();
  lua_closestring();
  return status;
}


/*
** API: set a function as a fallback
*/
lua_Object lua_setfallback (char *name, lua_CFunction fallback)
{
  static Object func = {LUA_T_CFUNCTION, luaI_setfallback};
  adjustC(0);
  lua_pushstring(name);
  lua_pushcfunction(fallback);
  do_protectedrun(&func, 1);
  return (Ref(top-1));
}


/* 
** API: receives on the stack the table and the index.
** returns the value.
*/
lua_Object lua_getsubscript (void)
{
  adjustC(2);
  pushsubscript();
  CBase++;  /* incorporate object in the stack */
  return (Ref(top-1));
}


#define MAX_C_BLOCKS 10

static int numCblocks = 0;
static StkId Cblocks[MAX_C_BLOCKS];

/*
** API: starts a new block
*/
void lua_beginblock (void)
{
  if (numCblocks < MAX_C_BLOCKS)
    Cblocks[numCblocks] = CBase;
  numCblocks++;
}

/*
** API: ends a block
*/
void lua_endblock (void)
{
  --numCblocks;
  if (numCblocks < MAX_C_BLOCKS)
  {
    CBase = Cblocks[numCblocks];
    adjustC(0);
  }
}

/* 
** API: receives on the stack the table, the index, and the new value.
*/
void lua_storesubscript (void)
{
  adjustC(3);
  storesubscript();
}

/*
** API: creates a new table
*/
lua_Object lua_createtable (void)
{
  adjustC(0);
  avalue(top) = lua_createarray(0);
  tag(top) = LUA_T_ARRAY;
  top++;
  CBase++;  /* incorporate object in the stack */
  return Ref(top-1);
}

/*
** Get a parameter, returning the object handle or LUA_NOOBJECT on error.
** 'number' must be 1 to get the first parameter.
*/
lua_Object lua_getparam (int number)
{
 if (number <= 0 || number > CnResults) return LUA_NOOBJECT;
 /* Ref(stack+(CBase-CnResults+number-1)) ==
    stack+(CBase-CnResults+number-1)-stack+1 == */
 return CBase-CnResults+number;
}

/*
** Given an object handle, return its number value. On error, return 0.0.
*/
real lua_getnumber (lua_Object object)
{
 if (object == LUA_NOOBJECT || tag(Address(object)) == LUA_T_NIL) return 0.0;
 if (tonumber (Address(object))) return 0.0;
 else                   return (nvalue(Address(object)));
}

/*
** Given an object handle, return its string pointer. On error, return NULL.
*/
char *lua_getstring (lua_Object object)
{
 if (object == LUA_NOOBJECT || tag(Address(object)) == LUA_T_NIL) return NULL;
 if (tostring (Address(object))) return NULL;
 else return (svalue(Address(object)));
}

/*
** Given an object handle, return its cfuntion pointer. On error, return NULL.
*/
lua_CFunction lua_getcfunction (lua_Object object)
{
 if (object == LUA_NOOBJECT || tag(Address(object)) != LUA_T_CFUNCTION)
   return NULL;
 else return (fvalue(Address(object)));
}

/*
** Given an object handle, return its user data. On error, return NULL.
*/
void *lua_getuserdata (lua_Object object)
{
 if (object == LUA_NOOBJECT || tag(Address(object)) < LUA_T_USERDATA)
   return NULL;
 else return (uvalue(Address(object)));
}


lua_Object lua_getlocked (int ref)
{
 adjustC(0);
 *top = *luaI_getlocked(ref);
 top++;
 CBase++;  /* incorporate object in the stack */
 return Ref(top-1);
}


void lua_pushlocked (int ref)
{
 lua_checkstack(top-stack+1);
 *top = *luaI_getlocked(ref);
 top++;
}


int lua_lock (void)
{
  adjustC(1);
  return luaI_lock(--top);
}


/*
** Get a global object. Return the object handle or NULL on error.
*/
lua_Object lua_getglobal (char *name)
{
 Word n = luaI_findsymbolbyname(name);
 adjustC(0);
 *top = s_object(n);
 top++;
 CBase++;  /* incorporate object in the stack */
 return Ref(top-1);
}

/*
** Store top of the stack at a global variable array field.
*/
void lua_storeglobal (char *name)
{
 Word n = luaI_findsymbolbyname(name);
 adjustC(1);
 s_object(n) = *(--top);
}

/*
** Push a nil object
*/
void lua_pushnil (void)
{
 lua_checkstack(top-stack+1);
 tag(top++) = LUA_T_NIL;
}

/*
** Push an object (tag=number) to stack.
*/
void lua_pushnumber (real n)
{
 lua_checkstack(top-stack+1);
 tag(top) = LUA_T_NUMBER; nvalue(top++) = n;
}

/*
** Push an object (tag=string) to stack.
*/
void lua_pushstring (char *s)
{
 lua_checkstack(top-stack+1);
 tsvalue(top) = lua_createstring(s);
 tag(top) = LUA_T_STRING;
 top++;
}

/*
** Push an object (tag=string) on stack and register it on the constant table.
*/
void lua_pushliteral (char *s)
{
 lua_checkstack(top-stack+1);
 tsvalue(top) = lua_constant[luaI_findconstant(lua_constcreate(s))];
 tag(top) = LUA_T_STRING;
 top++;
}

/*
** Push an object (tag=cfunction) to stack.
*/
void lua_pushcfunction (lua_CFunction fn)
{
 lua_checkstack(top-stack+1);
 tag(top) = LUA_T_CFUNCTION; fvalue(top++) = fn;
}

/*
** Push an object (tag=userdata) to stack.
*/
void lua_pushusertag (void *u, int tag)
{
 if (tag < LUA_T_USERDATA) return;
 lua_checkstack(top-stack+1);
 tag(top) = tag; uvalue(top++) = u;
}

/*
** Push a lua_Object to stack.
*/
void lua_pushobject (lua_Object o)
{
 lua_checkstack(top-stack+1);
 *top++ = *Address(o);
}

/*
** Push an object on the stack.
*/
void luaI_pushobject (Object *o)
{
 lua_checkstack(top-stack+1);
 *top++ = *o;
}

int lua_type (lua_Object o)
{
  if (o == LUA_NOOBJECT)
    return LUA_T_NIL;
  else
    return tag(Address(o));
}


void luaI_gcFB (Object *o)
{
  *(top++) = *o;
  do_call(&luaI_fallBacks[FB_GC].function, (top-stack)-1, 0, (top-stack)-1);
}


static void call_arith (char *op)
{
  lua_pushstring(op);
  do_call(&luaI_fallBacks[FB_ARITH].function, (top-stack)-3, 1, (top-stack)-3);
}

static void comparison (lua_Type tag_less, lua_Type tag_equal, 
                        lua_Type tag_great, char *op)
{
  Object *l = top-2;
  Object *r = top-1;
  int result;
  if (tag(l) == LUA_T_NUMBER && tag(r) == LUA_T_NUMBER)
    result = (nvalue(l) < nvalue(r)) ? -1 : (nvalue(l) == nvalue(r)) ? 0 : 1;
  else if (tostring(l) || tostring(r))
  {
    lua_pushstring(op);
    do_call(&luaI_fallBacks[FB_ORDER].function, (top-stack)-3, 1, (top-stack)-3);
    return;
  }
  else
    result = strcmp(svalue(l), svalue(r));
  top--;
  nvalue(top-1) = 1;
  tag(top-1) = (result < 0) ? tag_less : (result == 0) ? tag_equal : tag_great;
}



/*
** Execute the given opcode, until a RET. Parameters are between
** [stack+base,top). Returns n such that the the results are between
** [stack+n,top).
*/
static StkId lua_execute (Byte *pc, StkId base)
{
 lua_checkstack(STACKGAP+MAX_TEMPS+base);
 while (1)
 {
  OpCode opcode;
  switch (opcode = (OpCode)*pc++)
  {
   case PUSHNIL: tag(top++) = LUA_T_NIL; break;

   case PUSH0: case PUSH1: case PUSH2:
     tag(top) = LUA_T_NUMBER;
     nvalue(top++) = opcode-PUSH0;
     break;

   case PUSHBYTE: tag(top) = LUA_T_NUMBER; nvalue(top++) = *pc++; break;

   case PUSHWORD:
   {
    CodeWord code;
    get_word(code,pc);
    tag(top) = LUA_T_NUMBER; nvalue(top++) = code.w;
   }
   break;

   case PUSHFLOAT:
   {
    CodeFloat code;
    get_float(code,pc);
    tag(top) = LUA_T_NUMBER; nvalue(top++) = code.f;
   }
   break;

   case PUSHSTRING:
   {
    CodeWord code;
    get_word(code,pc);
    tag(top) = LUA_T_STRING; tsvalue(top++) = lua_constant[code.w];
   }
   break;

   case PUSHFUNCTION:
   {
    CodeCode code;
    get_code(code,pc);
    tag(top) = LUA_T_FUNCTION; bvalue(top++) = code.b;
   }
   break;

   case PUSHLOCAL0: case PUSHLOCAL1: case PUSHLOCAL2:
   case PUSHLOCAL3: case PUSHLOCAL4: case PUSHLOCAL5:
   case PUSHLOCAL6: case PUSHLOCAL7: case PUSHLOCAL8:
   case PUSHLOCAL9: *top++ = *((stack+base) + (int)(opcode-PUSHLOCAL0)); break;

   case PUSHLOCAL: *top++ = *((stack+base) + (*pc++)); break;

   case PUSHGLOBAL:
   {
    CodeWord code;
    get_word(code,pc);
    *top++ = s_object(code.w);
   }
   break;

   case PUSHINDEXED:
    pushsubscript();
    break;

   case PUSHSELF:
   {
     Object receiver = *(top-1);
     CodeWord code;
     get_word(code,pc);
     tag(top) = LUA_T_STRING; tsvalue(top++) = lua_constant[code.w];
     pushsubscript();
     *(top++) = receiver;
     break;
   }

   case STORELOCAL0: case STORELOCAL1: case STORELOCAL2:
   case STORELOCAL3: case STORELOCAL4: case STORELOCAL5:
   case STORELOCAL6: case STORELOCAL7: case STORELOCAL8:
   case STORELOCAL9:
     *((stack+base) + (int)(opcode-STORELOCAL0)) = *(--top);
     break;

   case STORELOCAL: *((stack+base) + (*pc++)) = *(--top); break;

   case STOREGLOBAL:
   {
    CodeWord code;
    get_word(code,pc);
    s_object(code.w) = *(--top);
   }
   break;

   case STOREINDEXED0:
    storesubscript();
    break;

   case STOREINDEXED:
   {
    int n = *pc++;
    if (tag(top-3-n) != LUA_T_ARRAY)
    {
      *(top+1) = *(top-1);
      *(top) = *(top-2-n);
      *(top-1) = *(top-3-n);
      top += 2;
      do_call(&luaI_fallBacks[FB_SETTABLE].function, (top-stack)-3, 0, (top-stack)-3);
    }
    else
    {
     Object *h = lua_hashdefine (avalue(top-3-n), top-2-n);
     *h = *(top-1);
     top--;
    }
   }
   break;

   case STORELIST0:
   case STORELIST:
   {
    int m, n;
    Object *arr;
    if (opcode == STORELIST0) m = 0;
    else m = *(pc++) * FIELDS_PER_FLUSH;
    n = *(pc++);
    arr = top-n-1;
    while (n)
    {
     tag(top) = LUA_T_NUMBER; nvalue(top) = n+m;
     *(lua_hashdefine (avalue(arr), top)) = *(top-1);
     top--;
     n--;
    }
   }
   break;

   case STORERECORD:
   {
    int n = *(pc++);
    Object *arr = top-n-1;
    while (n)
    {
     CodeWord code;
     get_word(code,pc);
     tag(top) = LUA_T_STRING; tsvalue(top) = lua_constant[code.w];
     *(lua_hashdefine (avalue(arr), top)) = *(top-1);
     top--;
     n--;
    }
   }
   break;

   case ADJUST0:
     adjust_top(base);
     break;

   case ADJUST:
     adjust_top(base + *(pc++));
     break;

   case CREATEARRAY:
   {
    CodeWord size;
    get_word(size,pc);
    avalue(top) = lua_createarray(size.w);
    tag(top) = LUA_T_ARRAY;
    top++;
   }
   break;

   case EQOP:
   {
    int res = lua_equalObj(top-2, top-1);
    --top;
    tag(top-1) = res ? LUA_T_NUMBER : LUA_T_NIL;
    nvalue(top-1) = 1;
   }
   break;

    case LTOP:
      comparison(LUA_T_NUMBER, LUA_T_NIL, LUA_T_NIL, "lt");
      break;

   case LEOP:
      comparison(LUA_T_NUMBER, LUA_T_NUMBER, LUA_T_NIL, "le");
      break;

   case GTOP:
      comparison(LUA_T_NIL, LUA_T_NIL, LUA_T_NUMBER, "gt");
      break;

   case GEOP:
      comparison(LUA_T_NIL, LUA_T_NUMBER, LUA_T_NUMBER, "ge");
      break;

   case ADDOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tonumber(r) || tonumber(l))
      call_arith("add");
    else
    {
      nvalue(l) += nvalue(r);
      --top;
    }
   }
   break;

   case SUBOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tonumber(r) || tonumber(l))
      call_arith("sub");
    else
    {
      nvalue(l) -= nvalue(r);
      --top;
    }
   }
   break;

   case MULTOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tonumber(r) || tonumber(l))
      call_arith("mul");
    else
    {
      nvalue(l) *= nvalue(r);
      --top;
    }
   }
   break;

   case DIVOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tonumber(r) || tonumber(l))
      call_arith("div");
    else
    {
      nvalue(l) /= nvalue(r);
      --top;
    }
   }
   break;

   case POWOP:
    call_arith("pow");
    break;

   case CONCOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tostring(r) || tostring(l))
      do_call(&luaI_fallBacks[FB_CONCAT].function, (top-stack)-2, 1, (top-stack)-2);
    else
    {
      tsvalue(l) = lua_createstring (lua_strconc(svalue(l),svalue(r)));
      --top;
    }
   }
   break;

   case MINUSOP:
    if (tonumber(top-1))
    {
      tag(top++) = LUA_T_NIL;
      call_arith("unm");
    }
    else
      nvalue(top-1) = - nvalue(top-1);
   break;

   case NOTOP:
    tag(top-1) = (tag(top-1) == LUA_T_NIL) ? LUA_T_NUMBER : LUA_T_NIL;
    nvalue(top-1) = 1;
   break;

   case ONTJMP:
   {
    CodeWord code;
    get_word(code,pc);
    if (tag(top-1) != LUA_T_NIL) pc += code.w;
   }
   break;

   case ONFJMP:	
   {
    CodeWord code;
    get_word(code,pc);
    if (tag(top-1) == LUA_T_NIL) pc += code.w;
   }
   break;

   case JMP:
   {
    CodeWord code;
    get_word(code,pc);
    pc += code.w;
   }
   break;

   case UPJMP:
   {
    CodeWord code;
    get_word(code,pc);
    pc -= code.w;
   }
   break;

   case IFFJMP:
   {
    CodeWord code;
    get_word(code,pc);
    top--;
    if (tag(top) == LUA_T_NIL) pc += code.w;
   }
   break;

   case IFFUPJMP:
   {
    CodeWord code;
    get_word(code,pc);
    top--;
    if (tag(top) == LUA_T_NIL) pc -= code.w;
   }
   break;

   case POP: --top; break;

   case CALLFUNC:
   {
     int nParams = *(pc++);
     int nResults = *(pc++);
     Object *func = top-1-nParams; /* function is below parameters */
     StkId newBase = (top-stack)-nParams;
     do_call(func, newBase, nResults, newBase-1);
   }
   break;

   case RETCODE0:
     return base;

   case RETCODE:
     return base+*pc;

   case SETFUNCTION:
   {
    CodeCode file;
    CodeWord func;
    get_code(file,pc);
    get_word(func,pc);
    lua_pushfunction ((char *)file.b, func.w);
   }
   break;

   case SETLINE:
   {
    CodeWord code;
    get_word(code,pc);
    lua_debugline = code.w;
   }
   break;

   case RESET:
    lua_popfunction ();
   break;

   default:
    lua_error ("internal error - opcode doesn't match");
  }
 }
}


