/*
** $Id: lundump.c,v 1.41 2002/06/06 13:22:56 lhf Exp lhf $
** load pre-compiled Lua chunks
** See Copyright Notice in lua.h
*/

#include "lua.h"

#include "ldebug.h"
#include "lfunc.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"
#include "lzio.h"

#define	LoadByte	(lu_byte) ezgetc

static const char* ZNAME (ZIO* Z)
{
 const char* s=zname(Z);
 if (*s=='@' || *s=='=')
  return s+1;
 else if (*s==LUA_SIGNATURE[0])
  return "binary string";
 else
  return s;
}

static void unexpectedEOZ (lua_State* L, ZIO* Z)
{
 luaG_runerror(L,"unexpected end of file in %s",ZNAME(Z));
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
  char *p=(char*) b+size-1;
  int n=size;
  while (n--) *p--=(char)ezgetc(L,Z);
 }
 else
  ezread(L,Z,b,size);
}

static void LoadVector (lua_State* L, void* b, int m, size_t size, ZIO* Z, int swap)
{
 if (swap)
 {
  char *q=(char*) b;
  while (m--)
  {
   char *p=q+size-1;
   int n=size;
   while (n--) *p--=(char)ezgetc(L,Z);
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
 if (x<0) luaG_runerror(L,"bad integer in %s",ZNAME(Z));
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
  char* s=luaO_openspace(L,size,char);
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
 LoadVector(L,f->lineinfo,n,sizeof(*f->lineinfo),Z,swap);
}

static Proto* LoadFunction (lua_State* L, TString* p, ZIO* Z, int swap);

static void LoadConstants (lua_State* L, Proto* f, ZIO* Z, int swap)
{
 int i,n;
 n=LoadInt(L,Z,swap);
 f->k=luaM_newvector(L,n,TObject);
 f->sizek=n;
 for (i=0; i<n; i++)
 {
  TObject* o=&f->k[i];
  int t=LoadByte(L,Z);
  switch (t)
  {
   case LUA_TNUMBER:
	setnvalue(o,LoadNumber(L,Z,swap));
	break;
   case LUA_TSTRING:
	setsvalue(o,LoadString(L,Z,swap));
	break;
   case LUA_TNIL:
   	setnilvalue(o);
	break;
   default:
	luaG_runerror(L,"bad constant type (%d) in %s",t,ZNAME(Z));
	break;
  }
 }
 n=LoadInt(L,Z,swap);
 f->p=luaM_newvector(L,n,Proto*);
 f->sizep=n;
 for (i=0; i<n; i++) f->p[i]=LoadFunction(L,f->source,Z,swap);
}

static Proto* LoadFunction (lua_State* L, TString* p, ZIO* Z, int swap)
{
 Proto* f=luaF_newproto(L);
 f->source=LoadString(L,Z,swap); if (f->source==NULL) f->source=p;
 f->lineDefined=LoadInt(L,Z,swap);
 f->nupvalues=LoadByte(L,Z);
 f->numparams=LoadByte(L,Z);
 f->is_vararg=LoadByte(L,Z);
 f->maxstacksize=LoadByte(L,Z);
 LoadLocals(L,f,Z,swap);
 LoadLines(L,f,Z,swap);
 LoadConstants(L,f,Z,swap);
 LoadCode(L,f,Z,swap);
#ifndef TRUST_BINARIES
 if (!luaG_checkcode(f)) luaG_runerror(L,"bad code in %s",ZNAME(Z));
#endif
 return f;
}

static void LoadSignature (lua_State* L, ZIO* Z)
{
 const char* s=LUA_SIGNATURE;
 while (*s!=0 && ezgetc(L,Z)==*s)
  ++s;
 if (*s!=0) luaG_runerror(L,"bad signature in %s",ZNAME(Z));
}

static void TestSize (lua_State* L, int s, const char* what, ZIO* Z)
{
 int r=LoadByte(L,Z);
 if (r!=s)
  luaG_runerror(L,"virtual machine mismatch in %s: "
	"size of %s is %d but read %d",ZNAME(Z),what,s,r);
}

#define TESTSIZE(s,w)	TestSize(L,s,w,Z)
#define V(v)		v/16,v%16

static int LoadHeader (lua_State* L, ZIO* Z)
{
 int version,swap;
 lua_Number x=0,tx=TEST_NUMBER;
 LoadSignature(L,Z);
 version=LoadByte(L,Z);
 if (version>VERSION)
  luaG_runerror(L,"%s too new: "
	"read version %d.%d; expected at most %d.%d",
	ZNAME(Z),V(version),V(VERSION));
 if (version<VERSION0)			/* check last major change */
  luaG_runerror(L,"%s too old: "
	"read version %d.%d; expected at least %d.%d",
	ZNAME(Z),V(version),V(VERSION0));
 swap=(luaU_endianness()!=LoadByte(L,Z));	/* need to swap bytes? */
 TESTSIZE(sizeof(int),"int");
 TESTSIZE(sizeof(size_t), "size_t");
 TESTSIZE(sizeof(Instruction), "Instruction");
 TESTSIZE(SIZE_OP, "OP");
 TESTSIZE(SIZE_A, "A");
 TESTSIZE(SIZE_B, "B");
 TESTSIZE(SIZE_C, "C");
 TESTSIZE(sizeof(lua_Number), "number");
 x=LoadNumber(L,Z,swap);
 if ((long)x!=(long)tx)		/* disregard errors in last bits of fraction */
  luaG_runerror(L,"unknown number format in %s: read %f; expected %f",
      ZNAME(Z),x,tx);
 return swap;
}

static Proto* LoadChunk (lua_State* L, ZIO* Z)
{
 return LoadFunction(L,NULL,Z,LoadHeader(L,Z));
}

/*
** load one chunk from a file or buffer
*/
Proto* luaU_undump (lua_State* L, ZIO* Z)
{
 Proto* f=LoadChunk(L,Z);
 if (zgetc(Z)!=EOZ)
  luaG_runerror(L,"%s apparently contains more than one chunk",ZNAME(Z));
 return f;
}

/*
** find byte order
*/
int luaU_endianness (void)
{
 int x=1;
 return *(char*)&x;
}
