/*
** $Id: lundump.c,v 1.4 1998/01/13 20:05:24 lhf Exp $
** load bytecodes from files
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include "lauxlib.h"
#include "lfunc.h"
#include "lmem.h"
#include "lstring.h"
#include "lundump.h"

typedef struct {
 ZIO* Z;
 int SwapNumber;
 int LoadFloat;
} Sundump;

static void unexpectedEOZ(ZIO* Z)
{
 luaL_verror("unexpected end of binary file %s",Z->name);
}

static int ezgetc(ZIO* Z)
{
 int c=zgetc(Z);
 if (c==EOZ) unexpectedEOZ(Z);
 return c;
}

static int ezread(ZIO* Z, void* b, int n)
{
 int r=zread(Z,b,n);
 if (r!=0) unexpectedEOZ(Z);
 return r;
}

static int LoadWord(ZIO* Z)
{
 int hi=ezgetc(Z);
 int lo=ezgetc(Z);
 return (hi<<8)|lo;
}

static void* LoadBlock(int size, ZIO* Z)
{
 void* b=luaM_malloc(size);
 ezread(Z,b,size);
 return b;
}

static int LoadSize(ZIO* Z)
{
 int hi=LoadWord(Z);
 int lo=LoadWord(Z);
 int s=(hi<<16)|lo;
 if (hi!=0 && s==lo)
  luaL_verror("code too long (%ld bytes)",(hi<<16)|(long)lo);
 return s;
}

static char* LoadString(ZIO* Z)
{
 int size=LoadWord(Z);
 if (size==0)
  return NULL;
 else
 {
  char* b=luaL_openspace(size);
  ezread(Z,b,size);
  return b;
 }
}

static TaggedString* LoadTString(ZIO* Z)
{
 char* s=LoadString(Z);
 return (s==NULL) ? NULL : luaS_new(s);
}

static void SwapFloat(float* f)
{
 Byte* p=(Byte*)f;
 Byte* q=p+sizeof(float)-1;
 Byte t;
 t=*p; *p++=*q; *q--=t;
 t=*p; *p++=*q; *q--=t;
}

static void SwapDouble(double* f)
{
 Byte* p=(Byte*)f;
 Byte* q=p+sizeof(double)-1;
 Byte t;
 t=*p; *p++=*q; *q--=t;
 t=*p; *p++=*q; *q--=t;
 t=*p; *p++=*q; *q--=t;
 t=*p; *p++=*q; *q--=t;
}

static real LoadNumber(Sundump* S)
{
 if (S->LoadFloat)
 {
  float f;
  ezread(S->Z,&f,sizeof(f));
  if (S->SwapNumber) SwapFloat(&f);
  return f;
 }
 else
 {
  double f;
  ezread(S->Z,&f,sizeof(f));
  if (S->SwapNumber) SwapDouble(&f);
  return f;
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

static void LoadConstants(TProtoFunc* tf, Sundump* S)
{
 int i,n=LoadWord(S->Z);
 tf->nconsts=n;
 if (n==0) return;
 tf->consts=luaM_newvector(n,TObject);
 for (i=0; i<n; i++)
 {
  TObject* o=tf->consts+i;
  int c=ezgetc(S->Z);
  switch (c)
  {
   case ID_NUM:
	ttype(o)=LUA_T_NUMBER;
	nvalue(o)=LoadNumber(S);
	break;
   case ID_STR:
	ttype(o)=LUA_T_STRING;	
	tsvalue(o)=LoadTString(S->Z);
	break;
   case ID_FUN:
	ttype(o)=LUA_T_PROTO;
	tfvalue(o)=NULL;
	break;
#ifdef DEBUG
   default:				/* cannot happen */
	luaL_verror("internal error in LoadConstants: "
		"bad constant #%d type=%d ('%c')\n",i,c,c);
	break;
#endif
  }
 }
}

static TProtoFunc* LoadFunction(Sundump* S);

static void LoadFunctions(TProtoFunc* tf, Sundump* S)
{
 while (zgetc(S->Z)==ID_FUNCTION)
 {
  int i=LoadWord(S->Z); 
  TProtoFunc* t=LoadFunction(S);
  TObject* o=tf->consts+i;
  tfvalue(o)=t;
 }
}

static TProtoFunc* LoadFunction(Sundump* S)
{
 ZIO* Z=S->Z;
 TProtoFunc* tf=luaF_newproto();
 tf->lineDefined=LoadWord(Z);
 tf->fileName=LoadTString(Z);
 tf->code=LoadBlock(LoadSize(Z),Z);
 LoadConstants(tf,S);
 LoadLocals(tf,Z);
 LoadFunctions(tf,S);
 return tf;
}

static void LoadSignature(ZIO* Z)
{
 char* s=SIGNATURE;
 while (*s!=0 && ezgetc(Z)==*s)
  ++s;
 if (*s!=0) luaL_verror("bad signature in binary file %s",Z->name);
}

static void LoadHeader(Sundump* S)
{
 ZIO* Z=S->Z;
 int version,sizeofR;
 LoadSignature(Z);
 version=ezgetc(Z);
 if (version>VERSION)
  luaL_verror(
	"binary file %s too new: version=0x%02x; expected at most 0x%02x",
	Z->name,version,VERSION);
 if (version<0x31)			/* major change in 3.1 */
  luaL_verror(
	"binary file %s too old: version=0x%02x; expected at least 0x%02x",
	Z->name,version,0x31);
 sizeofR=ezgetc(Z);			/* test float representation */
 if (sizeofR==sizeof(float))
 {
  float f,tf=TEST_FLOAT;
  ezread(Z,&f,sizeof(f));
  if (f!=tf)
  {
   SwapFloat(&f);
   if (f!=tf)
    luaL_verror("unknown float representation in binary file %s",Z->name);
   S->SwapNumber=1;
  }
  S->LoadFloat=1;
 }
 else if (sizeofR==sizeof(double))
 {
  double f,tf=TEST_FLOAT;
  ezread(Z,&f,sizeof(f));
  if (f!=tf)
  {
   SwapDouble(&f);
   if (f!=tf)
    luaL_verror("unknown float representation in binary file %s",Z->name);
   S->SwapNumber=1;
  }
  S->LoadFloat=0;
 }
 else
  luaL_verror(
       "floats in binary file %s have %d bytes; "
       "expected %d (float) or %d (double)",
       Z->name,sizeofR,sizeof(float),sizeof(double));
}

static TProtoFunc* LoadChunk(Sundump* S)
{
 LoadHeader(S);
 return LoadFunction(S);
}

/*
** load one chunk from a file or buffer
** return main if ok and NULL at EOF
*/
TProtoFunc* luaU_undump1(ZIO* Z)
{
 int c=zgetc(Z);
 Sundump S;
 S.Z=Z;
 S.SwapNumber=0;
 S.LoadFloat=1;
 if (c==ID_CHUNK)
  return LoadChunk(&S);
 else if (c!=EOZ)
  luaL_verror("%s is not a lua binary file",Z->name);
 return NULL;
}
