/*
** $Id: luac.c,v 1.17 1999/07/02 19:34:26 lhf Exp $
** lua compiler (saves bytecodes to files; also list binary files)
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "luac.h"
#include "lparser.h"
#include "lstate.h"
#include "lzio.h"

#define	OUTPUT	"luac.out"		/* default output file */

static FILE* efopen(char* name, char* mode);
static void doit(int undump, char* filename);

static int listing=0;			/* list bytecodes? */
static int debugging=0;			/* emit debug information? */
static int dumping=1;			/* dump bytecodes? */
static int undumping=0;			/* undump bytecodes? */
static int optimizing=0;		/* optimize? */
static int parsing=0;			/* parse only? */
static int testing=0;			/* test integrity? */
static int verbose=0;			/* tell user what is done */
static int native=0;			/* save numbers in native format? */
static FILE* D;				/* output file */

static void usage(char* op)
{
 if (op) fprintf(stderr,"luac: unrecognized option '%s'\n",op);
 fprintf(stderr,
 "usage: luac [options] [filenames].  Available options are:\n"
 " -c\t\tcompile (default)\n"
 " -d\t\tgenerate debugging information\n"
 " -D name\tpredefine 'name' for conditional compilation\n"
 " -l\t\tlist (default for -u)\n"
 " -n\t\tsave numbers in native format (file may not be portable)\n"
 " -o file\toutput file for -c (default is \"" OUTPUT "\")\n"
 " -O\t\toptimize\n"
 " -p\t\tparse only\n"
 " -q\t\tquiet (default for -c)\n"
 " -t\t\ttest code integrity\n"
 " -u\t\tundump\n"
 " -U name\tundefine 'name' for conditional compilation\n"
 " -v\t\tshow version information\n"
 " -V\t\tverbose\n"
 " -\t\tcompile \"stdin\"\n"
 );
 exit(1);
}

#define	IS(s)	(strcmp(argv[i],s)==0)

int main(int argc, char* argv[])
{
 char* d=OUTPUT;			/* output file name */
 int i;
 lua_open();
 for (i=1; i<argc; i++)
 {
  if (argv[i][0]!='-')			/* end of options */
   break;
  else if (IS("-"))			/* end of options; use stdin */
   break;
  else if (IS("-c"))			/* compile (and dump) */
  {
   dumping=1;
   undumping=0;
  }
  else if (IS("-D"))			/* $define */
  {
   TaggedString* s=luaS_new(argv[++i]);
   s->u.s.globalval.ttype=LUA_T_NUMBER;
   s->u.s.globalval.value.n=1;
  }
  else if (IS("-d"))			/* debug */
   debugging=1;
  else if (IS("-l"))			/* list */
   listing=1;
  else if (IS("-n"))			/* native */
   native=1;
  else if (IS("-o"))			/* output file */
   d=argv[++i];
  else if (IS("-O"))			/* optimize */
   optimizing=1; 
  else if (IS("-p"))			/* parse only */
  {
   dumping=0;
   parsing=1;
  }
  else if (IS("-q"))			/* quiet */
   listing=0;
  else if (IS("-t"))			/* test */
   testing=1;
  else if (IS("-u"))			/* undump */
  {
   dumping=0;
   undumping=1;
   listing=1;
  }
  else if (IS("-U"))			/* undefine */
  {
   TaggedString* s=luaS_new(argv[++i]);
   s->u.s.globalval.ttype=LUA_T_NIL;
  }
  else if (IS("-v"))			/* show version */
   printf("%s  %s\n(written by %s)\n\n",LUA_VERSION,LUA_COPYRIGHT,LUA_AUTHORS);
  else if (IS("-V"))			/* verbose */
   verbose=1;
  else					/* unknown option */
   usage(argv[i]);
 }
 --i;					/* fake new argv[0] */
 argc-=i;
 argv+=i;
 if (dumping || parsing)
 {
  if (argc<2) usage(NULL);
  if (dumping)
  {
   for (i=1; i<argc; i++)		/* play safe with output file */
    if (IS(d)) luaL_verror("will not overwrite input file \"%s\"",d);
   D=efopen(d,"wb");			/* must open in binary mode */
  }
  for (i=1; i<argc; i++) doit(0,IS("-")? NULL : argv[i]);
  if (dumping) fclose(D);
 }
 if (undumping)
 {
  if (argc<2)
   doit(1,OUTPUT);
  else
   for (i=1; i<argc; i++) doit(1,IS("-")? NULL : argv[i]);
 }
 return 0;
}

static void do_compile(ZIO* z)
{
 TProtoFunc* Main;
 if (optimizing) L->debug=0;
 if (debugging)  L->debug=1;
 Main=luaY_parser(z);
 if (optimizing) luaU_optchunk(Main);
 if (listing) luaU_printchunk(Main);
 if (testing) luaU_testchunk(Main);
 if (dumping) luaU_dumpchunk(Main,D,native);
}

static void do_undump(ZIO* z)
{
 for (;;)
 {
  TProtoFunc* Main=luaU_undump1(z);
  if (Main==NULL) break;
  if (optimizing) luaU_optchunk(Main);
  if (listing) luaU_printchunk(Main);
  if (testing) luaU_testchunk(Main);
 }
}

static void doit(int undump, char* filename)
{
 FILE* f= (filename==NULL) ? stdin : efopen(filename, undump ? "rb" : "r");
 ZIO z;
 char source[255+2];			/* +2 for '@' and '\0' */
 luaL_filesource(source,filename,sizeof(source));
 zFopen(&z,f,source);
 if (verbose) fprintf(stderr,"%s\n",source+1);
 if (undump) do_undump(&z); else do_compile(&z);
 if (f!=stdin) fclose(f);
}

static FILE* efopen(char* name, char* mode)
{
 FILE* f=fopen(name,mode);
 if (f==NULL)
 {
  fprintf(stderr,"luac: cannot open %sput file ",mode[0]=='r' ? "in" : "out");
  perror(name);
  exit(1);
 }
 return f; 
}
