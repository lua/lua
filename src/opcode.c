/*
** opcode.c
** TecCGraf - PUC-Rio
*/

char *rcs_opcode="$Id: opcode.c,v 4.15 1997/06/26 21:40:57 roberto Exp $";

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "luadebug.h"
#include "luamem.h"
#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"
#include "fallback.h"
#include "auxlib.h"
#include "lex.h"

#define tonumber(o) ((ttype(o) != LUA_T_NUMBER) && (lua_tonumber(o) != 0))
#define tostring(o) ((ttype(o) != LUA_T_STRING) && (lua_tostring(o) != 0))


#define STACK_SIZE 	128

#ifndef STACK_LIMIT
#define STACK_LIMIT     6000
#endif

typedef int StkId;  /* index to stack elements */

static TObject initial_stack;

static TObject *stackLimit = &initial_stack+1;
static TObject *stack = &initial_stack;
static TObject *top = &initial_stack;


/* macros to convert from lua_Object to (TObject *) and back */
 
#define Address(lo)     ((lo)+stack-1)
#define Ref(st)         ((st)-stack+1)
 

/* macro to increment stack top. There must be always an empty slot in
*  the stack
*/
#define incr_top	if (++top >= stackLimit) growstack()

struct C_Lua_Stack {
  StkId base;  /* when Lua calls C or C calls Lua, points to */
               /* the first slot after the last parameter. */
  StkId lua2C; /* points to first element of "array" lua2C */
  int num;     /* size of "array" lua2C */
};

static struct C_Lua_Stack CLS_current = {0, 0, 0};

static  jmp_buf *errorJmp = NULL; /* current error recover point */


/* Hooks */
lua_LHFunction lua_linehook = NULL;
lua_CHFunction lua_callhook = NULL;


static StkId lua_execute (Byte *pc, StkId base);
static void do_call (StkId base, int nResults);



TObject *luaI_Address (lua_Object o)
{
  return Address(o);
}


/*
** Init stack
*/
static void lua_initstack (void)
{
 Long maxstack = STACK_SIZE;
 stack = newvector(maxstack, TObject);
 stackLimit = stack+maxstack;
 top = stack;
 *(top++) = initial_stack;
}


/*
** Check stack overflow and, if necessary, realloc vector
*/
#define lua_checkstack(nt)  if ((nt) >= stackLimit) growstack()

static void growstack (void)
{
 if (stack == &initial_stack)
   lua_initstack();
 else
 {
  static int limit = STACK_LIMIT;
  StkId t = top-stack;
  Long stacksize = stackLimit - stack;
  stacksize = growvector(&stack, stacksize, TObject, stackEM, limit+100);
  stackLimit = stack+stacksize;
  top = stack + t;
  if (stacksize >= limit)
  {
    limit = stacksize;
    lua_error(stackEM);
  }
 }
}


/*
** Concatenate two given strings. Return the new string pointer.
*/
static char *lua_strconc (char *l, char *r)
{
  size_t nl = strlen(l);
  char *buffer = luaI_buffer(nl+strlen(r)+1);
  strcpy(buffer, l);
  strcpy(buffer+nl, r);
  return buffer;
}


/*
** Convert, if possible, to a number object.
** Return 0 if success, not 0 if error.
*/
static int lua_tonumber (TObject *obj)
{
 float t;
 char c;
 if (ttype(obj) != LUA_T_STRING)
   return 1;
 else if (sscanf(svalue(obj), "%f %c",&t, &c) == 1)
 {
   nvalue(obj) = t;
   ttype(obj) = LUA_T_NUMBER;
   return 0;
 }
 else
   return 2;
}


/*
** Convert, if possible, to a string ttype
** Return 0 in success or not 0 on error.
*/
static int lua_tostring (TObject *obj)
{
  if (ttype(obj) != LUA_T_NUMBER)
    return 1;
  else {
    char s[60];
    real f = nvalue(obj);
    int i;
    if ((real)(-MAX_INT) <= f && f <= (real)MAX_INT && (real)(i=(int)f) == f)
      sprintf (s, "%d", i);
    else
      sprintf (s, "%g", nvalue(obj));
    tsvalue(obj) = lua_createstring(s);
    ttype(obj) = LUA_T_STRING;
    return 0;
  }
}


/*
** Adjust stack. Set top to the given value, pushing NILs if needed.
*/
static void adjust_top_aux (StkId newtop)
{
  TObject *nt;
  lua_checkstack(stack+newtop);
  nt = stack+newtop;  /* warning: previous call may change stack */
  while (top < nt) ttype(top++) = LUA_T_NIL;
}


#define adjust_top(newtop)  { if (newtop <= top-stack) \
                                 top = stack+newtop; \
                              else adjust_top_aux(newtop); }

