/*
** $Id: ldo.c,v 1.27 1998/06/19 18:47:06 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "ltm.h"
#include "lua.h"
#include "luadebug.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"



#ifndef STACK_LIMIT
#define STACK_LIMIT     6000
#endif



/*
** Error messages
*/

static void stderrorim (void)
{
  fprintf(stderr, "lua error: %s\n", lua_getstring(lua_getparam(1)));
}



#define STACK_UNIT	128


void luaD_init (void)
{
  L->stack.stack = luaM_newvector(STACK_UNIT, TObject);
  L->stack.top = L->stack.stack;
  L->stack.last = L->stack.stack+(STACK_UNIT-1);
  ttype(&L->errorim) = LUA_T_CPROTO;
  fvalue(&L->errorim) = stderrorim;
}


void luaD_checkstack (int n)
{
  struct Stack *S = &L->stack;
  if (S->last-S->top <= n) {
    StkId top = S->top-S->stack;
    int stacksize = (S->last-S->stack)+1+STACK_UNIT+n;
    S->stack = luaM_reallocvector(S->stack, stacksize, TObject);
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
void luaD_adjusttop (StkId newtop)
{
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
void luaD_openstack (int nelems)
{
  luaO_memup(L->stack.top-nelems+1, L->stack.top-nelems,
             nelems*sizeof(TObject));
  incr_top;
}


void luaD_lineHook (int line)
{
  struct C_Lua_Stack oldCLS = L->Cstack;
  StkId old_top = L->Cstack.lua2C = L->Cstack.base = L->stack.top-L->stack.stack;
  L->Cstack.num = 0;
  (*lua_linehook)(line);
  L->stack.top = L->stack.stack+old_top;
  L->Cstack = oldCLS;
}


void luaD_callHook (StkId base, TProtoFunc *tf, int isreturn)
{
  struct C_Lua_Stack oldCLS = L->Cstack;
  StkId old_top = L->Cstack.lua2C = L->Cstack.base = L->stack.top-L->stack.stack;
  L->Cstack.num = 0;
  if (isreturn)
    (*lua_callhook)(LUA_NOOBJECT, "(return)", 0);
  else {
    TObject *f = L->stack.stack+base-1;
    if (tf)
      (*lua_callhook)(Ref(f), tf->fileName->str, tf->lineDefined);
    else
      (*lua_callhook)(Ref(f), "(C)", -1);
  }
  L->stack.top = L->stack.stack+old_top;
  L->Cstack = oldCLS;
}


/*
** Call a C function.
** Cstack.num is the number of arguments; Cstack.lua2C points to the
** first argument. Returns an index to the first result from C.
*/
static StkId callC (lua_CFunction f, StkId base)
{
  struct C_Lua_Stack *CS = &L->Cstack;
  struct C_Lua_Stack oldCLS = *CS;
  StkId firstResult;
  int numarg = (L->stack.top-L->stack.stack) - base;
  CS->num = numarg;
  CS->lua2C = base;
  CS->base = base+numarg;  /* == top-stack */
  if (lua_callhook)
    luaD_callHook(base, NULL, 0);
  (*f)();  /* do the actual call */
  if (lua_callhook)  /* func may have changed lua_callhook */
    luaD_callHook(base, NULL, 1);
  firstResult = CS->base;
  *CS = oldCLS;
  return firstResult;
}


static StkId callCclosure (struct Closure *cl, lua_CFunction f, StkId base)
{
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


void luaD_callTM (TObject *f, int nParams, int nResults)
{
  luaD_openstack(nParams);
  *(L->stack.top-nParams-1) = *f;
  luaD_call((L->stack.top-L->stack.stack)-nParams, nResults);
}


/*
** Call a function (C or Lua). The parameters must be on the L->stack.stack,
** between [L->stack.stack+base,L->stack.top). The function to be called is at L->stack.stack+base-1.
** When returns, the results are on the L->stack.stack, between [L->stack.stack+base-1,L->stack.top).
** The number of results is nResults, unless nResults=MULT_RET.
*/
void luaD_call (StkId base, int nResults)
{
  StkId firstResult;
  TObject *func = L->stack.stack+base-1;
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
      luaD_callTM(im, (L->stack.top-L->stack.stack)-(base-1), nResults);
      return;
    }
  }
  /* adjust the number of results */
  if (nResults != MULT_RET)
    luaD_adjusttop(firstResult+nResults);
  /* move results to base-1 (to erase parameters and function) */
  base--;
  nResults = L->stack.top - (L->stack.stack+firstResult);  /* actual number of results */
  for (i=0; i<nResults; i++)
    *(L->stack.stack+base+i) = *(L->stack.stack+firstResult+i);
  L->stack.top -= firstResult-base;
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



static void message (char *s)
{
  TObject im = L->errorim;
  if (ttype(&im) != LUA_T_NIL) {
    lua_pushstring(s);
    luaD_callTM(&im, 1, 0);
  }
}

/*
** Reports an error, and jumps up to the available recover label
*/
void lua_error (char *s)
{
  if (s) message(s);
  if (L->errorJmp)
    longjmp(*((jmp_buf *)L->errorJmp), 1);
  else {
    fprintf (stderr, "lua: exit(1). Unable to recover\n");
    exit(1);
  }
}

/*
** Call the function at L->Cstack.base, and incorporate results on
** the Lua2C structure.
*/
static void do_callinc (int nResults)
{
  StkId base = L->Cstack.base;
  luaD_call(base+1, nResults);
  L->Cstack.lua2C = base;  /* position of the luaM_new results */
  L->Cstack.num = (L->stack.top-L->stack.stack) - base;  /* number of results */
  L->Cstack.base = base + L->Cstack.num;  /* incorporate results on L->stack.stack */
}


/*
** Execute a protected call. Assumes that function is at L->Cstack.base and
** parameters are on top of it. Leave nResults on the stack.
*/
int luaD_protectedrun (int nResults)
{
  jmp_buf myErrorJmp;
  int status;
  volatile struct C_Lua_Stack oldCLS = L->Cstack;
  jmp_buf *volatile oldErr = L->errorJmp;
  L->errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp) == 0) {
    do_callinc(nResults);
    status = 0;
  }
  else { /* an error occurred: restore L->Cstack and L->stack.top */
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
static int protectedparser (ZIO *z, int bin)
{
  volatile int status;
  TProtoFunc *volatile tf;
  jmp_buf myErrorJmp;
  jmp_buf *volatile oldErr = L->errorJmp;
  L->errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp) == 0) {
    tf = bin ? luaU_undump1(z) : luaY_parser(z);
    status = 0;
  }
  else {
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


static int do_main (ZIO *z, int bin)
{
  int status;
  do {
    long old_blocks = (luaC_checkGC(), L->nblocks);
    status = protectedparser(z, bin);
    if (status == 1) return 1;  /* error */
    else if (status == 2) return 0;  /* 'natural' end */
    else {
      unsigned long newelems2 = 2*(L->nblocks-old_blocks);
      L->GCthreshold += newelems2;
      status = luaD_protectedrun(MULT_RET);
      L->GCthreshold -= newelems2;
    }
  } while (bin && status == 0);
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


int lua_dofile (char *filename)
{
  ZIO z;
  int status;
  int c;
  int bin;
  FILE *f = (filename == NULL) ? stdin : fopen(filename, "r");
  if (f == NULL)
    return 2;
  if (filename == NULL)
    filename = "(stdin)";
  c = fgetc(f);
  ungetc(c, f);
  bin = (c == ID_CHUNK);
  if (bin)
    f = freopen(filename, "rb", f);  /* set binary mode */
  luaZ_Fopen(&z, f, filename);
  status = do_main(&z, bin);
  if (f != stdin)
    fclose(f);
  return status;
}


#define SIZE_PREF 20  /* size of string prefix to appear in error messages */
#define SSIZE_PREF "20"


static void build_name (char *str, char *name) {
  if (str == NULL || *str == ID_CHUNK)
    strcpy(name, "(buffer)");
  else {
    char *temp;
    sprintf(name, "(dostring) >> \"%." SSIZE_PREF "s\"", str);
    temp = strchr(name, '\n');
    if (temp) {  /* end string after first line */
     *temp = '"';
     *(temp+1) = 0;
    }
  }
}


int lua_dostring (char *str) {
  return lua_dobuffer(str, strlen(str), NULL);
}


int lua_dobuffer (char *buff, int size, char *name) {
  char newname[SIZE_PREF+25];
  ZIO z;
  int status;
  if (name==NULL) {
    build_name(buff, newname);
    name = newname;
  }
  luaZ_mopen(&z, buff, size, name);
  status = do_main(&z, buff[0]==ID_CHUNK);
  return status;
}

