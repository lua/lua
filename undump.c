/*
** undump.c
** load bytecodes from files
*/

char *rcs_undump="$Id$";

#include <stdio.h>
#include <string.h>
#include "luac.h"

static void warn(char *s)
{
 fprintf(stderr,"luac: %s\n",s);
}

static void panic(char *s)
{
 warn(s);
 exit(1);
}

static void Unthread(Byte *p, int i, int v)
{
 while (i!=0)
 {
  CodeWord c;
  Byte *q=p+i;
  get_word(c,q);
  q=p+i;
  i=c.w;
  c.w=v;
  q[0]=c.m.c1;
  q[1]=c.m.c2;
 }
}

static char* LoadBlock(int size, FILE *D)
{
 char *b=luaI_malloc(size);
 fread(b,size,1,D);
 return b;
}

static int LoadWord(FILE *D)
{
 Word w;
 fread(&w,sizeof(w),1,D);
 return w;
}

static char* LoadString(FILE *D)
{
 return LoadBlock(LoadWord(D),D);
}

static char* LoadCode(int size, FILE *D)
{
 return LoadBlock(size,D);
}

static TFunc* LoadFunction(FILE *D)
{
 TFunc *tf=new(TFunc);
 tf->size=LoadWord(D);
 tf->marked=LoadWord(D);
 tf->lineDefined=LoadWord(D);
 tf->fileName=LoadString(D);
 tf->code=LoadCode(tf->size,D);
 while (1)
 {
  int c=getc(D);
  if (c=='V')
  {
   int i=LoadWord(D);
   char *s=LoadString(D);
   int v=luaI_findsymbolbyname(s);
   Unthread(tf->code,i,v);
  }
  else if (c=='S')
  {
   int i=LoadWord(D);
   char *s=LoadString(D);
   int v=luaI_findconstantbyname(s);
   Unthread(tf->code,i,v);
  }
  else
  {
PrintFunction(tf);
   ungetc(c,D);
   return tf;
  }
 }
}

static void LoadHeader(FILE *D)
{
 char *s=LoadString(D);
 Word w,tw=TEST_WORD;
 float f,tf=TEST_FLOAT;
 if (strcmp(s,SIGNATURE)!=0) panic("bad signature");
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
  if (c=='F') LoadFunction(D); else { ungetc(c,D); return; }
 }
}

void Undump(FILE *D)
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
 Undump(stdin); 
 return 0;
}