#define adjustC(nParams)	adjust_top(CLS_current.base+nParams)


static void checkCparams (int nParams)
{
  if (top-stack < CLS_current.base+nParams)
    lua_error("API error - wrong number of arguments in C2lua stack");
}


/*
** Open a hole below "nelems" from the top.
*/
static void open_stack (int nelems)
{
  int i;
  for (i=0; i<nelems; i++)
    *(top-i) = *(top-i-1);
  incr_top;
}


static lua_Object put_luaObject (TObject *o)
{
  open_stack((top-stack)-CLS_current.base);
  stack[CLS_current.base++] = *o;
  return CLS_current.base;  /* this is +1 real position (see Ref) */
}


static lua_Object put_luaObjectonTop (void)
{
  open_stack((top-stack)-CLS_current.base);
  stack[CLS_current.base++] = *(--top);
  return CLS_current.base;  /* this is +1 real position (see Ref) */
}


lua_Object lua_pop (void)
{
  checkCparams(1);
  return put_luaObjectonTop();
}



/*
** call Line hook
*/
static void lineHook (int line)
{
  struct C_Lua_Stack oldCLS = CLS_current;
  StkId old_top = CLS_current.lua2C = CLS_current.base = top-stack;
  CLS_current.num = 0;
  (*lua_linehook)(line);
  top = stack+old_top;
  CLS_current = oldCLS;
}


/*
** Call hook
** The function being called is in [stack+base-1]
*/
static void callHook (StkId base, lua_Type type, int isreturn)
{
  struct C_Lua_Stack oldCLS = CLS_current;
  StkId old_top = CLS_current.lua2C = CLS_current.base = top-stack;
  CLS_current.num = 0;
  if (isreturn)
    (*lua_callhook)(LUA_NOOBJECT, "(return)", 0);
  else
  {
    TObject *f = stack+base-1;
    if (type == LUA_T_MARK)
      (*lua_callhook)(Ref(f), f->value.tf->fileName, f->value.tf->lineDefined);
    else
      (*lua_callhook)(Ref(f), "(C)", -1);
  }
  top = stack+old_top;
  CLS_current = oldCLS;
}


/*
** Call a C function. CLS_current.base will point to the top of the stack,
** and CLS_current.num is the number of parameters. Returns an index
** to the first result from C.
*/
static StkId callC (lua_CFunction func, StkId base)
{
  struct C_Lua_Stack oldCLS = CLS_current;
  StkId firstResult;
  CLS_current.num = (top-stack) - base;
  /* incorporate parameters on the stack */
  CLS_current.lua2C = base;
  CLS_current.base = base+CLS_current.num;  /* == top-stack */
  if (lua_callhook)
    callHook(base, LUA_T_CMARK, 0);
  (*func)();
  if (lua_callhook)  /* func may have changed lua_callhook */
    callHook(base, LUA_T_CMARK, 1);
  firstResult = CLS_current.base;
  CLS_current = oldCLS;
  return firstResult;
}

static void callIM (TObject *f, int nParams, int nResults)
{
  open_stack(nParams);
  *(top-nParams-1) = *f;
  do_call((top-stack)-nParams, nResults);
}


/*
** Call a function (C or Lua). The parameters must be on the stack,
** between [stack+base,top). The function to be called is at stack+base-1.
** When returns, the results are on the stack, between [stack+base-1,top).
** The number of results is nResults, unless nResults=MULT_RET.
*/
static void do_call (StkId base, int nResults)
{
  StkId firstResult;
  TObject *func = stack+base-1;
  int i;
  if (ttype(func) == LUA_T_CFUNCTION) {
    ttype(func) = LUA_T_CMARK;
    firstResult = callC(fvalue(func), base);
  }
  else if (ttype(func) == LUA_T_FUNCTION) {
    ttype(func) = LUA_T_MARK;
    firstResult = lua_execute(func->value.tf->code, base);
  }
  else { /* func is not a function */
    /* Check the tag method for invalid functions */
    TObject *im = luaI_getimbyObj(func, IM_FUNCTION);
    if (ttype(im) == LUA_T_NIL)
      lua_error("call expression not a function");
    open_stack((top-stack)-(base-1));
    stack[base-1] = *im;
    do_call(base, nResults);
    return;
  }
  /* adjust the number of results */
  if (nResults != MULT_RET)
    adjust_top(firstResult+nResults);
  /* move results to base-1 (to erase parameters and function) */
  base--;
  nResults = top - (stack+firstResult);  /* actual number of results */
  for (i=0; i<nResults; i++)
    *(stack+base+i) = *(stack+firstResult+i);
  top -= firstResult-base;
}


