/*
** undump.c
** load bytecodes from files
*/

char *rcs_undump="$Id: undump.c,v 1.5 1996/02/26 19:44:17 lhf Exp lhf $";

#include <stdio.h>
#include <string.h>
#include "luac.h"

static void warn(char *s)			/* TODO: remove */
{
 fprintf(stderr,"luac: %s\n",s);
}

static void panic(char *s)			/* TODO: remove */
{
 warn(s);
 exit(1);
}

static void Unthread(Byte *code, int i, int v)
{
 while (i!=0)
 {
  CodeWord c;
  Byte *p=code+i;
  get_word(c,p);
  i=c.w;
  c.w=v;
  p[-2]=c.m.c1;
  p[-1]=c.m.c2;
 }
}

static int LoadWord(FILE *D)
{
 Word w;
 fread(&w,sizeof(w),1,D);
 return w;
}

static char* LoadBlock(int size, FILE *D)
{
 char *b=luaI_malloc(size);
 fread(b,size,1,D);
 return b;
}

static char* LoadString(FILE *D)
{
 return LoadBlock(LoadWord(D),D);
}

static TFunc *Main=NULL;
static TFunc *lastF=NULL;

static void LoadFunction(FILE *D)
{
 TFunc *tf=new(TFunc);
 tf->next=NULL;
 tf->size=LoadWord(D);				/* TODO: Long? */
 tf->lineDefined=LoadWord(D);
 if (IsMain(tf))				/* new main */
 {
  tf->fileName=LoadString(D);
  Main=lastF=tf;
 }
 else						/* fix PUSHFUNCTION */
 {
  CodeCode c;
  Byte *p;
  tf->marked=LoadWord(D);
  tf->fileName=Main->fileName;
  p=Main->code+tf->marked;			/* TODO: tf->marked=? */
  c.tf=tf;
  *p++=c.m.c1; *p++=c.m.c2; *p++=c.m.c3; *p++=c.m.c4;
  lastF->next=tf; lastF=tf;
 }
 tf->code=LoadBlock(tf->size,D);
 while (1)					/* unthread */
 {
  int c=getc(D);
  if (c=='V')
  {
   int i=LoadWord(D);
   char *s=LoadString(D);
   int v=luaI_findsymbolbyname(s);		/* TODO: free s? */
   Unthread(tf->code,i,v);
  }
  else if (c=='S')
  {
   int i=LoadWord(D);
   char *s=LoadString(D);
   int v=luaI_findconstantbyname(s);		/* TODO: free s? */
   Unthread(tf->code,i,v);
  }
  else
  {
   ungetc(c,D);
   return;
  }
 }
}

static void LoadHeader(FILE *D)			/* TODO: error handling */
{
 char *s=LoadString(D);
 Word w,tw=TEST_WORD;
 float f,tf=TEST_FLOAT;
 if (strcmp(s,SIGNATURE)!=0) panic("bad signature");
 luaI_free(s);
 getc(D);				/* skip version */
 fread(&w,sizeof(w),1,D);		/* a word for testing byte ordering */
 if (w!=tw) warn("different byte order");
 fread(&f,sizeof(f),1,D);		/* a float for testing byte ordering */
 if (f!=tf) warn("different float representation");
}

static void LoadChunk(FILE *D)
{
 LoadHeader(D);
 while (1)
 {
  int c=getc(D);
  if (c=='F') LoadFunction(D); else { ungetc(c,D); break; }
 }
#if 1
 {						/* TODO: run Main? */
  TFunc *tf;
  for (tf=Main; tf!=NULL; tf=tf->next)
   PrintFunction(tf);
 }
#endif
}

void luaI_undump(FILE *D)
{
 while (1)
 {
  int c=getc(D);
  if (c==ESC) LoadChunk(D);	else
  if (c==EOF) return;		else
  panic("not a lua binary file");
 }
}

int main(int argc, char* argv[])
{
 FILE *f=freopen("luac.out","rb",stdin);
 if (f==NULL)
 {
  fprintf(stderr,"undump: cannot open ");
  perror("luac.out");
  exit(1);
 }
 luaI_undump(stdin); 
 return 0;
}
