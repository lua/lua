/*
** $Id: luac.c,v 1.10 1998/07/12 00:38:30 lhf Exp $
** lua compiler (saves bytecodes to files; also list binary files)
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "luac.h"
#include "lparser.h"
#include "lzio.h"
#include "luadebug.h"

#define	OUTPUT	"luac.out"		/* default output file */

extern void DumpChunk(TProtoFunc* Main, FILE* D);
extern void PrintChunk(TProtoFunc* Main);
extern void OptChunk(TProtoFunc* Main);

static FILE* efopen(char* name, char* mode);
static void doit(int undump, char* filename);

static int listing=0;			/* list bytecodes? */
static int debugging=0;			/* debug? */
static int dumping=1;			/* dump bytecodes? */
static int undumping=0;			/* undump bytecodes? */
static int optimizing=0;		/* optimize? */
static int parsing=0;			/* parse only? */
static int verbose=0;			/* tell user what is done */
static FILE* D;				/* output file */

static void usage(void)
{
 fprintf(stderr,"usage: "
 "luac [-c | -u] [-D name] [-d] [-l] [-o output] [-O] [-p] [-q] [-v] [-V] [files]\n"
 " -c\tcompile (default)\n"
 " -u\tundump\n"
 " -d\tgenerate debugging information\n"
 " -D\tpredefine symbol for conditional compilation\n"
 " -l\tlist (default for -u)\n"
 " -o\toutput file for -c (default is \"" OUTPUT "\")\n"
 " -O\toptimize\n"
 " -p\tparse only\n"
 " -q\tquiet (default for -c)\n"
 " -v\tshow version information\n"
 " -V\tverbose\n"
 " -\tcompile \"stdin\"\n"
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
  else if (IS("-"))			/* use stdin */
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
  else if (IS("-u"))			/* undump */
  {
   dumping=0;
   undumping=1;
   listing=1;
  }
  else if (IS("-v"))			/* show version */
   printf("%s  %s\n(written by %s)\n\n",LUA_VERSION,LUA_COPYRIGHT,LUA_AUTHORS);
  else if (IS("-V"))			/* verbose */
   verbose=1;
  else					/* unknown option */
   usage();
 }
 --i;					/* fake new argv[0] */
 argc-=i;
 argv+=i;
 if (dumping || parsing)
 {
  if (argc<2) usage();
  if (dumping)
  {
   for (i=1; i<argc; i++)		/* play safe with output file */
    if (IS(d)) luaL_verror("will not overwrite input file \"%s\"",d);
   D=efopen(d,"wb");			/* must open in binary mode */
#if ID_NUMBER==ID_NATIVE
   if (verbose) fprintf(stderr,"luac: warning: "
	"saving numbers in native format. file may not be portable.\n");
#endif
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
 if (optimizing) lua_debug=0;		/* set debugging before parsing */
 if (debugging)  lua_debug=1;
 Main=luaY_parser(z);
 if (optimizing) OptChunk(Main);
 if (listing) PrintChunk(Main);
 if (dumping) DumpChunk(Main,D);
}

static void do_undump(ZIO* z)
{
 while (1)
 {
  TProtoFunc* Main=luaU_undump1(z);
  if (Main==NULL) break;
  if (optimizing) OptChunk(Main);
  if (listing) PrintChunk(Main);
 }
}

static void doit(int undump, char* filename)
{
 FILE* f;
 ZIO z;
 if (filename==NULL)
 {
  f=stdin; filename="(stdin)";
 }
 else
 {
  f=efopen(filename, undump ? "rb" : "r");
 }
 zFopen(&z,f,filename);
 if (verbose) fprintf(stderr,"%s\n",filename);
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