/*
** Function to index a table. Receives the table at top-2 and the index
** at top-1.
*/
static void pushsubscript (void)
{
  TObject *im;
  if (ttype(top-2) != LUA_T_ARRAY)  /* not a table, get "gettable" method */
    im = luaI_getimbyObj(top-2, IM_GETTABLE);
  else {  /* object is a table... */
    int tg = (top-2)->value.a->htag;
    im = luaI_getim(tg, IM_GETTABLE);
    if (ttype(im) == LUA_T_NIL) {  /* and does not have a "gettable" method */
      TObject *h = lua_hashget(avalue(top-2), top-1);
      if (h != NULL && ttype(h) != LUA_T_NIL) {
        --top;
        *(top-1) = *h;
      }
      else if (ttype(im=luaI_getim(tg, IM_INDEX)) != LUA_T_NIL)
        callIM(im, 2, 1);
      else {
        --top;
        ttype(top-1) = LUA_T_NIL;
      }
      return;
    }
    /* else it has a "gettable" method, go through to next command */
  }
  /* object is not a table, or it has a "gettable" method */ 
  if (ttype(im) != LUA_T_NIL)
    callIM(im, 2, 1);
  else
    lua_error("indexed expression not a table");
}


lua_Object lua_rawgettable (void)
{
  checkCparams(2);
  if (ttype(top-2) != LUA_T_ARRAY)
    lua_error("indexed expression not a table in raw gettable");
  else {
    TObject *h = lua_hashget(avalue(top-2), top-1);
    --top;
    if (h != NULL)
      *(top-1) = *h;
    else
      ttype(top-1) = LUA_T_NIL;
  }
  return put_luaObjectonTop();
}


/*
** Function to store indexed based on values at the top
** mode = 0: raw store (without internal methods)
** mode = 1: normal store (with internal methods)
** mode = 2: "deep stack" store (with internal methods)
*/
static void storesubscript (TObject *t, int mode)
{
  TObject *im = (mode == 0) ? NULL : luaI_getimbyObj(t, IM_SETTABLE);
  if (ttype(t) == LUA_T_ARRAY && (im == NULL || ttype(im) == LUA_T_NIL)) {
    TObject *h = lua_hashdefine(avalue(t), t+1);
    *h = *(top-1);
    top -= (mode == 2) ? 1 : 3;
  }
  else {  /* object is not a table, and/or has a specific "settable" method */
    if (im && ttype(im) != LUA_T_NIL) {
      if (mode == 2) {
        lua_checkstack(top+2);
        *(top+1) = *(top-1);
        *(top) = *(t+1);
        *(top-1) = *t;
        top += 2;
      }
      callIM(im, 3, 0);
    }
    else
      lua_error("indexed expression not a table");
  }
}


static void getglobal (Word n)
{
  TObject *value = &lua_table[n].object;
  TObject *im = luaI_getimbyObj(value, IM_GETGLOBAL);
  if (ttype(im) == LUA_T_NIL) {  /* default behavior */
    *top = *value;
    incr_top;
  }
  else {
    ttype(top) = LUA_T_STRING;
    tsvalue(top) = lua_table[n].varname;
    incr_top;
    *top = *value;
    incr_top;
    callIM(im, 2, 1);
    }
}

/*
** Traverse all objects on stack
*/
void lua_travstack (int (*fn)(TObject *))
{
  StkId i;
  for (i = (top-1)-stack; i>=0; i--)
    fn (stack+i);
}


/*
** Error messages and debug functions
*/

