/*
** $Id: luac.c,v 1.44 2003/04/07 20:34:20 lhf Exp $
** Lua compiler (saves bytecodes to files; also list bytecodes)
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#ifndef LUA_DEBUG
#define luaB_opentests(L)
#endif

#ifndef PROGNAME
#define PROGNAME	"luac"		/* program name */
#endif

#define	OUTPUT		"luac.out"	/* default output file */

static int listing=0;			/* list bytecodes? */
static int dumping=1;			/* dump bytecodes? */
static int stripping=0;			/* strip debug information? */
static char Output[]={ OUTPUT };	/* default output file name */
static const char* output=Output;	/* output file name */
static const char* progname=PROGNAME;	/* actual program name */

static void fatal(const char* message)
{
 fprintf(stderr,"%s: %s\n",progname,message);
 exit(EXIT_FAILURE);
}

static void cannot(const char* name, const char* what, const char* mode)
{
 fprintf(stderr,"%s: cannot %s %sput file ",progname,what,mode);
 perror(name);
 exit(EXIT_FAILURE);
}

static void usage(const char* message, const char* arg)
{
 if (message!=NULL)
 {
  fprintf(stderr,"%s: ",progname); fprintf(stderr,message,arg); fprintf(stderr,"\n");
 }
 fprintf(stderr,
 "usage: %s [options] [filenames].  Available options are:\n"
 "  -        process stdin\n"
 "  -l       list\n"
 "  -o name  output to file `name' (default is \"" OUTPUT "\")\n"
 "  -p       parse only\n"
 "  -s       strip debug information\n"
 "  -v       show version information\n"
 "  --       stop handling options\n",
 progname);
 exit(EXIT_FAILURE);
}

#define	IS(s)	(strcmp(argv[i],s)==0)

static int doargs(int argc, char* argv[])
{
 int i;
 if (argv[0]!=NULL && *argv[0]!=0) progname=argv[0];
 for (i=1; i<argc; i++)
 {
  if (*argv[i]!='-')			/* end of options; keep it */
   break;
  else if (IS("--"))			/* end of options; skip it */
  {
   ++i;
   break;
  }
  else if (IS("-"))			/* end of options; use stdin */
   return i;
  else if (IS("-l"))			/* list */
   listing=1;
  else if (IS("-o"))			/* output file */
  {
   output=argv[++i];
   if (output==NULL || *output==0) usage("`-o' needs argument",NULL);
  }
  else if (IS("-p"))			/* parse only */
   dumping=0;
  else if (IS("-s"))			/* strip debug information */
   stripping=1;
  else if (IS("-v"))			/* show version */
  {
   printf("%s  %s\n",LUA_VERSION,LUA_COPYRIGHT);
   if (argc==2) exit(EXIT_SUCCESS);
  }
  else					/* unknown option */
   usage("unrecognized option `%s'",argv[i]);
 }
 if (i==argc && (listing || !dumping))
 {
  dumping=0;
  argv[--i]=Output;
 }
 return i;
}

static Proto* toproto(lua_State* L, int i)
{
 const Closure* c=(const Closure*)lua_topointer(L,i);
 return c->l.p;
}

static Proto* combine(lua_State* L, int n)
{
 if (n==1)
  return toproto(L,-1);
 else
 {
  int i,pc=0;
  Proto* f=luaF_newproto(L);
  f->source=luaS_newliteral(L,"=(" PROGNAME ")");
  f->maxstacksize=1;
  f->p=luaM_newvector(L,n,Proto*);
  f->sizep=n;
  f->sizecode=2*n+1;
  f->code=luaM_newvector(L,f->sizecode,Instruction);
  for (i=0; i<n; i++)
  {
   f->p[i]=toproto(L,i-n);
   f->code[pc++]=CREATE_ABx(OP_CLOSURE,0,i);
   f->code[pc++]=CREATE_ABC(OP_CALL,0,1,1);
  }
  f->code[pc++]=CREATE_ABC(OP_RETURN,0,1,0);
  return f;
 }
}

static void strip(lua_State* L, Proto* f)
{
 int i,n=f->sizep;
 luaM_freearray(L, f->lineinfo, f->sizelineinfo, int);
 luaM_freearray(L, f->locvars, f->sizelocvars, struct LocVar);
 luaM_freearray(L, f->upvalues, f->sizeupvalues, TString *);
 f->lineinfo=NULL; f->sizelineinfo=0;
 f->locvars=NULL;  f->sizelocvars=0;
 f->upvalues=NULL; f->sizeupvalues=0;
 f->source=luaS_newliteral(L,"=(none)");
 for (i=0; i<n; i++) strip(L,f->p[i]);
}

static int writer(lua_State* L, const void* p, size_t size, void* u)
{
 UNUSED(L);
 return fwrite(p,size,1,(FILE*)u)==1;
}

int main(int argc, char* argv[])
{
 lua_State* L;
 Proto* f;
 int i=doargs(argc,argv);
 argc-=i; argv+=i;
 if (argc<=0) usage("no input files given",NULL);
 L=lua_open();
 luaB_opentests(L);
 for (i=0; i<argc; i++)
 {
  const char* filename=IS("-") ? NULL : argv[i];
  if (luaL_loadfile(L,filename)!=0) fatal(lua_tostring(L,-1));
 }
 f=combine(L,argc);
 if (listing) luaU_print(f);
 if (dumping)
 {
  FILE* D=fopen(output,"wb");
  if (D==NULL) cannot(output,"open","out");
  if (stripping) strip(L,f);
  luaU_dump(L,f,writer,D);
  if (ferror(D)) cannot(output,"write","out");
  fclose(D);
 }
 lua_close(L);
 return 0;
}
