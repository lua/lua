/*
** $Id: ldo.c,v 1.45b 1999/06/22 20:37:23 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltm.h"
#include "lua.h"
#include "luadebug.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"



#ifndef STACK_LIMIT
#define STACK_LIMIT     6000  /* arbitrary limit */
#endif



#define STACK_UNIT	128


#ifdef DEBUG
#undef STACK_UNIT
#define STACK_UNIT	2
#endif


void luaD_init (void) {
  L->stack.stack = luaM_newvector(STACK_UNIT, TObject);
  L->stack.top = L->stack.stack;
  L->stack.last = L->stack.stack+(STACK_UNIT-1);
}


void luaD_checkstack (int n) {
  struct Stack *S = &L->stack;
  if (S->last-S->top <= n) {
    StkId top = S->top-S->stack;
    int stacksize = (S->last-S->stack)+STACK_UNIT+n;
    luaM_reallocvector(S->stack, stacksize, TObject);
    S->last = S->stack+(stacksize-1);
    S->top = S->stack + top;
    if (stacksize >= STACK_LIMIT) {  /* stack overflow? */
      if (lua_stackedfunction(100) == LUA_NOOBJECT)  /* 100 funcs on stack? */
        lua_error("Lua2C - C2Lua overflow"); /* doesn't look like a rec. loop */
      else
        lua_error("stack size overflow");
    }
  }
}


/*
** Adjust stack. Set top to the given value, pushing NILs if needed.
*/
void luaD_adjusttop (StkId newtop) {
  int diff = newtop-(L->stack.top-L->stack.stack);
  if (diff <= 0)
    L->stack.top += diff;
  else {
    luaD_checkstack(diff);
    while (diff--)
      ttype(L->stack.top++) = LUA_T_NIL;
  }
}


/*
** Open a hole below "nelems" from the L->stack.top.
*/
void luaD_openstack (int nelems) {
  luaO_memup(L->stack.top-nelems+1, L->stack.top-nelems,
             nelems*sizeof(TObject));
  incr_top;
}


void luaD_lineHook (int line) {
  struct C_Lua_Stack oldCLS = L->Cstack;
  StkId old_top = L->Cstack.lua2C = L->Cstack.base = L->stack.top-L->stack.stack;
  L->Cstack.num = 0;
  (*L->linehook)(line);
  L->stack.top = L->stack.stack+old_top;
  L->Cstack = oldCLS;
}


void luaD_callHook (StkId base, TProtoFunc *tf, int isreturn) {
  struct C_Lua_Stack oldCLS = L->Cstack;
  StkId old_top = L->Cstack.lua2C = L->Cstack.base = L->stack.top-L->stack.stack;
  L->Cstack.num = 0;
  if (isreturn)
    (*L->callhook)(LUA_NOOBJECT, "(return)", 0);
  else {
    TObject *f = L->stack.stack+base-1;
    if (tf)
      (*L->callhook)(Ref(f), tf->source->str, tf->lineDefined);
    else
      (*L->callhook)(Ref(f), "(C)", -1);
  }
  L->stack.top = L->stack.stack+old_top;
  L->Cstack = oldCLS;
}


/*
** Call a C function.
** Cstack.num is the number of arguments; Cstack.lua2C points to the
** first argument. Returns an index to the first result from C.
*/
static StkId callC (lua_CFunction f, StkId base) {
  struct C_Lua_Stack *cls = &L->Cstack;
  struct C_Lua_Stack oldCLS = *cls;
  StkId firstResult;
  int numarg = (L->stack.top-L->stack.stack) - base;
  cls->num = numarg;
  cls->lua2C = base;
  cls->base = base+numarg;  /* == top-stack */
  if (L->callhook)
    luaD_callHook(base, NULL, 0);
  (*f)();  /* do the actual call */
  if (L->callhook)  /* func may have changed callhook */
    luaD_callHook(base, NULL, 1);
  firstResult = cls->base;
  *cls = oldCLS;
  return firstResult;
}


