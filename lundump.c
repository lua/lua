/*
** $Id: lundump.c,v 1.35 2001/06/28 13:55:17 lhf Exp lhf $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <string.h>

#define LUA_PRIVATE
#include "lua.h"

#include "ldebug.h"
#include "lfunc.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#define	LoadByte		ezgetc

static const l_char* ZNAME (ZIO* Z)
{
 const l_char* s=zname(Z);
 return (*s==l_c('@')) ? s+1 : s;
}

static void unexpectedEOZ (lua_State* L, ZIO* Z)
{
 luaO_verror(L,l_s("unexpected end of file in `%.99s'"),ZNAME(Z));
}

static int ezgetc (lua_State* L, ZIO* Z)
{
 int c=zgetc(Z);
 if (c==EOZ) unexpectedEOZ(L,Z);
 return c;
}

static void ezread (lua_State* L, ZIO* Z, void* b, int n)
{
 int r=zread(Z,b,n);
 if (r!=0) unexpectedEOZ(L,Z);
}

static void LoadBlock (lua_State* L, void* b, size_t size, ZIO* Z, int swap)
{
 if (swap)
 {
  l_char *p=(l_char*) b+size-1;
  int n=size;
  while (n--) *p--=(l_char)ezgetc(L,Z);
 }
 else
  ezread(L,Z,b,size);
}

static void LoadVector (lua_State* L, void* b, int m, size_t size, ZIO* Z, int swap)
{
 if (swap)
 {
  l_char *q=(l_char*) b;
  while (m--)
  {
   l_char *p=q+size-1;
   int n=size;
   while (n--) *p--=(l_char)ezgetc(L,Z);
   q+=size;
  }
 }
 else
  ezread(L,Z,b,m*size);
}

static int LoadInt (lua_State* L, ZIO* Z, int swap)
{
 int x;
 LoadBlock(L,&x,sizeof(x),Z,swap);
 return x;
}

static size_t LoadSize (lua_State* L, ZIO* Z, int swap)
{
 size_t x;
 LoadBlock(L,&x,sizeof(x),Z,swap);
 return x;
}

static lua_Number LoadNumber (lua_State* L, ZIO* Z, int swap)
{
 lua_Number x;
 LoadBlock(L,&x,sizeof(x),Z,swap);
 return x;
}

static TString* LoadString (lua_State* L, ZIO* Z, int swap)
{
 size_t size=LoadSize(L,Z,swap);
 if (size==0)
  return NULL;
 else
 {
  l_char* s=luaO_openspace(L,size,l_char);
  LoadBlock(L,s,size,Z,0);
  return luaS_newlstr(L,s,size-1);	/* remove trailing '\0' */
 }
}

static void LoadCode (lua_State* L, Proto* f, ZIO* Z, int swap)
{
 int size=LoadInt(L,Z,swap);
 f->code=luaM_newvector(L,size,Instruction);
 f->sizecode=size;
 LoadVector(L,f->code,size,sizeof(*f->code),Z,swap);
}

static void LoadLocals (lua_State* L, Proto* f, ZIO* Z, int swap)
{
 int i,n;
 n=LoadInt(L,Z,swap);
 f->locvars=luaM_newvector(L,n,LocVar);
 f->sizelocvars=n;
 for (i=0; i<n; i++)
 {
  f->locvars[i].varname=LoadString(L,Z,swap);
  f->locvars[i].startpc=LoadInt(L,Z,swap);
  f->locvars[i].endpc=LoadInt(L,Z,swap);
 }
}

static void LoadLines (lua_State* L, Proto* f, ZIO* Z, int swap)
{
 int n;
 n=LoadInt(L,Z,swap);
 f->lineinfo=luaM_newvector(L,n,int);
 f->sizelineinfo=n;
 LoadVector(L,f->lineinfo,n,sizeof(*f->lineinfo),Z,swap);
}

static Proto* LoadFunction (lua_State* L, ZIO* Z, int swap);

