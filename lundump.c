/*
** $Id: lundump.c,v 1.14 1999/09/06 13:55:09 roberto Exp roberto $
** load bytecodes from files
** See Copyright Notice in lua.h
*/

#define LUA_REENTRANT

#include <stdio.h>
#include <string.h>
#include "lauxlib.h"
#include "lfunc.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#define	LoadBlock(L, b,size,Z)	ezread(L, Z,b,size)

static void unexpectedEOZ (lua_State *L, ZIO* Z)
{
 luaL_verror(L, "unexpected end of file in %s",zname(Z));
}

static int ezgetc (lua_State *L, ZIO* Z)
{
 int c=zgetc(Z);
 if (c==EOZ) unexpectedEOZ(L, Z);
 return c;
}

static void ezread (lua_State *L, ZIO* Z, void* b, int n)
{
 int r=zread(Z,b,n);
 if (r!=0) unexpectedEOZ(L, Z);
}

static unsigned int LoadWord (lua_State *L, ZIO* Z)
{
 unsigned int hi=ezgetc(L, Z);
 unsigned int lo=ezgetc(L, Z);
 return (hi<<8)|lo;
}

static unsigned long LoadLong (lua_State *L, ZIO* Z)
{
 unsigned long hi=LoadWord(L, Z);
 unsigned long lo=LoadWord(L, Z);
 return (hi<<16)|lo;
}

/*
* convert number from text
*/
real luaU_str2d (lua_State *L, const char* b, const char* where)
{
 real x;
 if (!luaO_str2d(b, &x))
   luaL_verror(L, "cannot convert number '%s' in %s",b,where);
 return x;
}

static real LoadNumber (lua_State *L, ZIO* Z, int native)
{
 real x;
 if (native)
 {
  LoadBlock(L, &x,sizeof(x),Z);
  return x;
 }
 else
 {
  char b[256];
  int size=ezgetc(L, Z);
  LoadBlock(L, b,size,Z);
  b[size]=0;
  return luaU_str2d(L, b,zname(Z));
 }
}

static int LoadInt (lua_State *L, ZIO* Z, const char* message)
{
 unsigned long l=LoadLong(L, Z);
 unsigned int i=l;
 if (i!=l) luaL_verror(L, message,l,zname(Z));
 return i;
}

#define PAD	5			/* two word operands plus opcode */

static Byte* LoadCode (lua_State *L, ZIO* Z)
{
 int size=LoadInt(L, Z,"code too long (%ld bytes) in %s");
 Byte* b=luaM_malloc(L, size+PAD);
 LoadBlock(L, b,size,Z);
 if (b[size-1]!=ENDCODE) luaL_verror(L, "bad code in %s",zname(Z));
 memset(b+size,ENDCODE,PAD);		/* pad code for safety */
 return b;
}

static TaggedString* LoadTString (lua_State *L, ZIO* Z)
{
 long size=LoadLong(L, Z);
 if (size==0)
  return NULL;
 else
 {
  char* s=luaL_openspace(L, size);
  LoadBlock(L, s,size,Z);
  return luaS_newlstr(L, s,size-1);
 }
}

static void LoadLocals (lua_State *L, TProtoFunc* tf, ZIO* Z)
{
 int i,n=LoadInt(L, Z,"too many locals (%ld) in %s");
 if (n==0) return;
 tf->locvars=luaM_newvector(L, n+1,LocVar);
 for (i=0; i<n; i++)
 {
  tf->locvars[i].line=LoadInt(L, Z,"too many lines (%ld) in %s");
  tf->locvars[i].varname=LoadTString(L, Z);
 }
 tf->locvars[i].line=-1;		/* flag end of vector */
 tf->locvars[i].varname=NULL;
}

static TProtoFunc* LoadFunction (lua_State *L, ZIO* Z, int native);

