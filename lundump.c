/*
** $Id: lundump.c,v 2.27 2014/02/27 18:56:15 roberto Exp roberto $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#include <string.h>

#define lundump_c
#define LUA_CORE

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstring.h"
#include "lundump.h"
#include "lzio.h"

typedef struct {
 lua_State* L;
 ZIO* Z;
 Mbuffer* b;
 const char* name;
} LoadState;

static l_noret error(LoadState* S, const char* why)
{
 luaO_pushfstring(S->L,"%s: %s precompiled chunk",S->name,why);
 luaD_throw(S->L,LUA_ERRSYNTAX);
}

#define LoadMem(S,b,n,size)	LoadBlock(S,b,(n)*(size))
#define LoadVar(S,x)		LoadMem(S,&x,1,sizeof(x))
#define LoadVector(S,b,n,size)	LoadMem(S,b,n,size)

#if !defined(luai_verifycode)
#define luai_verifycode(L,b,f)	/* empty */
#endif

static void LoadBlock(LoadState* S, void* b, size_t size)
{
 if (luaZ_read(S->Z,b,size)!=0) error(S,"truncated");
}

static lu_byte LoadByte(LoadState* S)
{
 lu_byte x;
 LoadVar(S,x);
 return x;
}

static int LoadInt(LoadState* S)
{
 int x;
 LoadVar(S,x);
 if (x<0) error(S,"corrupted");
 return x;
}

static lua_Number LoadNumber(LoadState* S)
{
 lua_Number x;
 LoadVar(S,x);
 return x;
}

static lua_Integer LoadInteger(LoadState* S)
{
 lua_Integer x;
 LoadVar(S,x);
 return x;
}

static TString* LoadString(LoadState* S)
{
 size_t size;
 LoadVar(S,size);
 if (size==0)
  return NULL;
 else
 {
  char* s=luaZ_openspace(S->L,S->b,size);
  LoadBlock(S,s,size*sizeof(char));
  return luaS_newlstr(S->L,s,size-1);		/* remove trailing '\0' */
 }
}

static void LoadCode(LoadState* S, Proto* f)
{
 int n=LoadInt(S);
 f->code=luaM_newvector(S->L,n,Instruction);
 f->sizecode=n;
 LoadVector(S,f->code,n,sizeof(Instruction));
}

static void LoadFunction(LoadState* S, Proto* f);

static void LoadConstants(LoadState* S, Proto* f)
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
   case LUA_TNIL:
	setnilvalue(o);
	break;
   case LUA_TBOOLEAN:
	setbvalue(o,LoadByte(S));
	break;
   case LUA_TNUMFLT:
	setnvalue(o,LoadNumber(S));
	break;
   case LUA_TNUMINT:
	setivalue(o,LoadInteger(S));
	break;
   case LUA_TSHRSTR: case LUA_TLNGSTR:
	setsvalue2n(S->L,o,LoadString(S));
	break;
    default: lua_assert(0);
  }
 }
 n=LoadInt(S);
 f->p=luaM_newvector(S->L,n,Proto*);
 f->sizep=n;
 for (i=0; i<n; i++) f->p[i]=NULL;
 for (i=0; i<n; i++)
 {
  f->p[i]=luaF_newproto(S->L);
  LoadFunction(S,f->p[i]);
 }
}

static void LoadUpvalues(LoadState* S, Proto* f)
{
 int i,n;
 n=LoadInt(S);
 f->upvalues=luaM_newvector(S->L,n,Upvaldesc);
 f->sizeupvalues=n;
 for (i=0; i<n; i++) f->upvalues[i].name=NULL;
 for (i=0; i<n; i++)
 {
  f->upvalues[i].instack=LoadByte(S);
  f->upvalues[i].idx=LoadByte(S);
 }
}

static void LoadDebug(LoadState* S, Proto* f)
{
 int i,n;
 f->source=LoadString(S);
 n=LoadInt(S);
 f->lineinfo=luaM_newvector(S->L,n,int);
 f->sizelineinfo=n;
 LoadVector(S,f->lineinfo,n,sizeof(int));
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
 n=LoadInt(S);
 for (i=0; i<n; i++) f->upvalues[i].name=LoadString(S);
}

static void LoadFunction(LoadState* S, Proto* f)
{
 f->linedefined=LoadInt(S);
 f->lastlinedefined=LoadInt(S);
 f->numparams=LoadByte(S);
 f->is_vararg=LoadByte(S);
 f->maxstacksize=LoadByte(S);
 LoadCode(S,f);
 LoadConstants(S,f);
 LoadUpvalues(S,f);
 LoadDebug(S,f);
}

static void checkstring(LoadState *S, const char *s, const char *msg)
{
 char buff[sizeof(LUA_SIGNATURE)+sizeof(LUAC_DATA)];  /* larger than each */
 LoadMem(S,buff,strlen(s)+1,sizeof(char));
 if (strcmp(s,buff)!=0) error(S,msg);
}

static void fchecksize(LoadState *S, size_t size, const char *tname)
{
 if (LoadByte(S) != size)
  error(S,luaO_pushfstring(S->L,"%s size mismatch in",tname));
}

#define checksize(S,t)	fchecksize(S,sizeof(t),#t)

static void checkHeader(LoadState* S)
{
 checkstring(S,LUA_SIGNATURE+1,"not a");
 checkstring(S,LUAC_DATA,"corrupted");
 if (LoadByte(S) != LUAC_VERSION) error(S,"version mismatch in");
 if (LoadByte(S) != LUAC_FORMAT) error(S,"format mismatch in");
 checksize(S,int);
 checksize(S,size_t);
 checksize(S,Instruction);
 checksize(S,lua_Integer);
 checksize(S,lua_Number);
 if (LoadInteger(S) != LUAC_INT) error(S,"endianess mismatch in");
 if (LoadNumber(S) != LUAC_NUM) error(S,"float format mismatch in");
}

/*
** load precompiled chunk
*/
Closure* luaU_undump (lua_State* L, ZIO* Z, Mbuffer* buff, const char* name)
{
 LoadState S;
 Closure* cl;
 if (*name=='@' || *name=='=')
  S.name=name+1;
 else if (*name==LUA_SIGNATURE[0])
  S.name="binary string";
 else
  S.name=name;
 S.L=L;
 S.Z=Z;
 S.b=buff;
 checkHeader(&S);
 cl=luaF_newLclosure(L,LoadByte(&S));
 setclLvalue(L,L->top,cl); incr_top(L);
 cl->l.p=luaF_newproto(L);
 LoadFunction(&S,cl->l.p);
 lua_assert(cl->l.nupvalues==cl->l.p->sizeupvalues);
 luai_verifycode(L,buff,cl->l.p);
 return cl;
}