static void lua_message (char *s)
{
  TObject *im = luaI_geterrorim();
  if (ttype(im) != LUA_T_NIL) {
    lua_pushstring(s);
    callIM(im, 1, 0);
  }
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


lua_Function lua_stackedfunction (int level)
{
  StkId i;
  for (i = (top-1)-stack; i>=0; i--)
    if (stack[i].ttype == LUA_T_MARK || stack[i].ttype == LUA_T_CMARK)
      if (level-- == 0)
        return Ref(stack+i);
  return LUA_NOOBJECT;
}


int lua_currentline (lua_Function func)
{
  TObject *f = Address(func);
  return (f+1 < top && (f+1)->ttype == LUA_T_LINE) ? (f+1)->value.i : -1;
}


lua_Object lua_getlocal (lua_Function func, int local_number, char **name)
{
  TObject *f = luaI_Address(func);
  *name = luaI_getlocalname(f->value.tf, local_number, lua_currentline(func));
  if (*name)
  {
    /* if "*name", there must be a LUA_T_LINE */
    /* therefore, f+2 points to function base */
    return Ref((f+2)+(local_number-1));
  }
  else
    return LUA_NOOBJECT;
}

int lua_setlocal (lua_Function func, int local_number)
{
  TObject *f = Address(func);
  char *name = luaI_getlocalname(f->value.tf, local_number, lua_currentline(func));
  checkCparams(1);
  --top;
  if (name)
  {
    /* if "name", there must be a LUA_T_LINE */
    /* therefore, f+2 points to function base */
    *((f+2)+(local_number-1)) = *top;
    return 1;
  }
  else
    return 0;
}

/*
** Call the function at CLS_current.base, and incorporate results on
** the Lua2C structure.
*/
static void do_callinc (int nResults)
{
  StkId base = CLS_current.base;
  do_call(base+1, nResults);
  CLS_current.lua2C = base;  /* position of the new results */
  CLS_current.num = (top-stack) - base;  /* number of results */
  CLS_current.base = base + CLS_current.num;  /* incorporate results on stack */
}


static void do_unprotectedrun (lua_CFunction f, int nParams, int nResults)
{
  StkId base = (top-stack)-nParams;
  open_stack(nParams);
  stack[base].ttype = LUA_T_CFUNCTION;
  stack[base].value.f = f;
  do_call(base+1, nResults);
}


/*
** Execute a protected call. Assumes that function is at CLS_current.base and
** parameters are on top of it. Leave nResults on the stack.
*/
static int do_protectedrun (int nResults)
{
  jmp_buf myErrorJmp;
  int status;
  struct C_Lua_Stack oldCLS = CLS_current;
  jmp_buf *oldErr = errorJmp;
  errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp) == 0) {
    do_callinc(nResults);
    status = 0;
  }
  else { /* an error occurred: restore CLS_current and top */
    CLS_current = oldCLS;
    top = stack+CLS_current.base;
    status = 1;
  }
  errorJmp = oldErr;
  return status;
}

int luaI_dorun (TFunc *tf)
{
  int status;
  adjustC(1);  /* one slot for the pseudo-function */
  stack[CLS_current.base].ttype = LUA_T_FUNCTION;
  stack[CLS_current.base].value.tf = tf;
  status = do_protectedrun(MULT_RET);
  return status;
}


int lua_domain (void)
{
  TFunc tf;
  int status;
  jmp_buf myErrorJmp;
  jmp_buf *oldErr = errorJmp;
  errorJmp = &myErrorJmp;
  luaI_initTFunc(&tf);
  if (setjmp(myErrorJmp) == 0) {
    lua_parse(&tf);
    status = 0;
  }
  else {
    adjustC(0);  /* erase extra slot */
    status = 1;
  }
  if (status == 0)
    status = luaI_dorun(&tf);
  errorJmp = oldErr;
  luaI_free(tf.code);
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
  {
    open_stack((top-stack)-CLS_current.base);
    stack[CLS_current.base] = *Address(function);
    return do_protectedrun (MULT_RET);
  }
}


lua_Object lua_gettagmethod (int tag, char *event)
{
  lua_pushnumber(tag);
  lua_pushstring(event);
  do_unprotectedrun(luaI_gettagmethod, 2, 1);
  return put_luaObjectonTop();
}

lua_Object lua_settagmethod (int tag, char *event)
{
  TObject newmethod;
  checkCparams(1);
  newmethod = *(--top);
  lua_pushnumber(tag);
  lua_pushstring(event);
  *top = newmethod;  incr_top;
  do_unprotectedrun(luaI_settagmethod, 3, 1);
  return put_luaObjectonTop();
}

lua_Object lua_seterrormethod (void)
{
  checkCparams(1);
  do_unprotectedrun(luaI_seterrormethod, 1, 1);
  return put_luaObjectonTop();
}


/* 
** API: receives on the stack the table and the index.
** returns the value.
*/
lua_Object lua_gettable (void)
{
  checkCparams(2);
  pushsubscript();
  return put_luaObjectonTop();
}


#define MAX_C_BLOCKS 10

static int numCblocks = 0;
static struct C_Lua_Stack Cblocks[MAX_C_BLOCKS];

/*
** API: starts a new block
*/
void lua_beginblock (void)
{
  if (numCblocks >= MAX_C_BLOCKS)
    lua_error("`lua_beginblock': too many nested blocks");
  Cblocks[numCblocks] = CLS_current;
  numCblocks++;
}

/*
** API: ends a block
*/
void lua_endblock (void)
{
  --numCblocks;
  CLS_current = Cblocks[numCblocks];
  adjustC(0);
}

void lua_settag (int tag)
{
  checkCparams(1);
  luaI_settag(tag, --top);
}

/* 
** API: receives on the stack the table, the index, and the new value.
*/
void lua_settable (void)
{
  checkCparams(3);
  storesubscript(top-3, 1);
}

void lua_rawsettable (void)
{
  checkCparams(3);
  storesubscript(top-3, 0);
}