static StkId callCclosure (struct Closure *cl, lua_CFunction f, StkId base) {
  TObject *pbase;
  int nup = cl->nelems;  /* number of upvalues */
  luaD_checkstack(nup);
  pbase = L->stack.stack+base;  /* care: previous call may change this */
  /* open space for upvalues as extra arguments */
  luaO_memup(pbase+nup, pbase, (L->stack.top-pbase)*sizeof(TObject));
  /* copy upvalues into stack */
  memcpy(pbase, cl->consts+1, nup*sizeof(TObject));
  L->stack.top += nup;
  return callC(f, base);
}


void luaD_callTM (TObject *f, int nParams, int nResults) {
  luaD_openstack(nParams);
  *(L->stack.top-nParams-1) = *f;
  luaD_calln(nParams, nResults);
}


/*
** Call a function (C or Lua). The parameters must be on the stack,
** between [top-nArgs,top). The function to be called is right below the
** arguments.
** When returns, the results are on the stack, between [top-nArgs-1,top).
** The number of results is nResults, unless nResults=MULT_RET.
*/
void luaD_calln (int nArgs, int nResults) {
  struct Stack *S = &L->stack;  /* to optimize */
  StkId base = (S->top-S->stack)-nArgs;
  TObject *func = S->stack+base-1;
  StkId firstResult;
  int i;
  switch (ttype(func)) {
    case LUA_T_CPROTO:
      ttype(func) = LUA_T_CMARK;
      firstResult = callC(fvalue(func), base);
      break;
    case LUA_T_PROTO:
      ttype(func) = LUA_T_PMARK;
      firstResult = luaV_execute(NULL, tfvalue(func), base);
      break;
    case LUA_T_CLOSURE: {
      Closure *c = clvalue(func);
      TObject *proto = &(c->consts[0]);
      ttype(func) = LUA_T_CLMARK;
      firstResult = (ttype(proto) == LUA_T_CPROTO) ?
                       callCclosure(c, fvalue(proto), base) :
                       luaV_execute(c, tfvalue(proto), base);
      break;
    }
    default: { /* func is not a function */
      /* Check the tag method for invalid functions */
      TObject *im = luaT_getimbyObj(func, IM_FUNCTION);
      if (ttype(im) == LUA_T_NIL)
        lua_error("call expression not a function");
      luaD_callTM(im, (S->top-S->stack)-(base-1), nResults);
      return;
    }
  }
  /* adjust the number of results */
  if (nResults == MULT_RET)
    nResults = (S->top-S->stack)-firstResult;
  else
    luaD_adjusttop(firstResult+nResults);
  /* move results to base-1 (to erase parameters and function) */
  base--;
  for (i=0; i<nResults; i++)
    *(S->stack+base+i) = *(S->stack+firstResult+i);
  S->top -= firstResult-base;
}


/*
** Traverse all objects on L->stack.stack
*/
void luaD_travstack (int (*fn)(TObject *))
{
  StkId i;
  for (i = (L->stack.top-1)-L->stack.stack; i>=0; i--)
    fn(L->stack.stack+i);
}



static void message (char *s) {
  TObject *em = &(luaS_new("_ERRORMESSAGE")->u.s.globalval);
  if (ttype(em) == LUA_T_PROTO || ttype(em) == LUA_T_CPROTO ||
      ttype(em) == LUA_T_CLOSURE) {
    *L->stack.top = *em;
    incr_top;
    lua_pushstring(s);
    luaD_calln(1, 0);
  }
}

/*
** Reports an error, and jumps up to the available recover label
*/
void lua_error (char *s) {
  if (s) message(s);
  if (L->errorJmp)
    longjmp(L->errorJmp->b, 1);
  else {
    message("exit(1). Unable to recover.\n");
    exit(1);
  }
}


