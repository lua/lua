/*
** luac.c
** lua compiler (saves bytecodes to files; also list binary files)
*/

char* rcs_luac="$Id: luac.c,v 1.23 1997/06/20 20:34:04 lhf Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "luac.h"
#include "lex.h"
#include "zio.h"

static void compile(char* filename);
static void undump(char* filename);

static int listing=0;			/* list bytecodes? */
static int dumping=1;			/* dump bytecodes? */
static int undumping=0;			/* undump bytecodes? */
static FILE* D;				/* output file */

static void usage(void)
{
 fprintf(stderr,
 "usage: luac [-c | -u] [-d] [-l] [-p] [-q] [-v] [-o output] file ...\n"
 " -c\tcompile (default)\n"
 " -u\tundump\n"
 " -d\tgenerate debugging information\n"
 " -l\tlist (default for -u)\n"
 " -o\toutput file for -c (default \"luac.out\")\n"
 " -p\tparse only\n"
 " -q\tquiet (default for -c)\n"
 " -v\tshow version information\n"
 );
 exit(1);
}

#define	IS(s)	(strcmp(argv[i],s)==0)

int main(int argc, char* argv[])
{
 char* d="luac.out";			/* default output file */
 int i;
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
  else if (IS("-d"))			/* debug */
   lua_debug=1;
  else if (IS("-l"))			/* list */
   listing=1;
  else if (IS("-o"))			/* output file */
   d=argv[++i];
  else if (IS("-p"))			/* parse only (for timing purposes) */
   dumping=0;
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
  else					/* unknown option */
   usage();
 }
 --i;					/* fake new argv[0] */
 argc-=i;
 argv+=i;
 if (dumping)
 {
  if (argc<2) usage();
  for (i=1; i<argc; i++)		/* play safe with output file */
   if (IS(d))
   {
    fprintf(stderr,"luac: will not overwrite input file \"%s\"\n",d);
    exit(1);
   }
  D=fopen(d,"wb");			/* must open in binary mode */
  if (D==NULL)
  {
   fprintf(stderr,"luac: cannot open ");
   perror(d);
   exit(1);
  }
  for (i=1; i<argc; i++) compile(IS("-")? NULL : argv[i]);
  fclose(D);
 }
 if (undumping)
 {
  if (argc<2)
   undump("luac.out");
  else
   for (i=1; i<argc; i++) undump(IS("-")? NULL : argv[i]);
 }
 return 0;
}

static void do_dump(TFunc* Main)
{
 TFunc* tf;
 LinkFunctions(Main);
 if (listing)
 {
  for (tf=Main; tf!=NULL; tf=tf->next) PrintFunction(tf,Main);
 }
 if (dumping)
 {
  DumpHeader(D);
  for (tf=Main; tf!=NULL; tf=tf->next) DumpFunction(tf,D);
 }
 for (tf=Main; tf!=NULL; )
 {
  TFunc* nf=tf->next;
  luaI_freefunc(tf);
  tf=nf;
 }
}

static void do_compile(ZIO* z)
{
 TFunc* tf=new(TFunc);
 lua_setinput(z);
 luaI_initTFunc(tf);
 tf->fileName=lua_parsedfile;
 lua_parse(tf);
 do_dump(tf);
}

static void compile(char* filename)
{
 FILE* f= (filename==NULL) ? stdin : fopen(filename, "r");
 if (f==NULL)
 {
  fprintf(stderr,"luac: cannot open ");
  perror(filename);
  exit(1);
 }
 else
 {
  ZIO z;
  zFopen(&z,f);
  luaI_setparsedfile(filename?filename:"(stdin)");
  do_compile(&z);
  fclose(f);
 }
}

static void do_undump(ZIO* z)
{
 TFunc* Main;
 while ((Main=luaI_undump1(z)))
 {
  if (listing)
  {
   TFunc* tf;
   for (tf=Main; tf!=NULL; tf=tf->next)
    PrintFunction(tf,Main);
  }
  luaI_freefunc(Main);			/* TODO: free others */
 }
}

static void undump(char* filename)
{
 FILE* f= (filename==NULL) ? stdin : fopen(filename, "rb");
 if (f==NULL)
 {
  fprintf(stderr,"luac: cannot open ");
  perror(filename);
  exit(1);
 }
 else
 {
  ZIO z;
  zFopen(&z,f);
  do_undump(&z);
  fclose(f);
 }
}