static void LoadConstants (lua_State *L, TProtoFunc* tf, ZIO* Z, int native)
{
 int i,n=LoadInt(L, Z,"too many constants (%ld) in %s");
 tf->nconsts=n;
 if (n==0) return;
 tf->consts=luaM_newvector(L, n,TObject);
 for (i=0; i<n; i++)
 {
  TObject* o=tf->consts+i;
  ttype(o)=-ezgetc(L, Z);			/* ttype(o) is negative - ORDER LUA_T */
  switch (ttype(o))
  {
   case LUA_T_NUMBER:
	nvalue(o)=LoadNumber(L, Z,native);
	break;
   case LUA_T_STRING:
	tsvalue(o)=LoadTString(L, Z);
	break;
   case LUA_T_PROTO:
	tfvalue(o)=LoadFunction(L, Z,native);
	break;
   case LUA_T_NIL:
	break;
   default:				/* cannot happen */
	luaU_badconstant(L, "load",i,o,tf);
	break;
  }
 }
}

static TProtoFunc* LoadFunction (lua_State *L, ZIO* Z, int native)
{
 TProtoFunc* tf=luaF_newproto(L);
 tf->lineDefined=LoadInt(L, Z,"lineDefined too large (%ld) in %s");
 tf->source=LoadTString(L, Z);
 if (tf->source==NULL) tf->source=luaS_new(L, zname(Z));
 tf->code=LoadCode(L, Z);
 LoadLocals(L, tf,Z);
 LoadConstants(L, tf,Z,native);
 return tf;
}

static void LoadSignature (lua_State *L, ZIO* Z)
{
 const char* s=SIGNATURE;
 while (*s!=0 && ezgetc(L, Z)==*s)
  ++s;
 if (*s!=0) luaL_verror(L, "bad signature in %s",zname(Z));
}

static int LoadHeader (lua_State *L, ZIO* Z)
{
 int version,sizeofR;
 int native;
 LoadSignature(L, Z);
 version=ezgetc(L, Z);
 if (version>VERSION)
  luaL_verror(L, 
	"%s too new: version=0x%02x; expected at most 0x%02x",
	zname(Z),version,VERSION);
 if (version<VERSION0)			/* check last major change */
  luaL_verror(L, 
	"%s too old: version=0x%02x; expected at least 0x%02x",
	zname(Z),version,VERSION0);
 sizeofR=ezgetc(L, Z);
 native=(sizeofR!=0);
 if (native)				/* test number representation */
 {
  if (sizeofR!=sizeof(real))
   luaL_verror(L, "unknown number size in %s: read %d; expected %d",
	 zname(Z),sizeofR,sizeof(real));
  else
  {
   real tf=TEST_NUMBER;
   real f=LoadNumber(L, Z,native);
   if ((long)f!=(long)tf)
    luaL_verror(L, "unknown number format in %s: "
	  "read " NUMBER_FMT "; expected " NUMBER_FMT,
	  zname(Z),f,tf);
  }
 }
 return native;
}

static TProtoFunc* LoadChunk (lua_State *L, ZIO* Z)
{
 return LoadFunction(L, Z,LoadHeader(L, Z));
}

/*
** load one chunk from a file or buffer
** return main if ok and NULL at EOF
*/
TProtoFunc* luaU_undump1 (lua_State *L, ZIO* Z)
{
 int c=zgetc(Z);
 if (c==ID_CHUNK)
  return LoadChunk(L, Z);
 else if (c!=EOZ)
  luaL_verror(L, "%s is not a Lua binary file",zname(Z));
 return NULL;
}

/*
* handle constants that cannot happen
*/
void luaU_badconstant (lua_State *L, const char* s, int i, const TObject* o, TProtoFunc* tf)
{
 int t=ttype(o);
 const char* name= (t>0 || t<LUA_T_LINE) ? "?" : luaO_typenames[-t];
 luaL_verror(L, "cannot %s constant #%d: type=%d [%s]" IN,s,i,t,name,INLOC);
}