/*
** API: creates a new table
*/
lua_Object lua_createtable (void)
{
  TObject o;
  avalue(&o) = lua_createarray(0);
  ttype(&o) = LUA_T_ARRAY;
  return put_luaObject(&o);
}

/*
** Get a parameter, returning the object handle or LUA_NOOBJECT on error.
** 'number' must be 1 to get the first parameter.
*/
lua_Object lua_lua2C (int number)
{
  if (number <= 0 || number > CLS_current.num) return LUA_NOOBJECT;
  /* Ref(stack+(CLS_current.lua2C+number-1)) ==
     stack+(CLS_current.lua2C+number-1)-stack+1 == */
  return CLS_current.lua2C+number;
}

int lua_isnil (lua_Object o)
{
  return (o!= LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_NIL);
}

int lua_istable (lua_Object o)
{
  return (o!= LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_ARRAY);
}

int lua_isuserdata (lua_Object o)
{
  return (o!= LUA_NOOBJECT) && (ttype(Address(o)) == LUA_T_USERDATA);
}

int lua_iscfunction (lua_Object o)
{
  int t = lua_tag(o);
  return (t == LUA_T_CMARK) || (t == LUA_T_CFUNCTION);
}

int lua_isnumber (lua_Object o)
{
  return (o!= LUA_NOOBJECT) && (tonumber(Address(o)) == 0);
}

int lua_isstring (lua_Object o)
{
  int t = lua_tag(o);
  return (t == LUA_T_STRING) || (t == LUA_T_NUMBER);
}

int lua_isfunction (lua_Object o)
{
  int t = lua_tag(o);
  return (t == LUA_T_FUNCTION) || (t == LUA_T_CFUNCTION) ||
         (t == LUA_T_MARK) || (t == LUA_T_CMARK);
}

/*
** Given an object handle, return its number value. On error, return 0.0.
*/
real lua_getnumber (lua_Object object)
{
 if (object == LUA_NOOBJECT) return 0.0;
 if (tonumber (Address(object))) return 0.0;
 else return (nvalue(Address(object)));
}

/*
** Given an object handle, return its string pointer. On error, return NULL.
*/
char *lua_getstring (lua_Object object)
{
  if (object == LUA_NOOBJECT || tostring (Address(object)))
    return NULL;
  else return (svalue(Address(object)));
}


void *lua_getuserdata (lua_Object object)
{
  if (object == LUA_NOOBJECT || ttype(Address(object)) != LUA_T_USERDATA)
    return NULL;
  else return tsvalue(Address(object))->u.v;
}


/*
** Given an object handle, return its cfuntion pointer. On error, return NULL.
*/
lua_CFunction lua_getcfunction (lua_Object object)
{
 if (object == LUA_NOOBJECT || ((ttype(Address(object)) != LUA_T_CFUNCTION) &&
                                (ttype(Address(object)) != LUA_T_CMARK)))
   return NULL;
 else return (fvalue(Address(object)));
}


lua_Object lua_getref (int ref)
{
  TObject *o = luaI_getref(ref);
  if (o == NULL)
    return LUA_NOOBJECT;
  return put_luaObject(o);
}


int lua_ref (int lock)
{
  checkCparams(1);
  return luaI_ref(--top, lock);
}



/*
** Get a global object.
*/
lua_Object lua_getglobal (char *name)
{
  getglobal(luaI_findsymbolbyname(name));
  return put_luaObjectonTop();
}


lua_Object lua_rawgetglobal (char *name)
{
  return put_luaObject(&lua_table[luaI_findsymbolbyname(name)].object);
}


/*
** Store top of the stack at a global variable array field.
*/
static void setglobal (Word n)
{
  TObject *oldvalue = &lua_table[n].object;
  TObject *im = luaI_getimbyObj(oldvalue, IM_SETGLOBAL);
  if (ttype(im) == LUA_T_NIL)  /* default behavior */
    s_object(n) = *(--top);
  else {
    TObject newvalue = *(top-1);
    ttype(top-1) = LUA_T_STRING;
    tsvalue(top-1) = lua_table[n].varname;
    *top = *oldvalue;
    incr_top;
    *top = newvalue;
    incr_top;
    callIM(im, 3, 0);
  }
}


void lua_setglobal (char *name)
{
  checkCparams(1);
  setglobal(luaI_findsymbolbyname(name));
}

void lua_rawsetglobal (char *name)
{
  Word n = luaI_findsymbolbyname(name);
  checkCparams(1);
  s_object(n) = *(--top);
}

/*
** Push a nil object
*/
void lua_pushnil (void)
{
  ttype(top) = LUA_T_NIL;
  incr_top;
}

/*
** Push an object (ttype=number) to stack.
*/
void lua_pushnumber (real n)
{
 ttype(top) = LUA_T_NUMBER; nvalue(top) = n;
 incr_top;
}

