/*
** $Id: luac.c,v 1.28 2000/11/06 20:06:27 lhf Exp $
** lua compiler (saves bytecodes to files; also list binary files)
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lparser.h"
#include "lstate.h"
#include "lzio.h"
#include "luac.h"

#define	OUTPUT	"luac.out"		/* default output file */

static void usage(const char* message, const char* arg);
static int doargs(int argc, const char* argv[]);
static Proto* load(const char* filename);
static FILE* efopen(const char* name, const char* mode);
static void strip(Proto* tf);
static Proto* combine(Proto** P, int n);

lua_State* lua_state=NULL;		/* lazy! */

static int listing=0;			/* list bytecodes? */
static int dumping=1;			/* dump bytecodes? */
static int stripping=0;			/* strip debug information? */
static int testing=0;			/* test integrity? */
static const char* output=OUTPUT;	/* output file name */

#define	IS(s)	(strcmp(argv[i],s)==0)

int main(int argc, const char* argv[])
{
 Proto** P,*tf;
 int i=doargs(argc,argv);
 argc-=i; argv+=i;
 if (argc<=0) usage("no input files given",NULL);
 L=lua_open(0);
 P=luaM_newvector(L,argc,Proto*);
 for (i=0; i<argc; i++)
  P[i]=load(IS("-")? NULL : argv[i]);
 tf=combine(P,argc);
 if (dumping) luaU_optchunk(tf);
 if (listing) luaU_printchunk(tf);
 if (testing) luaU_testchunk(tf);
 if (dumping)
 {
  if (stripping) strip(tf);
  luaU_dumpchunk(tf,efopen(output,"wb"));
 }
 return 0;
}

static void usage(const char* message, const char* arg)
{
 if (message!=NULL)
 {
  fprintf(stderr,"luac: "); fprintf(stderr,message,arg); fprintf(stderr,"\n");
 }
 fprintf(stderr,
 "usage: luac [options] [filenames].  Available options are:\n"
 "  -        process stdin\n"
 "  -l       list\n"
 "  -o file  output file (default is \"" OUTPUT "\")\n"
 "  -p       parse only\n"
 "  -s       strip debug information\n"
 "  -t       test code integrity\n"
 "  -v       show version information\n"
 );
 exit(1);
}

static int doargs(int argc, const char* argv[])
{
 int i;
 for (i=1; i<argc; i++)
 {
  if (*argv[i]!='-')			/* end of options */
   break;
  else if (IS("-"))			/* end of options; use stdin */
   return i;
  else if (IS("-l"))			/* list */
   listing=1;
  else if (IS("-o"))			/* output file */
  {
   output=argv[++i];
   if (output==NULL) usage(NULL,NULL);
  }
  else if (IS("-p"))			/* parse only */
   dumping=0;
  else if (IS("-s"))			/* strip debug information */
   stripping=1;
  else if (IS("-t"))			/* test */
  {
   testing=1;
   dumping=0;
  }
  else if (IS("-v"))			/* show version */
  {
   printf("%s  %s\n",LUA_VERSION,LUA_COPYRIGHT);
   if (argc==2) exit(0);
  }
  else					/* unknown option */
   usage("unrecognized option `%s'",argv[i]);
 }
 if (i==argc && (listing || testing))
 {
  dumping=0;
  argv[--i]=OUTPUT;
 }
 return i;
}

static Proto* load(const char* filename)
{
 Proto* tf;
 ZIO z;
 char source[512];
 FILE* f;
 int c,undump;
 if (filename==NULL) 
 {
  f=stdin;
  filename="(stdin)";
 }
 else
  f=efopen(filename,"r");
 c=ungetc(fgetc(f),f);
 if (ferror(f))
 {
  fprintf(stderr,"luac: cannot read from ");
  perror(filename);
  exit(1);
 }
 undump=(c==ID_CHUNK);
 if (undump && f!=stdin)
 {
  fclose(f);
  f=efopen(filename,"rb");
 }
 sprintf(source,"@%.*s",Sizeof(source)-2,filename);
 luaZ_Fopen(&z,f,source);
 tf = undump ? luaU_undump(L,&z) : luaY_parser(L,&z);
 if (f!=stdin) fclose(f);
 return tf;
}

static Proto* combine(Proto** P, int n)
{
 if (n==1)
  return P[0];
 else
 {
  int i,pc=0;
  Proto* tf=luaF_newproto(L);
  tf->source=luaS_new(L,"=(luac)");
  tf->maxstacksize=1;
  tf->kproto=P;
  tf->nkproto=n;
  tf->ncode=2*n+1;
  tf->code=luaM_newvector(L,tf->ncode,Instruction);
  for (i=0; i<n; i++)
  {
   tf->code[pc++]=CREATE_AB(OP_CLOSURE,i,0);
   tf->code[pc++]=CREATE_AB(OP_CALL,0,0);
  }
  tf->code[pc++]=OP_END;
  return tf;
 }
}

static void strip(Proto* tf)
{
 int i,n=tf->nkproto;
 tf->lineinfo=NULL;
 tf->nlineinfo=0;
 tf->source=luaS_new(L,"=(none)");
 tf->locvars=NULL;
 tf->nlocvars=0;
 for (i=0; i<n; i++) strip(tf->kproto[i]);
}

static FILE* efopen(const char* name, const char* mode)
{
 FILE* f=fopen(name,mode);
 if (f==NULL)
 {
  fprintf(stderr,"luac: cannot open %sput file ",*mode=='r' ? "in" : "out");
  perror(name);
  exit(1);
 }
 return f; 
}

void luaU_testchunk(const Proto* Main)
{
 UNUSED(Main);
 fprintf(stderr,"luac: -t not operational in this version\n");
 exit(1);
}
