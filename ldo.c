/*
** $Id: ldo.c,v 1.8 1997/11/07 15:09:49 roberto Exp roberto $
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
  lua_Object s = lua_getparam(1);
  if (lua_isstring(s))
    fprintf(stderr, "lua error: %s\n", lua_getstring(s));
}


static void initCfunc (TObject *o, lua_CFunction f)
{
  ttype(o) = LUA_T_CPROTO;
  fvalue(o) = f;
  luaF_simpleclosure(o);
}


#define STACK_EXTRA 	32
#define INIT_STACK_SIZE	32


void luaD_init (void)
{
  L->stacklimit = STACK_LIMIT;
  L->stack.stack = luaM_newvector(INIT_STACK_SIZE, TObject);
  L->stack.top = L->stack.stack;
  L->stack.last = L->stack.stack+(INIT_STACK_SIZE-1);
  initCfunc(&L->errorim, stderrorim);
}


void luaD_checkstack (int n)
{
  if (L->stack.last-L->stack.top <= n) {
    StkId top = L->stack.top-L->stack.stack;
    int stacksize = (L->stack.last-L->stack.stack)+1+STACK_EXTRA+n;
    L->stack.stack = luaM_reallocvector(L->stack.stack, stacksize,TObject);
    L->stack.last = L->stack.stack+(stacksize-1);
    L->stack.top = L->stack.stack + top;
    if (stacksize >= L->stacklimit) {
      /* extra space to run error handler */
      L->stacklimit = stacksize+STACK_EXTRA;
      if (lua_stackedfunction(100) == LUA_NOOBJECT) {
        /* less than 100 functions on the stack: cannot be recursive loop */
        lua_error("Lua2C - C2Lua overflow");
      }
      else
        lua_error(stackEM);
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
  int i;
  for (i=0; i<nelems; i++)
    *(L->stack.top-i) = *(L->stack.top-i-1);
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


void luaD_callHook (StkId base, lua_Type type, int isreturn)
{
  struct C_Lua_Stack oldCLS = L->Cstack;
  StkId old_top = L->Cstack.lua2C = L->Cstack.base = L->stack.top-L->stack.stack;
  L->Cstack.num = 0;
  if (isreturn)
    (*lua_callhook)(LUA_NOOBJECT, "(return)", 0);
  else {
    TObject *f = L->stack.stack+base-1;
    if (type == LUA_T_PROTO)
      (*lua_callhook)(Ref(f), tfvalue(protovalue(f))->fileName->str,
                              tfvalue(protovalue(f))->lineDefined);
    else
      (*lua_callhook)(Ref(f), "(C)", -1);
  }
  L->stack.top = L->stack.stack+old_top;
  L->Cstack = oldCLS;
}


/*
** Call a C function. L->Cstack.base will point to the top of the stack,
** and L->Cstack.num is the number of parameters. Returns an index
** to the first result from C.
*/
static StkId callC (lua_CFunction func, StkId base)
{
  struct C_Lua_Stack oldCLS = L->Cstack;
  StkId firstResult;
  L->Cstack.num = (L->stack.top-L->stack.stack) - base;
  /* incorporate parameters on the L->stack.stack */
  L->Cstack.lua2C = base;
  L->Cstack.base = base+L->Cstack.num;  /* == top-stack */
  if (lua_callhook)
    luaD_callHook(base, LUA_T_CPROTO, 0);
  (*func)();
  if (lua_callhook)  /* func may have changed lua_callhook */
    luaD_callHook(base, LUA_T_CPROTO, 1);
  firstResult = L->Cstack.base;
  L->Cstack = oldCLS;
  return firstResult;
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
  if (ttype(func) == LUA_T_FUNCTION) {
    TObject *proto = protovalue(func);
    ttype(func) = LUA_T_MARK;
    firstResult = (ttype(proto) == LUA_T_CPROTO) ? callC(fvalue(proto), base)
                                         : luaV_execute(func->value.cl, base);
  }
  else { /* func is not a function */
    /* Check the tag method for invalid functions */
    TObject *im = luaT_getimbyObj(func, IM_FUNCTION);
    if (ttype(im) == LUA_T_NIL)
      lua_error("call expression not a function");
    luaD_callTM(im, (L->stack.top-L->stack.stack)-(base-1), nResults);
    return;
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
    fn (L->stack.stack+i);
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
  struct C_Lua_Stack oldCLS = L->Cstack;
  jmp_buf *oldErr = L->errorJmp;
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
static int protectedparser (ZIO *z, char *chunkname, int bin)
{
  int status;
  TProtoFunc *tf;
  jmp_buf myErrorJmp;
  jmp_buf *oldErr = L->errorJmp;
  L->errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp) == 0) {
    tf = bin ? luaU_undump1(z, chunkname) : luaY_parser(z, chunkname);
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


static int do_main (ZIO *z, char *chunkname, int bin)
{
  int status;
  do {
    long old_blocks = (luaC_checkGC(), L->nblocks);
    status = protectedparser(z, chunkname, bin);
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
  luaZ_Fopen(&z, f);
  status = do_main(&z, filename, bin);
  if (f != stdin)
    fclose(f);
  return status;
}


#define SIZE_PREF 20  /* size of string prefix to appear in error messages */


int lua_dostring (char *str)
{
  int status;
  char buff[SIZE_PREF+25];
  char *temp;
  ZIO z;
  if (str == NULL) return 1;
  sprintf(buff, "(dostring) >> %.20s", str);
  temp = strchr(buff, '\n');
  if (temp) *temp = 0;  /* end string after first line */
  luaZ_sopen(&z, str);
  status = do_main(&z, buff, 0);
  return status;
}

