/*
** $Id: lundump.c,v 1.27 2000/09/04 18:53:41 roberto Exp roberto $
** load bytecodes from files
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <string.h>

#include "lauxlib.h"
#include "lfunc.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#define	LoadVector(L,b,n,s,Z)	LoadBlock(L,b,(n)*(s),Z)
#define	LoadBlock(L,b,size,Z)	ezread(L,Z,b,size)
#define	LoadByte		ezgetc

static const char* ZNAME(ZIO* Z)
{
 const char* s=zname(Z);
 return (*s=='@') ? s+1 : s;
}

static void unexpectedEOZ (lua_State* L, ZIO* Z)
{
 luaL_verror(L,"unexpected end of file in `%.255s'",ZNAME(Z));
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

static void LoadReverse (lua_State* L, void* b, size_t size, ZIO* Z)
{
 unsigned char *p=(unsigned char *) b+size;
 int n=size;
 while (n--) *p--=ezgetc(L,Z);
}

static int LoadInt (lua_State* L, ZIO* Z, int swap)
{
 int x;
 if (swap)
  LoadReverse(L,&x,sizeof(x),Z);
 else
  LoadBlock(L,&x,sizeof(x),Z);
 return x;
}

static size_t LoadSize (lua_State* L, ZIO* Z, int swap)
{
 size_t x;
 if (swap)
  LoadReverse(L,&x,sizeof(x),Z);
 else
  LoadBlock(L,&x,sizeof(x),Z);
 return x;
}

static Number LoadNumber (lua_State* L, ZIO* Z, int swap)
{
 Number x;
 if (swap)
  LoadReverse(L,&x,sizeof(x),Z);
 else
  LoadBlock(L,&x,sizeof(x),Z);
 return x;
}

static TString* LoadString (lua_State* L, ZIO* Z, int swap)
{
 size_t size=LoadSize(L,Z,swap);
 if (size==0)
  return NULL;
 else
 {
  char* s=luaO_openspace(L,size);
  LoadBlock(L,s,size,Z);
  return luaS_newlstr(L,s,size-1);	/* remove trailing '\0' */
 }
}

static void LoadCode (lua_State* L, Proto* tf, ZIO* Z, int swap)
{
 int size=LoadInt(L,Z,swap);
 tf->code=luaM_newvector(L,size,Instruction);
 LoadVector(L,tf->code,size,sizeof(*tf->code),Z);
#if 0
 if (swap) SwapBytes(tf->code,sizeof(*tf->code),size);
#endif
 if (tf->code[size-1]!=OP_END) luaL_verror(L,"bad code in `%.255s'",ZNAME(Z));
}

static void LoadLocals (lua_State* L, Proto* tf, ZIO* Z, int swap)
{
 int i,n;
 tf->nlocvars=n=LoadInt(L,Z,swap);
 tf->locvars=luaM_newvector(L,n,LocVar);
 for (i=0; i<n; i++)
 {
  tf->locvars[i].varname=LoadString(L,Z,swap);
  tf->locvars[i].startpc=LoadInt(L,Z,swap);
  tf->locvars[i].endpc=LoadInt(L,Z,swap);
 }
}

static void LoadLines (lua_State* L, Proto* tf, ZIO* Z, int swap)
{
 int n=LoadInt(L,Z,swap);
 if (n==0) return;
 tf->lineinfo=luaM_newvector(L,n,int);
 LoadVector(L,tf->lineinfo,n,sizeof(*tf->lineinfo),Z);
#if 0
 if (swap) SwapBytes(tf->lineinfo,sizeof(*tf->lineinfo),n);
#endif
}

static Proto* LoadFunction (lua_State* L, ZIO* Z, int swap);

