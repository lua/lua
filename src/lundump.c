/*
** $Id: lundump.c,v 1.53 2004/09/01 21:22:34 lhf Exp $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#include <stdarg.h>
#include <stddef.h>

#define lundump_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"
#include "lzio.h"

#define	LoadByte	(lu_byte) ezgetc

typedef struct {
 lua_State* L;
 ZIO* Z;
 Mbuffer* b;
 int swap;
 const char* name;
} LoadState;

static void error (LoadState* S, const char* fmt, ...)
{
 const char *msg;
 va_list argp;
 va_start(argp,fmt);
 msg=luaO_pushvfstring(S->L,fmt,argp);
 va_end(argp);
 luaO_pushfstring(S->L,"%s: %s",S->name,msg);
 luaD_throw(S->L,LUA_ERRSYNTAX);
}

static int ezgetc (LoadState* S)
{
 int c=zgetc(S->Z);
 if (c==EOZ) error(S,"unexpected end of file");
 return c;
}

static void ezread (LoadState* S, void* b, int n)
{
 int r=luaZ_read(S->Z,b,n);
 if (r!=0) error(S,"unexpected end of file");
}

static void LoadBlock (LoadState* S, void* b, size_t size)
{
 if (S->swap)
 {
  char* p=(char*) b+size-1;
  int n=size;
  while (n--) *p--=(char)ezgetc(S);
 }
 else
  ezread(S,b,size);
}

static void LoadVector (LoadState* S, void* b, int m, size_t size)
{
 if (S->swap)
 {
  char* q=(char*) b;
  while (m--)
  {
   char* p=q+size-1;
   int n=size;
   while (n--) *p--=(char)ezgetc(S);
   q+=size;
  }
 }
 else
  ezread(S,b,m*size);
}

static int LoadInt (LoadState* S)
{
 int x;
 LoadBlock(S,&x,sizeof(x));
 if (x<0) error(S,"bad integer");
 return x;
}

static size_t LoadSize (LoadState* S)
{
 size_t x;
 LoadBlock(S,&x,sizeof(x));
 return x;
}

static lua_Number LoadNumber (LoadState* S)
{
 lua_Number x;
 LoadBlock(S,&x,sizeof(x));
 return x;
}

static TString* LoadString (LoadState* S)
{
 size_t size=LoadSize(S);
 if (size==0)
  return NULL;
 else
 {
  char* s=luaZ_openspace(S->L,S->b,size);
  ezread(S,s,size);
  return luaS_newlstr(S->L,s,size-1);		/* remove trailing '\0' */
 }
}

static void LoadCode (LoadState* S, Proto* f)
{
 int size=LoadInt(S);
 f->code=luaM_newvector(S->L,size,Instruction);
 f->sizecode=size;
 LoadVector(S,f->code,size,sizeof(Instruction));
}

static void LoadLocals (LoadState* S, Proto* f)
{
 int i,n;
 n=LoadInt(S);
 f->locvars=luaM_newvector(S->L,n,LocVar);
 f->sizelocvars=n;
 for (i=0; i<n; i++) f->locvars[i].varname=NULL;
 for (i=0; i<n; i++)
 {
  f->locvars[i].varname=LoadString(S);
  f->locvars[i].startpc=LoadInt(S);
  f->locvars[i].endpc=LoadInt(S);
 }
}

static void LoadLines (LoadState* S, Proto* f)
{
 int size=LoadInt(S);
 f->lineinfo=luaM_newvector(S->L,size,int);
 f->sizelineinfo=size;
 LoadVector(S,f->lineinfo,size,sizeof(int));
}

static void LoadUpvalues (LoadState* S, Proto* f)
{
 int i,n;
 n=LoadInt(S);
 if (n!=0 && n!=f->nups)
  error(S,"bad nupvalues (read %d; expected %d)",n,f->nups);
 f->upvalues=luaM_newvector(S->L,n,TString*);
 f->sizeupvalues=n;
 for (i=0; i<n; i++) f->upvalues[i]=NULL;
 for (i=0; i<n; i++) f->upvalues[i]=LoadString(S);
}