/*
** Push an object (ttype=string) to stack.
*/
void lua_pushstring (char *s)
{
  if (s == NULL)
    ttype(top) = LUA_T_NIL;
  else
  {
    tsvalue(top) = lua_createstring(s);
    ttype(top) = LUA_T_STRING;
  }
  incr_top;
}


/*
** Push an object (ttype=cfunction) to stack.
*/
void lua_pushcfunction (lua_CFunction fn)
{
 ttype(top) = LUA_T_CFUNCTION; fvalue(top) = fn;
 incr_top;
}



void lua_pushusertag (void *u, int tag)
{
  if (tag < 0 && tag != LUA_ANYTAG)
    luaI_realtag(tag);  /* error if tag is not valid */
  tsvalue(top) = luaI_createudata(u, tag);
  ttype(top) = LUA_T_USERDATA;
  incr_top;
}

/*
** Push an object on the stack.
*/
void luaI_pushobject (TObject *o)
{
 *top = *o;
 incr_top;
}

/*
** Push a lua_Object on stack.
*/
void lua_pushobject (lua_Object o)
{
  if (o == LUA_NOOBJECT)
    lua_error("API error - attempt to push a NOOBJECT");
  *top = *Address(o);
  if (ttype(top) == LUA_T_MARK) ttype(top) = LUA_T_FUNCTION;
  else if (ttype(top) == LUA_T_CMARK) ttype(top) = LUA_T_CFUNCTION;
  incr_top;
}

int lua_tag (lua_Object lo)
{
  if (lo == LUA_NOOBJECT) return LUA_T_NIL;
  else {
    TObject *o = Address(lo);
    lua_Type t = ttype(o);
    if (t == LUA_T_USERDATA)
      return o->value.ts->tag;
    else if (t == LUA_T_ARRAY)
      return o->value.a->htag;
    else return t;
  }
}


void luaI_gcIM (TObject *o)
{
  TObject *im = luaI_getimbyObj(o, IM_GC);
  if (ttype(im) != LUA_T_NIL) {
    *top = *o;
    incr_top;
    callIM(im, 1, 0);
  }
}


static void call_binTM (IMS event, char *msg)
{
  TObject *im = luaI_getimbyObj(top-2, event);  /* try first operand */
  if (ttype(im) == LUA_T_NIL) {
    im = luaI_getimbyObj(top-1, event);  /* try second operand */
    if (ttype(im) == LUA_T_NIL) {
      im = luaI_getim(0, event);  /* try a 'global' i.m. */
      if (ttype(im) == LUA_T_NIL)
        lua_error(msg);
    }
  }
  lua_pushstring(luaI_eventname[event]);
  callIM(im, 3, 1);
}


static void call_arith (IMS event)
{
  call_binTM(event, "unexpected type at arithmetic operation");
}


static void comparison (lua_Type ttype_less, lua_Type ttype_equal, 
                        lua_Type ttype_great, IMS op)
{
  TObject *l = top-2;
  TObject *r = top-1;
  int result;
  if (ttype(l) == LUA_T_NUMBER && ttype(r) == LUA_T_NUMBER)
    result = (nvalue(l) < nvalue(r)) ? -1 : (nvalue(l) == nvalue(r)) ? 0 : 1;
  else if (ttype(l) == LUA_T_STRING && ttype(r) == LUA_T_STRING)
    result = strcmp(svalue(l), svalue(r));
  else {
    call_binTM(op, "unexpected type at comparison");
    return;
  }
  top--;
  nvalue(top-1) = 1;
  ttype(top-1) = (result < 0) ? ttype_less :
                                (result == 0) ? ttype_equal : ttype_great;
}


static void adjust_varargs (StkId first_extra_arg)
{
  TObject arg;
  TObject *firstelem = stack+first_extra_arg;
  int nvararg = top-firstelem;
  int i;
  if (nvararg < 0) nvararg = 0;
  avalue(&arg)  = lua_createarray(nvararg+1);  /* +1 for field 'n' */
  ttype(&arg) = LUA_T_ARRAY;
  for (i=0; i<nvararg; i++) {
    TObject index;
    ttype(&index) = LUA_T_NUMBER;
    nvalue(&index) = i+1;
    *(lua_hashdefine(avalue(&arg), &index)) = *(firstelem+i);
  }
  /* store counter in field "n" */ {
    TObject index, extra;
    ttype(&index) = LUA_T_STRING;
    tsvalue(&index) = lua_createstring("n");
    ttype(&extra) = LUA_T_NUMBER;
    nvalue(&extra) = nvararg;
    *(lua_hashdefine(avalue(&arg), &index)) = extra;
  }
  adjust_top(first_extra_arg);
  *top = arg; incr_top;
}



