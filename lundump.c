/*
** $Id: lundump.c,v 1.28 2000/04/24 19:32:58 lhf Exp $
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
 return luaU_str2d(L,b,ZNAME(Z));
}

static int LoadInt (lua_State* L, ZIO* Z, const char* message)
{
 unsigned long l=LoadLong(L,Z);
 unsigned int i=l;
 if (i!=l) luaL_verror(L,"%s in `%.255s';\n"
		"  read %lu; expected %u",message,ZNAME(Z),l,-1);
 return i;
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

static void SwapCode (lua_State* L, Instruction* code, int size, ZIO* Z)
{
 unsigned char* p;
 int c;
 if (sizeof(Instruction)==4)
  while (size--)
  {
   p=(unsigned char *) code++;
   c=p[0]; p[0]=p[3]; p[3]=c;
   c=p[1]; p[1]=p[2]; p[2]=c;
  }
 else if (sizeof(Instruction)==2)
  while (size--)
  {
   p=(unsigned char *) code++;
   c=p[0]; p[0]=p[1]; p[1]=c;
  }
 else
  luaL_verror(L,"cannot swap code with %d-byte instructions in `%.255s'",
	(int)sizeof(Instruction),ZNAME(Z));
}

static void LoadCode (lua_State* L, Proto* tf, ZIO* Z)
{
 int size=LoadInt(L,Z,"code too long");
 Instruction t=0,tt=TEST_CODE;
 tf->code=luaM_newvector(L,size,Instruction);
 LoadBlock(L,tf->code,size*sizeof(*tf->code),Z);
 if (tf->code[size-1]!=OP_END) luaL_verror(L,"bad code in `%.255s'",ZNAME(Z));
 LoadBlock(L,&t,sizeof(t),Z);
 if (t!=tt)
 {
  Instruction ot=t;
  SwapCode(L,&t,1,Z);
  if (t!=tt) luaL_verror(L,"cannot swap code in `%.255s';\n"
	"  read 0x%08X; swapped to 0x%08X; expected 0x%08X",
	ZNAME(Z),(unsigned long)ot,(unsigned long)t,(unsigned long)tt);
  SwapCode(L,tf->code,size,Z);
 }
}

static void LoadLocals (lua_State* L, Proto* tf, ZIO* Z)
{
 int i,n=LoadInt(L,Z,"too many locals");
 if (n==0) return;
 tf->locvars=luaM_newvector(L,n+1,LocVar);
 for (i=0; i<n; i++)
 {
  tf->locvars[i].line=LoadInt(L,Z,"too many lines");
  tf->locvars[i].varname=LoadString(L,Z);
 }
 tf->locvars[i].line=-1;		/* flag end of vector */
 tf->locvars[i].varname=NULL;
}

static Proto* LoadFunction (lua_State* L, ZIO* Z, int native);

static void LoadConstants (lua_State* L, Proto* tf, ZIO* Z, int native)
{
 int i,n;
 tf->nkstr=n=LoadInt(L,Z,"too many strings");
 if (n>0)
 {
  tf->kstr=luaM_newvector(L,n,TString*);
  for (i=0; i<n; i++)
  {
   TString* s=LoadString(L,Z);
   int isglobal=LoadByte(L,Z);
   if (isglobal) luaS_assertglobal(L,s);
   tf->kstr[i]=s;
  }
 }
 tf->nknum=n=LoadInt(L,Z,"too many numbers");
 if (n>0)
 {
  tf->knum=luaM_newvector(L,n,Number);
  if (native)
   LoadBlock(L,tf->knum,n*sizeof(*tf->knum),Z);
  else
   for (i=0; i<n; i++) tf->knum[i]=LoadNumber(L,Z);
 }
 tf->nkproto=n=LoadInt(L,Z,"too many functions");
 if (n>0)
 {
  tf->kproto=luaM_newvector(L,n,Proto*);
  for (i=0; i<n; i++) tf->kproto[i]=LoadFunction(L,Z,native);
 }
}

static Proto* LoadFunction (lua_State* L, ZIO* Z, int native)
{
 Proto* tf=luaF_newproto(L);
 tf->source=LoadString(L,Z);
 if (tf->source==NULL) tf->source=luaS_new(L,zname(Z));
 tf->lineDefined=LoadInt(L,Z,"lineDefined too large");
 tf->numparams=LoadInt(L,Z,"too many parameters");
 tf->is_vararg=LoadByte(L,Z);
 tf->maxstacksize=LoadInt(L,Z,"too much stack needed");
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
 if (*s!=0) luaL_verror(L,"bad signature in `%.255s'",ZNAME(Z));
}

#define V(v)	v/16,v%16

static int LoadHeader (lua_State* L, ZIO* Z)
{
 int version,sizeofR,native;
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
 {
  int I=ezgetc(L,Z);
  int i=ezgetc(L,Z);
  int op=ezgetc(L,Z);
  int b=ezgetc(L,Z);
  if (I!=sizeof(Instruction) || i!=SIZE_INSTRUCTION || op!=SIZE_OP || b!=SIZE_B)
   luaL_verror(L,"virtual machine mismatch in `%.255s':\n"
   	"  read %d-bit,%d-byte instructions, %d-bit opcodes, %d-bit b-arguments;\n"
   	"  expected %d-bit,%d-byte instructions, %d-bit opcodes, %d-bit b-arguments",
	ZNAME(Z),i,I,op,b,SIZE_INSTRUCTION,(int)sizeof(Instruction),SIZE_OP,SIZE_B);
 }
 sizeofR=ezgetc(L,Z);
 native=(sizeofR!=0);
 if (native)				/* test number representation */
 {
  if (sizeofR!=sizeof(Number))
   luaL_verror(L,"native number size mismatch in `%.255s':\n"
	"  read %d; expected %d",
	ZNAME(Z),sizeofR,(int)sizeof(Number));
  else
  {
   Number f=0,tf=TEST_NUMBER;
   LoadBlock(L,&f,sizeof(f),Z);
   if ((long)f!=(long)tf)		/* disregard errors in last bit of fraction */
    luaL_verror(L,"unknown number format in `%.255s':\n"
	"  read " NUMBER_FMT "; expected " NUMBER_FMT,
	ZNAME(Z),f,tf);
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
  luaL_verror(L,"`%.255s' is not a Lua binary file",ZNAME(Z));
 return NULL;
}

/*
* convert number from text
*/
double luaU_str2d (lua_State* L, const char* b, const char* where)
{
 double x;
 if (!luaO_str2d(b,&x))
  luaL_verror(L,"cannot convert number `%.255s' in `%.255s'",b,where);
 return x;
}
