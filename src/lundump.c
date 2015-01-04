/*
** $Id: lundump.c,v 1.12 1998/07/12 01:46:59 lhf Exp $
** load bytecodes from files
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <string.h>
#include "lauxlib.h"
#include "lfunc.h"
#include "lmem.h"
#include "lstring.h"
#include "lundump.h"

#define	LoadBlock(b,size,Z)	ezread(Z,b,size)
#define	LoadNative(t,Z)		LoadBlock(&t,sizeof(t),Z)

#if ID_NUMBER==ID_NATIVE
	#define doLoadNumber(f,Z)	LoadNative(f,Z)
#else
	#define doLoadNumber(f,Z)	f=LoadNumber(Z)
#endif

static void unexpectedEOZ(ZIO* Z)
{
 luaL_verror("unexpected end of file in %s",zname(Z));
}

static int ezgetc(ZIO* Z)
{
 int c=zgetc(Z);
 if (c==EOZ) unexpectedEOZ(Z);
 return c;
}

static void ezread(ZIO* Z, void* b, int n)
{
 int r=zread(Z,b,n);
 if (r!=0) unexpectedEOZ(Z);
}

static unsigned int LoadWord(ZIO* Z)
{
 unsigned int hi=ezgetc(Z);
 unsigned int lo=ezgetc(Z);
 return (hi<<8)|lo;
}

static unsigned long LoadLong(ZIO* Z)
{
 unsigned long hi=LoadWord(Z);
 unsigned long lo=LoadWord(Z);
 return (hi<<16)|lo;
}

#if ID_NUMBER==ID_REAL4
/* LUA_NUMBER */
/* assumes sizeof(long)==4 and sizeof(float)==4 (IEEE) */
static float LoadFloat(ZIO* Z)
{
 unsigned long l=LoadLong(Z);
 float f;
 memcpy(&f,&l,sizeof(f));
 return f;
}
#endif

#if ID_NUMBER==ID_REAL8
/* LUA_NUMBER */
/* assumes sizeof(long)==4 and sizeof(double)==8 (IEEE) */
static double LoadDouble(ZIO* Z)
{
 unsigned long l[2];
 double f;
 int x=1;
 if (*(char*)&x==1)			/* little-endian */
 {
  l[1]=LoadLong(Z);
  l[0]=LoadLong(Z);
 }
 else					/* big-endian */
 {
  l[0]=LoadLong(Z);
  l[1]=LoadLong(Z);
 }
 memcpy(&f,l,sizeof(f));
 return f;
}
#endif

static Byte* LoadCode(ZIO* Z)
{
 unsigned long size=LoadLong(Z);
 unsigned int s=size;
 void* b;
 if (s!=size) luaL_verror("code too long (%ld bytes) in %s",size,zname(Z));
 b=luaM_malloc(size);
 LoadBlock(b,size,Z);
 return b;
}

static TaggedString* LoadTString(ZIO* Z)
{
 int size=LoadWord(Z);
 if (size==0)
  return NULL;
 else
 {
  char* s=luaL_openspace(size);
  LoadBlock(s,size,Z);
  return luaS_newlstr(s,size-1);
 }
}

static void LoadLocals(TProtoFunc* tf, ZIO* Z)
{
 int i,n=LoadWord(Z);
 if (n==0) return;
 tf->locvars=luaM_newvector(n+1,LocVar);
 for (i=0; i<n; i++)
 {
  tf->locvars[i].line=LoadWord(Z);
  tf->locvars[i].varname=LoadTString(Z);
 }
 tf->locvars[i].line=-1;		/* flag end of vector */
 tf->locvars[i].varname=NULL;
}

static TProtoFunc* LoadFunction(ZIO* Z);

static void LoadConstants(TProtoFunc* tf, ZIO* Z)
{
 int i,n=LoadWord(Z);
 tf->nconsts=n;
 if (n==0) return;
 tf->consts=luaM_newvector(n,TObject);
 for (i=0; i<n; i++)
 {
  TObject* o=tf->consts+i;
  ttype(o)=-ezgetc(Z);
  switch (ttype(o))
  {
   case LUA_T_NUMBER:
	doLoadNumber(nvalue(o),Z);
	break;
   case LUA_T_STRING:
	tsvalue(o)=LoadTString(Z);
	break;
   case LUA_T_PROTO:
	tfvalue(o)=LoadFunction(Z);
	break;
   case LUA_T_NIL:
	break;
   default:
	luaL_verror("bad constant #%d in %s: type=%d [%s]",
		i,zname(Z),ttype(o),luaO_typename(o));
	break;
  }
 }
}

static TProtoFunc* LoadFunction(ZIO* Z)
{
 TProtoFunc* tf=luaF_newproto();
 tf->lineDefined=LoadWord(Z);
 tf->fileName=LoadTString(Z);
 tf->code=LoadCode(Z);
 LoadLocals(tf,Z);
 LoadConstants(tf,Z);
 return tf;
}

static void LoadSignature(ZIO* Z)
{
 char* s=SIGNATURE;
 while (*s!=0 && ezgetc(Z)==*s)
  ++s;
 if (*s!=0) luaL_verror("bad signature in %s",zname(Z));
}

static void LoadHeader(ZIO* Z)
{
 int version,id,sizeofR;
 real f=-TEST_NUMBER,tf=TEST_NUMBER;
 LoadSignature(Z);
 version=ezgetc(Z);
 if (version>VERSION)
  luaL_verror(
	"%s too new: version=0x%02x; expected at most 0x%02x",
	zname(Z),version,VERSION);
 if (version<VERSION0)			/* check last major change */
  luaL_verror(
	"%s too old: version=0x%02x; expected at least 0x%02x",
	zname(Z),version,VERSION0);
 id=ezgetc(Z);				/* test number representation */
 sizeofR=ezgetc(Z);
 if (id!=ID_NUMBER || sizeofR!=sizeof(real))
 {
  luaL_verror("unknown number signature in %s: "
	"read 0x%02x%02x; expected 0x%02x%02x",
	zname(Z),id,sizeofR,ID_NUMBER,sizeof(real));
 }
 doLoadNumber(f,Z);
 if (f!=tf)
  luaL_verror("unknown number representation in %s: "
	"read " NUMBER_FMT "; expected " NUMBER_FMT,	/* LUA_NUMBER */
	zname(Z),f,tf);
}

static TProtoFunc* LoadChunk(ZIO* Z)
{
 LoadHeader(Z);
 return LoadFunction(Z);
}

/*
** load one chunk from a file or buffer
** return main if ok and NULL at EOF
*/
TProtoFunc* luaU_undump1(ZIO* Z)
{
 int c=zgetc(Z);
 if (c==ID_CHUNK)
  return LoadChunk(Z);
 else if (c!=EOZ)
  luaL_verror("%s is not a Lua binary file",zname(Z));
 return NULL;
}
