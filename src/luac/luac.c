/*
** luac.c
** lua compiler (saves bytecodes to files)
*/

char* rcs_luac="$Id: luac.c,v 1.16 1996/03/13 17:33:03 lhf Exp $";

#include <stdio.h>
#include <string.h>
#include "luac.h"

static void compile(char* filename);

static int listing=0;			/* list bytecodes? */
static int dumping=1;			/* dump bytecodes? */
static FILE* D;				/* output file */

static void usage(void)
{
 fprintf(stderr,"usage: luac [-dlpv] [-o output] file ...\n");
 exit(0);
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
  else if (IS("-d"))			/* debug */
   lua_debug=1;
  else if (IS("-l"))			/* list */
   listing=1;
  else if (IS("-o"))			/* output file */
   d=argv[++i];
  else if (IS("-p"))			/* parse only (for timing purposes) */
   dumping=0;
  else if (IS("-v"))			/* show version */
   printf("%s  %s\n(written by %s)\n\n",LUA_VERSION,LUA_COPYRIGHT,LUA_AUTHORS);
  else					/* unknown option */
   usage();
 }
 --i;					/* fake new argv[0] */
 argc-=i;
 argv+=i;
 if (argc<2) usage();
 for (i=1; i<argc; i++)
  if (IS(d))
  {
   fprintf(stderr,"luac: will not overwrite input file \"%s\"\n",d);
   exit(1);
  }
 D=(dumping) ? fopen(d,"wb") : stdout;	/* must open in  binary mode */
 if (D==NULL)
 {
  fprintf(stderr,"luac: cannot open ");
  perror(d);
  exit(1);
 }
 for (i=1; i<argc; i++) compile(IS("-")? NULL : argv[i]);
 fclose(D);
 return 0;
}

static void do_dump(TFunc* tf)		/* only for tf==main */
{
 if (dumping) DumpHeader(D);
 while (tf!=NULL)
 {
  TFunc* nf;
  if (listing) PrintFunction(tf);
  if (dumping) DumpFunction(tf,D);
  nf=tf->next;				/* list only built after first main */
  luaI_freefunc(tf);
  tf=nf;
 }
}

static void do_compile(void)
{
 TFunc* tf=new(TFunc);
 luaI_initTFunc(tf);
 tf->fileName = lua_parsedfile;
 lua_parse(tf);
 do_dump(tf);
}

static void compile(char* filename)
{
 if (lua_openfile(filename)==NULL)
 {
  fprintf(stderr,"luac: cannot open ");
  perror(filename);
  exit(1);
 }
 do_compile();
 lua_closefile();
}
