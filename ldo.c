/*
** $Id: ldo.c,v 1.1 1997/09/16 19:25:59 roberto Exp roberto $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/


#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "lbuiltin.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lparser.h"
#include "ltm.h"
#include "lua.h"
#include "luadebug.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"



#ifndef STACK_LIMIT
#define STACK_LIMIT     6000
#endif


static TObject initial_stack;

struct Stack luaD_stack = {&initial_stack+1, &initial_stack, &initial_stack};


struct C_Lua_Stack luaD_Cstack = {0, 0, 0};

static  jmp_buf *errorJmp = NULL; /* current error recover point */





#define STACK_EXTRA 	32

static void initstack (int n)
{
  int maxstack = STACK_EXTRA+n;
  luaD_stack.stack = luaM_newvector(maxstack, TObject);
  luaD_stack.last = luaD_stack.stack+(maxstack-1);
  luaD_stack.top = luaD_stack.stack;
  *(luaD_stack.top++) = initial_stack;
  luaB_predefine();
}


void luaD_checkstack (int n)
{
  if (luaD_stack.stack == &initial_stack)
    initstack(n);
  else if (luaD_stack.last-luaD_stack.top <= n) {
    static int limit = STACK_LIMIT;
    StkId top = luaD_stack.top-luaD_stack.stack;
    int stacksize = (luaD_stack.last-luaD_stack.stack)+1+STACK_EXTRA+n;
    luaD_stack.stack = luaM_reallocvector(luaD_stack.stack, stacksize,TObject);
    luaD_stack.last = luaD_stack.stack+(stacksize-1);
    luaD_stack.top = luaD_stack.stack + top;
    if (stacksize >= limit) {
      limit = stacksize+STACK_EXTRA;  /* extra space to run error handler */
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
  int diff = newtop-(luaD_stack.top-luaD_stack.stack);
  if (diff <= 0)
    luaD_stack.top += diff;
  else {
    luaD_checkstack(diff);
    while (diff--)
      ttype(luaD_stack.top++) = LUA_T_NIL;
  }
}


/*
** Open a hole below "nelems" from the luaD_stack.top.
*/
void luaD_openstack (int nelems)
{
  int i;
  for (i=0; i<nelems; i++)
    *(luaD_stack.top-i) = *(luaD_stack.top-i-1);
  incr_top;
}


void luaD_lineHook (int line)
{
  struct C_Lua_Stack oldCLS = luaD_Cstack;
  StkId old_top = luaD_Cstack.lua2C = luaD_Cstack.base = luaD_stack.top-luaD_stack.stack;
  luaD_Cstack.num = 0;
  (*lua_linehook)(line);
  luaD_stack.top = luaD_stack.stack+old_top;
  luaD_Cstack = oldCLS;
}


void luaD_callHook (StkId base, lua_Type type, int isreturn)
{
  struct C_Lua_Stack oldCLS = luaD_Cstack;
  StkId old_top = luaD_Cstack.lua2C = luaD_Cstack.base = luaD_stack.top-luaD_stack.stack;
  luaD_Cstack.num = 0;
  if (isreturn)
    (*lua_callhook)(LUA_NOOBJECT, "(return)", 0);
  else {
    TObject *f = luaD_stack.stack+base-1;
    if (type == LUA_T_MARK)
      (*lua_callhook)(Ref(f), f->value.tf->fileName->str,
                      f->value.tf->lineDefined);
    else
      (*lua_callhook)(Ref(f), "(C)", -1);
  }
  luaD_stack.top = luaD_stack.stack+old_top;
  luaD_Cstack = oldCLS;
}


/*
** Call a C function. luaD_Cstack.base will point to the luaD_stack.top of the luaD_stack.stack,
** and luaD_Cstack.num is the number of parameters. Returns an index
** to the first result from C.
*/
static StkId callC (lua_CFunction func, StkId base)
{
  struct C_Lua_Stack oldCLS = luaD_Cstack;
  StkId firstResult;
  luaD_Cstack.num = (luaD_stack.top-luaD_stack.stack) - base;
  /* incorporate parameters on the luaD_stack.stack */
  luaD_Cstack.lua2C = base;
  luaD_Cstack.base = base+luaD_Cstack.num;  /* == luaD_stack.top-luaD_stack.stack */
  if (lua_callhook)
    luaD_callHook(base, LUA_T_CMARK, 0);
  (*func)();
  if (lua_callhook)  /* func may have changed lua_callhook */
    luaD_callHook(base, LUA_T_CMARK, 1);
  firstResult = luaD_Cstack.base;
  luaD_Cstack = oldCLS;
  return firstResult;
}


void luaD_callTM (TObject *f, int nParams, int nResults)
{
  luaD_openstack(nParams);
  *(luaD_stack.top-nParams-1) = *f;
  luaD_call((luaD_stack.top-luaD_stack.stack)-nParams, nResults);
}


/*
** Call a function (C or Lua). The parameters must be on the luaD_stack.stack,
** between [luaD_stack.stack+base,luaD_stack.top). The function to be called is at luaD_stack.stack+base-1.
** When returns, the results are on the luaD_stack.stack, between [luaD_stack.stack+base-1,luaD_stack.top).
** The number of results is nResults, unless nResults=MULT_RET.
*/
void luaD_call (StkId base, int nResults)
{
  StkId firstResult;
  TObject *func = luaD_stack.stack+base-1;
  int i;
  if (ttype(func) == LUA_T_CFUNCTION) {
    ttype(func) = LUA_T_CMARK;
    firstResult = callC(fvalue(func), base);
  }
  else if (ttype(func) == LUA_T_FUNCTION) {
    ttype(func) = LUA_T_MARK;
    firstResult = luaV_execute(func->value.cl, base);
  }
  else { /* func is not a function */
    /* Check the tag method for invalid functions */
    TObject *im = luaT_getimbyObj(func, IM_FUNCTION);
    if (ttype(im) == LUA_T_NIL)
      lua_error("call expression not a function");
    luaD_callTM(im, (luaD_stack.top-luaD_stack.stack)-(base-1), nResults);
    return;
  }
  /* adjust the number of results */
  if (nResults != MULT_RET)
    luaD_adjusttop(firstResult+nResults);
  /* move results to base-1 (to erase parameters and function) */
  base--;
  nResults = luaD_stack.top - (luaD_stack.stack+firstResult);  /* actual number of results */
  for (i=0; i<nResults; i++)
    *(luaD_stack.stack+base+i) = *(luaD_stack.stack+firstResult+i);
  luaD_stack.top -= firstResult-base;
}



/*
** Traverse all objects on luaD_stack.stack
*/
void luaD_travstack (int (*fn)(TObject *))
{
  StkId i;
  for (i = (luaD_stack.top-1)-luaD_stack.stack; i>=0; i--)
    fn (luaD_stack.stack+i);
}


/*
** Error messages
*/

static void auxerrorim (char *form)
{
  lua_Object s = lua_getparam(1);
  if (lua_isstring(s))
    fprintf(stderr, form, lua_getstring(s));
}


static void emergencyerrorf (void)
{
  auxerrorim("THERE WAS AN ERROR INSIDE AN ERROR METHOD:\n%s\n");
}


static void stderrorim (void)
{
  auxerrorim("lua: %s\n");
}


TObject luaD_errorim = {LUA_T_CFUNCTION, {stderrorim}};


static void message (char *s)
{
  TObject im = luaD_errorim;
  if (ttype(&im) != LUA_T_NIL) {
    luaD_errorim.ttype = LUA_T_CFUNCTION;
    luaD_errorim.value.f = emergencyerrorf;
    lua_pushstring(s);
    luaD_callTM(&im, 1, 0);
    luaD_errorim = im;
  }
}

/*
** Reports an error, and jumps up to the available recover label
*/
void lua_error (char *s)
{
  if (s) message(s);
  if (errorJmp)
    longjmp(*errorJmp, 1);
  else {
    fprintf (stderr, "lua: exit(1). Unable to recover\n");
    exit(1);
  }
}

/*
** Call the function at luaD_Cstack.base, and incorporate results on
** the Lua2C structure.
*/
static void do_callinc (int nResults)
{
  StkId base = luaD_Cstack.base;
  luaD_call(base+1, nResults);
  luaD_Cstack.lua2C = base;  /* position of the luaM_new results */
  luaD_Cstack.num = (luaD_stack.top-luaD_stack.stack) - base;  /* number of results */
  luaD_Cstack.base = base + luaD_Cstack.num;  /* incorporate results on luaD_stack.stack */
}


/*
** Execute a protected call. Assumes that function is at luaD_Cstack.base and
** parameters are on luaD_stack.top of it. Leave nResults on the luaD_stack.stack.
*/
int luaD_protectedrun (int nResults)
{
  jmp_buf myErrorJmp;
  int status;
  struct C_Lua_Stack oldCLS = luaD_Cstack;
  jmp_buf *oldErr = errorJmp;
  errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp) == 0) {
    do_callinc(nResults);
    status = 0;
  }
  else { /* an error occurred: restore luaD_Cstack and luaD_stack.top */
    luaD_Cstack = oldCLS;
    luaD_stack.top = luaD_stack.stack+luaD_Cstack.base;
    status = 1;
  }
  errorJmp = oldErr;
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
  jmp_buf *oldErr = errorJmp;
  errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp) == 0) {
    tf = bin ? luaU_undump1(z, chunkname) : luaY_parser(z, chunkname);
    status = 0;
  }
  else {
    tf = NULL;
    status = 1;
  }
  errorJmp = oldErr;
  if (status) return 1;  /* error code */
  if (tf == NULL) return 2;  /* 'natural' end */
  luaD_adjusttop(luaD_Cstack.base+1);  /* one slot for the pseudo-function */
  luaD_stack.stack[luaD_Cstack.base].ttype = LUA_T_PROTO;
  luaD_stack.stack[luaD_Cstack.base].value.tf = tf;
  luaV_closure();
  return 0;
}


static int do_main (ZIO *z, char *chunkname, int bin)
{
  int status;
  do {
    long old_entities = (luaC_checkGC(), luaO_nentities);
    status = protectedparser(z, chunkname, bin);
    if (status == 1) return 1;  /* error */
    else if (status == 2) return 0;  /* 'natural' end */
    else {
      long newelems2 = 2*(luaO_nentities-old_entities);
      luaC_threshold += newelems2;
      status = luaD_protectedrun(MULT_RET);
      luaC_threshold -= newelems2;
    }
  } while (bin && status == 0);
  return status;
}


void luaD_gcIM (TObject *o)
{
  TObject *im = luaT_getimbyObj(o, IM_GC);
  if (ttype(im) != LUA_T_NIL) {
    *luaD_stack.top = *o;
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