/*
** Execute the given opcode, until a RET. Parameters are between
** [stack+base,top). Returns n such that the the results are between
** [stack+n,top).
*/
static StkId lua_execute (Byte *pc, StkId base)
{
  if (lua_callhook)
    callHook (base, LUA_T_MARK, 0);
 while (1)
 {
  OpCode opcode;
  switch (opcode = (OpCode)*pc++)
  {
   case PUSHNIL: ttype(top) = LUA_T_NIL; incr_top; break;

   case PUSH0: case PUSH1: case PUSH2:
     ttype(top) = LUA_T_NUMBER;
     nvalue(top) = opcode-PUSH0;
     incr_top;
     break;

   case PUSHBYTE: 
     ttype(top) = LUA_T_NUMBER; nvalue(top) = *pc++; incr_top; break;

   case PUSHWORD:
   {
    Word w;
    get_word(w,pc);
    ttype(top) = LUA_T_NUMBER; nvalue(top) = w;
    incr_top;
   }
   break;

   case PUSHFLOAT:
   {
    real num;
    get_float(num,pc);
    ttype(top) = LUA_T_NUMBER; nvalue(top) = num;
    incr_top;
   }
   break;

   case PUSHSTRING:
   {
    Word w;
    get_word(w,pc);
    ttype(top) = LUA_T_STRING; tsvalue(top) = lua_constant[w];
    incr_top;
   }
   break;

   case PUSHFUNCTION:
   {
    TFunc *f;
    get_code(f,pc);
    luaI_insertfunction(f);  /* may take part in GC */
    top->ttype = LUA_T_FUNCTION;
    top->value.tf = f;
    incr_top;
   }
   break;

   case PUSHLOCAL0: case PUSHLOCAL1: case PUSHLOCAL2:
   case PUSHLOCAL3: case PUSHLOCAL4: case PUSHLOCAL5:
   case PUSHLOCAL6: case PUSHLOCAL7: case PUSHLOCAL8:
   case PUSHLOCAL9: 
     *top = *((stack+base) + (int)(opcode-PUSHLOCAL0)); incr_top; break;

   case PUSHLOCAL: *top = *((stack+base) + (*pc++)); incr_top; break;

   case PUSHGLOBAL:
   {
    Word w;
    get_word(w,pc);
    getglobal(w);
   }
   break;

   case PUSHINDEXED:
    pushsubscript();
    break;

   case PUSHSELF:
   {
     TObject receiver = *(top-1);
     Word w;
     get_word(w,pc);
     ttype(top) = LUA_T_STRING; tsvalue(top) = lua_constant[w];
     incr_top;
     pushsubscript();
     *top = receiver;
     incr_top;
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
    Word w;
    get_word(w,pc);
    setglobal(w);
   }
   break;

   case STOREINDEXED0:
    storesubscript(top-3, 1);
    break;

   case STOREINDEXED: {
     int n = *pc++;
     storesubscript(top-3-n, 2);
     break;
   }

   case STORELIST0:
   case STORELIST:
   {
    int m, n;
    TObject *arr;
    if (opcode == STORELIST0) m = 0;
    else m = *(pc++) * FIELDS_PER_FLUSH;
    n = *(pc++);
    arr = top-n-1;
    while (n)
    {
     ttype(top) = LUA_T_NUMBER; nvalue(top) = n+m;
     *(lua_hashdefine (avalue(arr), top)) = *(top-1);
     top--;
     n--;
    }
   }
   break;

   case STORERECORD:  /* opcode obsolete: supersed by STOREMAP */
   {
    int n = *(pc++);
    TObject *arr = top-n-1;
    while (n)
    {
     Word w;
     get_word(w,pc);
     ttype(top) = LUA_T_STRING; tsvalue(top) = lua_constant[w];
     *(lua_hashdefine (avalue(arr), top)) = *(top-1);
     top--;
     n--;
    }
   }
   break;

   case STOREMAP: {
     int n = *(pc++);
     TObject *arr = top-(2*n)-1;
     while (n--) {
       *(lua_hashdefine (avalue(arr), top-2)) = *(top-1);
       top-=2;
     }
   }
   break;

   case ADJUST0:
     adjust_top(base);
     break;

   case ADJUST: {
     StkId newtop = base + *(pc++);
     adjust_top(newtop);
     break;
   }

   case VARARGS:
     adjust_varargs(base + *(pc++));
     break;

   case CREATEARRAY:
   {
    Word size;
    get_word(size,pc);
    avalue(top) = lua_createarray(size);
    ttype(top) = LUA_T_ARRAY;
    incr_top;
   }
   break;

   case EQOP:
   {
    int res = lua_equalObj(top-2, top-1);
    --top;
    ttype(top-1) = res ? LUA_T_NUMBER : LUA_T_NIL;
    nvalue(top-1) = 1;
   }
   break;

    case LTOP:
      comparison(LUA_T_NUMBER, LUA_T_NIL, LUA_T_NIL, IM_LT);
      break;

   case LEOP:
      comparison(LUA_T_NUMBER, LUA_T_NUMBER, LUA_T_NIL, IM_LE);
      break;

   case GTOP:
      comparison(LUA_T_NIL, LUA_T_NIL, LUA_T_NUMBER, IM_GT);
      break;

   case GEOP:
      comparison(LUA_T_NIL, LUA_T_NUMBER, LUA_T_NUMBER, IM_GE);
      break;

   case ADDOP:
   {
    TObject *l = top-2;
    TObject *r = top-1;
    if (tonumber(r) || tonumber(l))
      call_arith(IM_ADD);
    else
    {
      nvalue(l) += nvalue(r);
      --top;
    }
   }
   break;

   case SUBOP:
   {
    TObject *l = top-2;
    TObject *r = top-1;
    if (tonumber(r) || tonumber(l))
      call_arith(IM_SUB);
    else
    {
      nvalue(l) -= nvalue(r);
      --top;
    }
   }
   break;

   case MULTOP:
   {
    TObject *l = top-2;
    TObject *r = top-1;
    if (tonumber(r) || tonumber(l))
      call_arith(IM_MUL);
    else
    {
      nvalue(l) *= nvalue(r);
      --top;
    }
   }
   break;

   case DIVOP:
   {
    TObject *l = top-2;
    TObject *r = top-1;
    if (tonumber(r) || tonumber(l))
      call_arith(IM_DIV);
    else
    {
      nvalue(l) /= nvalue(r);
      --top;
    }
   }
   break;

   case POWOP:
    call_arith(IM_POW);
    break;

   case CONCOP: {
     TObject *l = top-2;
     TObject *r = top-1;
     if (tostring(l) || tostring(r))
       call_binTM(IM_CONCAT, "unexpected type for concatenation");
     else {
       tsvalue(l) = lua_createstring(lua_strconc(svalue(l),svalue(r)));
       --top;
     }
   }
   break;

   case MINUSOP:
    if (tonumber(top-1))
    {
      ttype(top) = LUA_T_NIL;
      incr_top;
      call_arith(IM_UNM);
    }
    else
      nvalue(top-1) = - nvalue(top-1);
   break;

   case NOTOP:
    ttype(top-1) = (ttype(top-1) == LUA_T_NIL) ? LUA_T_NUMBER : LUA_T_NIL;
    nvalue(top-1) = 1;
   break;

   case ONTJMP:
   {
    Word w;
    get_word(w,pc);
    if (ttype(top-1) != LUA_T_NIL) pc += w;
   }
   break;

   case ONFJMP:
   {
    Word w;
    get_word(w,pc);
    if (ttype(top-1) == LUA_T_NIL) pc += w;
   }
   break;

   case JMP:
   {
    Word w;
    get_word(w,pc);
    pc += w;
   }
   break;

   case UPJMP:
   {
    Word w;
    get_word(w,pc);
    pc -= w;
   }
   break;

   case IFFJMP:
   {
    Word w;
    get_word(w,pc);
    top--;
    if (ttype(top) == LUA_T_NIL) pc += w;
   }
   break;

   case IFFUPJMP:
   {
    Word w;
    get_word(w,pc);
    top--;
    if (ttype(top) == LUA_T_NIL) pc -= w;
   }
   break;

   case POP: --top; break;

   case CALLFUNC:
   {
     int nParams = *(pc++);
     int nResults = *(pc++);
     StkId newBase = (top-stack)-nParams;
     do_call(newBase, nResults);
   }
   break;

   case RETCODE0:
   case RETCODE:
     if (lua_callhook)
       callHook (base, LUA_T_MARK, 1);
     return (base + ((opcode==RETCODE0) ? 0 : *pc));

   case SETLINE:
   {
    Word line;
    get_word(line,pc);
    if ((stack+base-1)->ttype != LUA_T_LINE)
    {
      /* open space for LINE value */
      open_stack((top-stack)-base);
      base++;
      (stack+base-1)->ttype = LUA_T_LINE;
    }
    (stack+base-1)->value.i = line;
    if (lua_linehook)
      lineHook (line);
    break;
   }

   default:
    lua_error ("internal error - opcode doesn't match");
  }
 }
}


#if COMPAT2_5
/*
** API: set a function as a fallback
*/
lua_Object lua_setfallback (char *name, lua_CFunction fallback)
{
  lua_pushstring(name);
  lua_pushcfunction(fallback);
  do_unprotectedrun(luaI_setfallback, 2, 1);
  return put_luaObjectonTop();
}
#endif