static void LoadConstants (lua_State* L, Proto* f, ZIO* Z, int swap)
{
 int i,n;
 n=LoadInt(L,Z,swap);
 f->k=luaM_newvector(L,n,TObject);
 f->sizek=n;
 for (i=0; i<n; i++)
 {
  TObject* o=&f->k[i];
  ttype(o)=LoadByte(L,Z);
  switch (ttype(o))
  {
   case LUA_TNUMBER:
	nvalue(o)=LoadNumber(L,Z,swap);
	break;
   case LUA_TSTRING:
	tsvalue(o)=LoadString(L,Z,swap);
	break;
   default:
	luaO_verror(L,l_s("bad constant type (%d) in `%.99s'"),ttype(o),ZNAME(Z));
	break;
  }
 }
 n=LoadInt(L,Z,swap);
 f->p=luaM_newvector(L,n,Proto*);
 f->sizep=n;
 for (i=0; i<n; i++) f->p[i]=LoadFunction(L,Z,swap);
}

static Proto* LoadFunction (lua_State* L, ZIO* Z, int swap)
{
 Proto* f=luaF_newproto(L);
 f->source=LoadString(L,Z,swap);
 f->lineDefined=LoadInt(L,Z,swap);
 f->nupvalues=LoadInt(L,Z,swap);
 f->numparams=LoadInt(L,Z,swap);
 f->is_vararg=LoadByte(L,Z);
 f->maxstacksize=LoadInt(L,Z,swap);
 LoadLocals(L,f,Z,swap);
 LoadLines(L,f,Z,swap);
 LoadConstants(L,f,Z,swap);
 LoadCode(L,f,Z,swap);
#ifndef TRUST_BINARIES
 if (luaG_checkcode(f)==0) luaO_verror(L,l_s("bad code in `%.99s'"),ZNAME(Z));
#endif
 return f;
}

static void LoadSignature (lua_State* L, ZIO* Z)
{
 const l_char* s=l_s(LUA_SIGNATURE);
 while (*s!=0 && ezgetc(L,Z)==*s)
  ++s;
 if (*s!=0) luaO_verror(L,l_s("bad signature in `%.99s'"),ZNAME(Z));
}

static void TestSize (lua_State* L, int s, const l_char* what, ZIO* Z)
{
 int r=ezgetc(L,Z);
 if (r!=s)
  luaO_verror(L,l_s("virtual machine mismatch in `%.99s':\n")
	l_s("  %.20s is %d but read %d"),ZNAME(Z),what,s,r);
}

#define TESTSIZE(s)	TestSize(L,s,#s,Z)
#define V(v)	v/16,v%16

static int LoadHeader (lua_State* L, ZIO* Z)
{
 int version,swap;
 lua_Number x=0,tx=TEST_NUMBER;
 LoadSignature(L,Z);
 version=ezgetc(L,Z);
 if (version>VERSION)
  luaO_verror(L,l_s("`%.99s' too new:\n")
	l_s("  read version %d.%d; expected at most %d.%d"),
	ZNAME(Z),V(version),V(VERSION));
 if (version<VERSION0)			/* check last major change */
  luaO_verror(L,l_s("`%.99s' too old:\n")
	l_s("  read version %d.%d; expected at least %d.%d"),
	ZNAME(Z),V(version),V(VERSION));
 swap=(luaU_endianness()!=ezgetc(L,Z));	/* need to swap bytes? */
 TESTSIZE(sizeof(int));
 TESTSIZE(sizeof(size_t));
 TESTSIZE(sizeof(Instruction));
 TESTSIZE(SIZE_OP);
 TESTSIZE(SIZE_A);
 TESTSIZE(SIZE_B);
 TESTSIZE(SIZE_C);
 TESTSIZE(sizeof(lua_Number));
 x=LoadNumber(L,Z,swap);
 if ((long)x!=(long)tx)		/* disregard errors in last bits of fraction */
  luaO_verror(L,l_s("unknown number format in `%.99s':\n")
      l_s("  read ") l_s(LUA_NUMBER_FMT) l_s("; expected ") l_s(LUA_NUMBER_FMT),
      ZNAME(Z),x,tx);
 return swap;
}

static Proto* LoadChunk (lua_State* L, ZIO* Z)
{
 return LoadFunction(L,Z,LoadHeader(L,Z));
}

/*
** load one chunk from a file or buffer
*/
Proto* luaU_undump (lua_State* L, ZIO* Z)
{
 Proto* f=LoadChunk(L,Z);
 if (zgetc(Z)!=EOZ)
  luaO_verror(L,l_s("`%.99s' apparently contains more than one chunk"),ZNAME(Z));
 return f;
}

/*
** find byte order
*/
int luaU_endianness (void)
{
 int x=1;
 return *(l_char*)&x;
}
