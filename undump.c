/*
** undump.c
** load bytecodes from files
*/

char* rcs_undump="$Id: undump.c,v 1.9 1996/03/06 16:01:08 lhf Exp lhf $";

#include <stdio.h>
#include <string.h>
#include "luac.h"

static int swapword=0;
static int swapfloat=0;

static void warn(char* s)			/* TODO: remove */
{
 fprintf(stderr,"undump: %s\n",s);
}

static void panic(char* s)			/* TODO: remove */
{
 warn(s);
 exit(1);
}

static void Unthread(Byte* code, int i, int v)
{
 while (i!=0)
 {
  CodeWord c;
  Byte* p=code+i;
  get_word(c,p);
  i=c.w;
  c.w=v;
  p[-2]=c.m.c1;
  p[-1]=c.m.c2;
 }
}

static int LoadWord(FILE* D)
{
 Word w;
 fread(&w,sizeof(w),1,D);
 if (swapword)
 {
  Byte* p=&w;					/* TODO: need union? */
  Byte t;
  t=p[0]; p[0]=p[1]; p[1]=t;
 }
 return w;
}

static char* LoadBlock(int size, FILE* D)
{
 char* b=luaI_malloc(size);
 fread(b,size,1,D);
 return b;
}

static int LoadSize(FILE* D)
{
 Word hi=LoadWord(D);
 Word lo=LoadWord(D);
 int s=(hi<<16)|lo;
 if ((Word)s != s) panic("code too long");
 return s;
}

static char* LoadString(FILE* D)
{
 return LoadBlock(LoadWord(D),D);
}

static TFunc* Main=NULL;
static TFunc* lastF=NULL;

static void LoadFunction(FILE* D)
{
 TFunc* tf=new(TFunc);
 tf->next=NULL;
 tf->locvars=NULL;
 tf->size=LoadSize(D);
 tf->lineDefined=LoadWord(D);
 if (IsMain(tf))				/* new main */
 {
  tf->fileName=LoadString(D);
  Main=lastF=tf;
 }
 else						/* fix PUSHFUNCTION */
 {
  CodeCode c;
  Byte* p;
  tf->marked=LoadWord(D);
  tf->fileName=Main->fileName;
  p=Main->code+tf->marked;
  c.tf=tf;
  *p++=c.m.c1; *p++=c.m.c2; *p++=c.m.c3; *p++=c.m.c4;
  lastF=lastF->next=tf;
 }
#if 0
 tf->marked=0;					/* TODO: is this ok? */
#endif
 tf->code=LoadBlock(tf->size,D);
 if (swapword || swapfloat) FixCode(tf->code,tf->code+tf->size);
 while (1)					/* unthread */
 {
  int c=getc(D);
  if (c==ID_VAR)				/* global var */
  {
   int i=LoadWord(D);
   char* s=LoadString(D);
   int v=luaI_findsymbolbyname(s);		/* TODO: free s? */
   Unthread(tf->code,i,v);
  }
  else if (c==ID_STR)				/* constant string */
  {
   int i=LoadWord(D);
   char* s=LoadString(D);
   int v=luaI_findconstantbyname(s);		/* TODO: free s? */
   Unthread(tf->code,i,v);
  }
  else
  {
   ungetc(c,D);
   break;
  }
 }
}

static void LoadSignature(FILE* D)
{
 char* s=SIGNATURE;
 while (*s!=0 && getc(D)==*s)
  ++s;
 if (*s!=0) panic("bad signature");
}

static void LoadHeader(FILE* D)			/* TODO: error handling */
{
 Word w,tw=TEST_WORD;
 float f,tf=TEST_FLOAT;
 LoadSignature(D);
 getc(D);					/* skip version */
 fread(&w,sizeof(w),1,D);		/* a word for testing byte ordering */
 if (w!=tw)
 {
  swapword=1;
  warn("different byte order");
 }
 fread(&f,sizeof(f),1,D);		/* a float for testing byte ordering */
 if (f!=tf)
 {
  swapfloat=1;					/* TODO: only one test? */
  if (f!=tf) warn("different float representation");
 }
}

static void LoadChunk(FILE* D)
{
 LoadHeader(D);
 while (1)
 {
  int c=getc(D);
  if (c==ID_FUN) LoadFunction(D); else { ungetc(c,D); break; }
 }
#if 1
 {						/* TODO: run Main? */
  TFunc* tf;
  for (tf=Main; tf!=NULL; tf=tf->next)
   PrintFunction(tf);
 }
#endif
}

void luaI_undump(FILE* D)
{
 while (1)
 {
  int c=getc(D);
  if (c==ID_CHUNK)
   LoadChunk(D);
  else if (c==EOF)
   break;
  else
   panic("not a lua binary file");
 }
}

int main(int argc, char* argv[])
{
 char* fn=(argc>1)? argv[1] : "luac.out";
 FILE* f=freopen(fn,"rb",stdin);
 if (f==NULL)
 {
  fprintf(stderr,"undump: cannot open ");
  perror(fn);
  exit(1);
 }
 luaI_undump(stdin); 
 return 0;
}