static void LoadConstants (lua_State* L, Proto* tf, ZIO* Z, int swap)
{
 int i,n;
 tf->nkstr=n=LoadInt(L,Z,swap);
 tf->kstr=luaM_newvector(L,n,TString*);
 for (i=0; i<n; i++)
  tf->kstr[i]=LoadString(L,Z,swap);
 tf->nknum=n=LoadInt(L,Z,swap);
 tf->knum=luaM_newvector(L,n,Number);
 LoadVector(L,tf->knum,n,sizeof(*tf->knum),Z);
 if (swap)
  for (i=0; i<n; i++) tf->knum[i]=LoadNumber(L,Z,swap); /* TODO */
 tf->nkproto=n=LoadInt(L,Z,swap);
 tf->kproto=luaM_newvector(L,n,Proto*);
 for (i=0; i<n; i++)
  tf->kproto[i]=LoadFunction(L,Z,swap);
}

static Proto* LoadFunction (lua_State* L, ZIO* Z, int swap)
{
 Proto* tf=luaF_newproto(L);
 tf->source=LoadString(L,Z,swap);
 tf->lineDefined=LoadInt(L,Z,swap);
 tf->numparams=LoadInt(L,Z,swap);
 tf->is_vararg=LoadByte(L,Z);
 tf->maxstacksize=LoadInt(L,Z,swap);
 LoadCode(L,tf,Z,swap);
 LoadLocals(L,tf,Z,swap);
 LoadLines(L,tf,Z,swap);
 LoadConstants(L,tf,Z,swap);
 return tf;
}

static void LoadSignature (lua_State* L, ZIO* Z)
{
 const char* s=SIGNATURE;
 while (*s!=0 && ezgetc(L,Z)==*s)
  ++s;
 if (*s!=0) luaL_verror(L,"bad signature in `%.255s'",ZNAME(Z));
}

static void TestSize (lua_State* L, int s, const char* what, ZIO* Z)
{
 int r=ezgetc(L,Z);
 if (r!=s)
  luaL_verror(L,"virtual machine mismatch in `%.255s':\n"
	"  %s is %d but read %d",ZNAME(Z),what,r,s);
}

#define TESTSIZE(s)	TestSize(L,s,#s,Z)
#define V(v)	v/16,v%16

static int LoadHeader (lua_State* L, ZIO* Z)
{
 int version,swap;
 Number f=0,tf=TEST_NUMBER;
 LoadSignature(L,Z);
 version=ezgetc(L,Z);
 if (version>VERSION)
  luaL_verror(L,"`%.255s' too new:\n"
	"  read version %d.%d; expected at most %d.%d",
	ZNAME(Z),V(version),V(VERSION));
 if (version<VERSION0)			/* check last major change */
  luaL_verror(L,"`%.255s' too old:\n"
	"  read version %d.%d; expected at least %d.%d",
	ZNAME(Z),V(version),V(VERSION));
 swap=(luaU_endianess()!=ezgetc(L,Z));	/* need to swap bytes? */
 TESTSIZE(sizeof(int));
 TESTSIZE(sizeof(size_t));
 TESTSIZE(sizeof(Instruction));
 TESTSIZE(SIZE_INSTRUCTION);
 TESTSIZE(SIZE_OP);
 TESTSIZE(SIZE_B);
 TESTSIZE(sizeof(Number));
 f=LoadNumber(L,Z,swap);
 if ((long)f!=(long)tf)		/* disregard errors in last bit of fraction */
  luaL_verror(L,"unknown number format in `%.255s':\n"
      "  read " NUMBER_FMT "; expected " NUMBER_FMT,
      ZNAME(Z),f,tf);
 return swap;
}

static Proto* LoadChunk (lua_State* L, ZIO* Z)
{
 return LoadFunction(L,Z,LoadHeader(L,Z));
}

/*
** load one chunk from a file or buffer
** return main if ok and NULL at EOF
*/
Proto* luaU_undump (lua_State* L, ZIO* Z)
{
 int c=zgetc(Z);
 if (c==ID_CHUNK)
  return LoadChunk(L,Z);
 else if (c!=EOZ)
  luaL_verror(L,"`%.255s' is not a precompiled Lua chunk",ZNAME(Z));
 return NULL;
}

Proto* luaU_undump1 (lua_State* L, ZIO* Z)
{
 return luaU_undump(L,Z);
}

/*
** find byte order
*/
int luaU_endianess (void)
{
 int x=1;
 return *(char*)&x;
}