static Proto* LoadFunction (LoadState* S, TString* p);

static void LoadConstants (LoadState* S, Proto* f)
{
 int i,n;
 n=LoadInt(S);
 f->k=luaM_newvector(S->L,n,TValue);
 f->sizek=n;
 for (i=0; i<n; i++) setnilvalue(&f->k[i]);
 for (i=0; i<n; i++)
 {
  TValue* o=&f->k[i];
  int t=LoadByte(S);
  switch (t)
  {
   case LUA_TNUMBER:
	setnvalue(o,LoadNumber(S));
	break;
   case LUA_TSTRING:
	setsvalue2n(S->L,o,LoadString(S));
	break;
   case LUA_TNIL:
   	setnilvalue(o);
	break;
   default:
	error(S,"bad constant type (%d)",t);
	break;
  }
 }
 n=LoadInt(S);
 f->p=luaM_newvector(S->L,n,Proto*);
 f->sizep=n;
 for (i=0; i<n; i++) f->p[i]=NULL;
 for (i=0; i<n; i++) f->p[i]=LoadFunction(S,f->source);
}

static Proto* LoadFunction (LoadState* S, TString* p)
{
 Proto* f=luaF_newproto(S->L);
 setptvalue2s(S->L,S->L->top,f); incr_top(S->L);
 f->source=LoadString(S); if (f->source==NULL) f->source=p;
 f->lineDefined=LoadInt(S);
 f->nups=LoadByte(S);
 f->numparams=LoadByte(S);
 f->is_vararg=LoadByte(S);
 f->maxstacksize=LoadByte(S);
 LoadLines(S,f);
 LoadLocals(S,f);
 LoadUpvalues(S,f);
 LoadConstants(S,f);
 LoadCode(S,f);
#ifndef TRUST_BINARIES
 if (!luaG_checkcode(f)) error(S,"bad code");
#endif
 S->L->top--;
 return f;
}

static void LoadSignature (LoadState* S)
{
 const char* s=LUA_SIGNATURE;
 while (*s!=0 && ezgetc(S)==*s)
  ++s;
 if (*s!=0) error(S,"bad signature");
}

static void TestSize (LoadState* S, int s, const char* what)
{
 int r=LoadByte(S);
 if (r!=s)
  error(S,"bad size of %s (read %d; expected %d)",what,r,s);
}

#define V(v)		v/16,v%16

static void LoadHeader (LoadState* S)
{
 int version;
 lua_Number x,tx=TEST_NUMBER;
 LoadSignature(S);
 version=LoadByte(S);
 if (version>VERSION)
  error(S,"bad version (read %d.%d; expected at %s %d.%d)",
	V(version),"most",V(VERSION));
 if (version<VERSION0)				/* check last major change */
  error(S,"bad version (read %d.%d; expected at %s %d.%d)",
	V(version),"least",V(VERSION0));
 S->swap=(luaU_endianness()!=LoadByte(S));	/* need to swap bytes? */
 TestSize(S,sizeof(int),"int");
 TestSize(S,sizeof(size_t),"size_t");
 TestSize(S,sizeof(Instruction),"instruction");
 TestSize(S,sizeof(lua_Number),"number");
 x=LoadNumber(S);
 if ((long)x!=(long)tx)		/* disregard errors in last bits of fraction */
  error(S,"unknown number format");
}

/*
** load precompiled chunk
*/
Proto* luaU_undump (lua_State* L, ZIO* Z, Mbuffer* buff, const char* name)
{
 LoadState S;
 if (*name=='@' || *name=='=')
  S.name=name+1;
 else if (*name==LUA_SIGNATURE[0])
  S.name="binary string";
 else
  S.name=name;
 S.L=L;
 S.Z=Z;
 S.b=buff;
 LoadHeader(&S);
 return LoadFunction(&S,NULL);
}

/*
** find byte order
*/
int luaU_endianness (void)
{
 int x=1;
 return *(char*)&x;
}
