/*
** $Id: lundump.c,v 1.18 2000/03/03 14:58:26 roberto Exp roberto $
** load bytecodes from files
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <string.h>

#define LUA_REENTRANT

#include "lauxlib.h"
#include "lfunc.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#define	LoadBlock(L,b,size,Z)	ezread(L,Z,b,size)

static void unexpectedEOZ (lua_State* L, ZIO* Z)
{
 luaL_verror(L,"unexpected end of file in %.255s",zname(Z));
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

static unsigned int LoadWord (lua_State* L, ZIO* Z)
{
 unsigned int hi=ezgetc(L,Z);
 unsigned int lo=ezgetc(L,Z);
 return (hi<<8)|lo;
}

static unsigned long LoadLong (lua_State* L, ZIO* Z)
{
 unsigned long hi=LoadWord(L,Z);
 unsigned long lo=LoadWord(L,Z);
 return (hi<<16)|lo;
}

static Number LoadNumber (lua_State* L, ZIO* Z)
{
 char b[256];
 int size=ezgetc(L,Z);
 LoadBlock(L,b,size,Z);
 b[size]=0;
 return luaU_str2d(L,b,zname(Z));
}

static int LoadInt (lua_State* L, ZIO* Z, const char* message)
{
 unsigned long l=LoadLong(L,Z);
 unsigned int i=l;
 if (i!=l) luaL_verror(L,message,l,zname(Z));
 return i;
}

static void LoadCode (lua_State* L, Proto* tf, ZIO* Z)
{
 int size=LoadInt(L,Z,"code too long (%lu bytes) in %.255s");
 tf->code=luaM_newvector(L,size,Instruction);
 LoadBlock(L,tf->code,size*sizeof(tf->code[0]),Z);
 if (tf->code[size-1]!=OP_END) luaL_verror(L,"bad code in %.255s",zname(Z));
}

static TString* LoadString (lua_State* L, ZIO* Z)
{
 long size=LoadLong(L,Z);
 if (size==0)
  return NULL;
 else
 {
  char* s=luaL_openspace(L,size);
  LoadBlock(L,s,size,Z);
  return luaS_newlstr(L,s,size-1);	/* remove trailing '\0' */
 }
}

static void LoadLocals (lua_State* L, Proto* tf, ZIO* Z)
{
 int i,n=LoadInt(L,Z,"too many locals (%lu) in %.255s");
 if (n==0) return;
 tf->locvars=luaM_newvector(L,n+1,LocVar);
 for (i=0; i<n; i++)
 {
  tf->locvars[i].line=LoadInt(L,Z,"too many lines (%lu) in %.255s");
  tf->locvars[i].varname=LoadString(L,Z);
 }
 tf->locvars[i].line=-1;		/* flag end of vector */
 tf->locvars[i].varname=NULL;
}

static Proto* LoadFunction (lua_State* L, ZIO* Z, int native);

static void LoadConstants (lua_State* L, Proto* tf, ZIO* Z, int native)
{
 int i,n;
 tf->nkstr=n=LoadInt(L,Z,"too many strings (%lu) in %.255s");
 if (n>0)
 {
  tf->kstr=luaM_newvector(L,n,TString*);
  for (i=0; i<n; i++) tf->kstr[i]=LoadString(L,Z);
 }
 tf->nknum=n=LoadInt(L,Z,"too many numbers (%lu) in %.255s");
 if (n>0)
 {
  tf->knum=luaM_newvector(L,n,Number);
  if (native)
   LoadBlock(L,tf->knum,n*sizeof(tf->knum[0]),Z);
  else
   for (i=0; i<n; i++) tf->knum[i]=LoadNumber(L,Z);
 }
 tf->nkproto=n=LoadInt(L,Z,"too many functions (%lu) in %.255s");
 if (n>0)
 {
  tf->kproto=luaM_newvector(L,n,Proto*);
  for (i=0; i<n; i++) tf->kproto[i]=LoadFunction(L,Z,native);
 }
}

static Proto* LoadFunction (lua_State* L, ZIO* Z, int native)
{
 Proto* tf=luaF_newproto(L);
 tf->lineDefined=LoadInt(L,Z,"lineDefined too large (%lu) in %.255s");
 tf->source=LoadString(L,Z);
 if (tf->source==NULL) tf->source=luaS_new(L,zname(Z));
 tf->numparams=LoadInt(L,Z,"numparams too large (%lu) in %.255s");
 tf->is_vararg=LoadInt(L,Z,"is_vararg too large (%lu) in %.255s");
 tf->maxstacksize=LoadInt(L,Z,"maxstacksize too large (%lu) in %.255s");
 LoadCode(L,tf,Z);
 LoadLocals(L,tf,Z);
 LoadConstants(L,tf,Z,native);
 return tf;
}

static void LoadSignature (lua_State* L, ZIO* Z)
{
 const char* s=SIGNATURE;
 while (*s!=0 && ezgetc(L,Z)==*s)
  ++s;
 if (*s!=0) luaL_verror(L,"bad signature in %.255s",zname(Z));
}

#define V(v)	v/16,v%16

static int LoadHeader (lua_State* L, ZIO* Z)
{
 int version,sizeofR,native;
 LoadSignature(L,Z);
 version=ezgetc(L,Z);
 if (version>VERSION)
  luaL_verror(L,
	"%.255s too new: its version is %d.%d; expected at most %d.%d",
	zname(Z),V(version),V(VERSION));
 if (version<VERSION0)			/* check last major change */
  luaL_verror(L,
	"%.255s too old: its version is %d.%d; expected at least %d.%d",
	zname(Z),V(version),V(VERSION));
 sizeofR=ezgetc(L,Z);
 native=(sizeofR!=0);
 if (native)				/* test number representation */
 {
  if (sizeofR!=sizeof(Number))
   luaL_verror(L,"unknown number size in %.255s: read %d; expected %d",
	 zname(Z),sizeofR,(int)sizeof(Number));
  else
  {
   Number f=0,tf=TEST_NUMBER;
   LoadBlock(L,&f,sizeof(f),Z);
   if ((long)f!=(long)tf)		/* disregard errors in last bit of fraction */
    luaL_verror(L,"unknown number format in %.255s: "
	  "read " NUMBER_FMT "; expected " NUMBER_FMT,
	  zname(Z),f,tf);
  }
 }
 return native;
}

static Proto* LoadChunk (lua_State* L, ZIO* Z)
{
 return LoadFunction(L,Z,LoadHeader(L,Z));
}

/*
** load one chunk from a file or buffer
** return main if ok and NULL at EOF
*/
Proto* luaU_undump1 (lua_State* L, ZIO* Z)
{
 int c=zgetc(Z);
 if (c==ID_CHUNK)
  return LoadChunk(L,Z);
 else if (c!=EOZ)
  luaL_verror(L,"%.255s is not a Lua binary file",zname(Z));
 return NULL;
}

/*
* convert number from text
*/
double luaU_str2d (lua_State* L, const char* b, const char* where)
{
 double x;
 if (!luaO_str2d(b,&x))
  luaL_verror(L,"cannot convert number '%.255s' in %.255s",b,where);
 return x;
}
