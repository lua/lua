/*
** $Id: lundump.c,v 1.8 1999/03/30 20:29:34 roberto Exp roberto $
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

#define	LoadBlock(b,size,Z)	ezread(Z,b,size)
#define	LoadNative(t,Z)		LoadBlock(&t,sizeof(t),Z)

#if ID_NUMBER==ID_NATIVE
	#define doLoadNumber(f,Z)	LoadNative(f,Z)
#else
	#define doLoadNumber(f,Z)	f=LoadNumber(Z)
#endif

static void unexpectedEOZ (ZIO* Z)
{
 luaL_verror("unexpected end of file in %s",zname(Z));
}

static int ezgetc (ZIO* Z)
{
 int c=zgetc(Z);
 if (c==EOZ) unexpectedEOZ(Z);
 return c;
}

static void ezread (ZIO* Z, void* b, int n)
{
 int r=zread(Z,b,n);
 if (r!=0) unexpectedEOZ(Z);
}

static unsigned int LoadWord (ZIO* Z)
{
 unsigned int hi=ezgetc(Z);
 unsigned int lo=ezgetc(Z);
 return (hi<<8)|lo;
}

static unsigned long LoadLong (ZIO* Z)
{
 unsigned long hi=LoadWord(Z);
 unsigned long lo=LoadWord(Z);
 return (hi<<16)|lo;
}

#if ID_NUMBER==ID_REAL4			/* LUA_NUMBER */
/* assumes sizeof(long)==4 and sizeof(float)==4 (IEEE) */
static float LoadFloat (ZIO* Z)
{
 unsigned long l=LoadLong(Z);
 float f;
 memcpy(&f,&l,sizeof(f));
 return f;
}
#endif

#if ID_NUMBER==ID_REAL8			/* LUA_NUMBER */
/* assumes sizeof(long)==4 and sizeof(double)==8 (IEEE) */
static double LoadDouble (ZIO* Z)
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

static int LoadInt (ZIO* Z, char* message)
{
 unsigned long l=LoadLong(Z);
 unsigned int i=l;
 if (i!=l) luaL_verror(message,l,zname(Z));
 return i;
}

#define PAD	5			/* two word operands plus opcode */

static Byte* LoadCode (ZIO* Z)
{
 int size=LoadInt(Z,"code too long (%ld bytes) in %s");
 Byte* b=luaM_malloc(size+PAD);
 LoadBlock(b,size,Z);
 if (b[size-1]!=ENDCODE) luaL_verror("bad code in %s",zname(Z));
 memset(b+size,ENDCODE,PAD);		/* pad for safety */
 return b;
}

static TaggedString* LoadTString (ZIO* Z)
{
 long size=LoadLong(Z);
 if (size==0)
  return NULL;
 else
 {
  char* s=luaL_openspace(size);
  LoadBlock(s,size,Z);
  return luaS_newlstr(s,size-1);
 }
}

static void LoadLocals (TProtoFunc* tf, ZIO* Z)
{
 int i,n=LoadInt(Z,"too many locals (%ld) in %s");
 if (n==0) return;
 tf->locvars=luaM_newvector(n+1,LocVar);
 for (i=0; i<n; i++)
 {
  tf->locvars[i].line=LoadInt(Z,"too many lines (%ld) in %s");
  tf->locvars[i].varname=LoadTString(Z);
 }
 tf->locvars[i].line=-1;		/* flag end of vector */
 tf->locvars[i].varname=NULL;
}

static TProtoFunc* LoadFunction (ZIO* Z);

static void LoadConstants (TProtoFunc* tf, ZIO* Z)
{
 int i,n=LoadInt(Z,"too many constants (%ld) in %s");
 tf->nconsts=n;
 if (n==0) return;
 tf->consts=luaM_newvector(n,TObject);
 for (i=0; i<n; i++)
 {
  TObject* o=tf->consts+i;
  ttype(o)=-ezgetc(Z);			/* ttype(o) is negative - ORDER LUA_T */
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
   default:				/* cannot happen */
	luaU_badconstant("load",i,o,tf);
	break;
  }
 }
}

static TProtoFunc* LoadFunction (ZIO* Z)
{
 TProtoFunc* tf=luaF_newproto();
 tf->lineDefined=LoadInt(Z,"lineDefined too large (%ld) in %s");
 tf->source=LoadTString(Z);
 if (tf->source==NULL) tf->source=luaS_new(zname(Z));
 tf->code=LoadCode(Z);
 LoadLocals(tf,Z);
 LoadConstants(tf,Z);
 return tf;
}

static void LoadSignature (ZIO* Z)
{
 char* s=SIGNATURE;
 while (*s!=0 && ezgetc(Z)==*s)
  ++s;
 if (*s!=0) luaL_verror("bad signature in %s",zname(Z));
}

static void LoadHeader (ZIO* Z)
{
 int version,id,sizeofR;
 real f=-TEST_NUMBER,tf=TEST_NUMBER;
 luaU_testnumber();
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
  luaL_verror("unknown number signature in %s: "
	"read 0x%02x%02x; expected 0x%02x%02x",
	zname(Z),id,sizeofR,ID_NUMBER,sizeof(real));
 doLoadNumber(f,Z);
 if (f!=tf)
  luaL_verror("unknown number representation in %s: "
	"read " NUMBER_FMT "; expected " NUMBER_FMT,
	zname(Z),f,tf);
}

static TProtoFunc* LoadChunk (ZIO* Z)
{
 LoadHeader(Z);
 return LoadFunction(Z);
}

/*
** load one chunk from a file or buffer
** return main if ok and NULL at EOF
*/
TProtoFunc* luaU_undump1 (ZIO* Z)
{
 int c=zgetc(Z);
 if (c==ID_CHUNK)
  return LoadChunk(Z);
 else if (c!=EOZ)
  luaL_verror("%s is not a Lua binary file",zname(Z));
 return NULL;
}

/*
** test number representation
*/
void luaU_testnumber (void)
{
 if (sizeof(real)!=SIZEOF_NUMBER)
  luaL_verror("numbers have %d bytes; expected %d. see lundump.h",
	(int)sizeof(real),SIZEOF_NUMBER);
#if ID_NUMBER==ID_REAL4 || ID_NUMBER==ID_REAL8
 if (sizeof(long)!=4)
  luaL_verror("longs have %d bytes; expected %d. see lundump.h",
	(int)sizeof(long),4);
#endif
 {
  real t=TEST_NUMBER;
  TYPEOF_NUMBER v=TEST_NUMBER;
  if (t!=v)
   luaL_verror("unsupported number type; expected %d-byte " NAMEOF_NUMBER "."
	" see config and lundump.h",SIZEOF_NUMBER);
 }
}

/*
* handle constants that cannot happen
*/
void luaU_badconstant (char* s, int i, TObject* o, TProtoFunc* tf)
{
 int t=ttype(o);
 char* name= (t>0 || t<LUA_T_LINE) ? "?" : luaO_typenames[-t];
 luaL_verror("cannot %s constant #%d: type=%d [%s]" IN,s,i,t,name,INLOC);
}