/*
** Execute a protected call. Assumes that function is at L->Cstack.base and
** parameters are on top of it. Leave nResults on the stack.
*/
int luaD_protectedrun (void) {
  volatile struct C_Lua_Stack oldCLS = L->Cstack;
  struct lua_longjmp myErrorJmp;
  volatile int status;
  struct lua_longjmp *volatile oldErr = L->errorJmp;
  L->errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp.b) == 0) {
    StkId base = L->Cstack.base;
    luaD_calln((L->stack.top-L->stack.stack)-base-1, MULT_RET);
    L->Cstack.lua2C = base;  /* position of the new results */
    L->Cstack.num = (L->stack.top-L->stack.stack) - base;
    L->Cstack.base = base + L->Cstack.num;  /* incorporate results on stack */
    status = 0;
  }
  else {  /* an error occurred: restore L->Cstack and L->stack.top */
    L->Cstack = oldCLS;
    L->stack.top = L->stack.stack+L->Cstack.base;
    status = 1;
  }
  L->errorJmp = oldErr;
  return status;
}


/*
** returns 0 = chunk loaded; 1 = error; 2 = no more chunks to load
*/
static int protectedparser (ZIO *z, int bin) {
  volatile struct C_Lua_Stack oldCLS = L->Cstack;
  struct lua_longjmp myErrorJmp;
  volatile int status;
  TProtoFunc *volatile tf;
  struct lua_longjmp *volatile oldErr = L->errorJmp;
  L->errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp.b) == 0) {
    tf = bin ? luaU_undump1(z) : luaY_parser(z);
    status = 0;
  }
  else {  /* an error occurred: restore L->Cstack and L->stack.top */
    L->Cstack = oldCLS;
    L->stack.top = L->stack.stack+L->Cstack.base;
    tf = NULL;
    status = 1;
  }
  L->errorJmp = oldErr;
  if (status) return 1;  /* error code */
  if (tf == NULL) return 2;  /* 'natural' end */
  luaD_adjusttop(L->Cstack.base+1);  /* one slot for the pseudo-function */
  L->stack.stack[L->Cstack.base].ttype = LUA_T_PROTO;
  L->stack.stack[L->Cstack.base].value.tf = tf;
  luaV_closure(0);
  return 0;
}


static int do_main (ZIO *z, int bin) {
  int status;
  int debug = L->debug;  /* save debug status */
  do {
    long old_blocks = (luaC_checkGC(), L->nblocks);
    status = protectedparser(z, bin);
    if (status == 1) return 1;  /* error */
    else if (status == 2) return 0;  /* 'natural' end */
    else {
      unsigned long newelems2 = 2*(L->nblocks-old_blocks);
      L->GCthreshold += newelems2;
      status = luaD_protectedrun();
      L->GCthreshold -= newelems2;
    }
  } while (bin && status == 0);
  L->debug = debug;  /* restore debug status */
  return status;
}


void luaD_gcIM (TObject *o)
{
  TObject *im = luaT_getimbyObj(o, IM_GC);
  if (ttype(im) != LUA_T_NIL) {
    *L->stack.top = *o;
    incr_top;
    luaD_callTM(im, 1, 0);
  }
}


#define	MAXFILENAME	260	/* maximum part of a file name kept */

int lua_dofile (char *filename) {
  ZIO z;
  int status;
  int c;
  int bin;
  char source[MAXFILENAME];
  FILE *f = (filename == NULL) ? stdin : fopen(filename, "r");
  if (f == NULL)
    return 2;
  luaL_filesource(source, filename, sizeof(source));
  c = fgetc(f);
  ungetc(c, f);
  bin = (c == ID_CHUNK);
  if (bin && f != stdin) {
    f = freopen(filename, "rb", f);  /* set binary mode */
    if (f == NULL) return 2;
  }
  luaZ_Fopen(&z, f, source);
  status = do_main(&z, bin);
  if (f != stdin)
    fclose(f);
  return status;
}


int lua_dostring (char *str) {
  return lua_dobuffer(str, strlen(str), str);
}


int lua_dobuffer (char *buff, int size, char *name) {
  ZIO z;
  if (!name) name = "?";
  luaZ_mopen(&z, buff, size, name);
  return do_main(&z, buff[0]==ID_CHUNK);
}

