/*
** undump.c
** load bytecodes from files
*/

char* rcs_undump="$Id: undump.c,v 1.14 1996/03/14 17:31:15 lhf Exp $";

#include <stdio.h>
#include <string.h>
#include "opcode.h"
#include "mem.h"
#include "table.h"
#include "undump.h"

static int swapword=0;
static int swapfloat=0;
static TFunc* Main=NULL;			/* functions in a chunk */
static TFunc* lastF=NULL;

static void warn(char* s)			/* TODO: remove */
{
#if 0
 fprintf(stderr,"undump: %s\n",s);
#endif
}

static void FixCode(Byte* code, Byte* end)	/* swap words */
{
 Byte* p;
 for (p=code; p!=end;)
 {
	OpCode op=(OpCode)*p;
	switch (op)
	{
	case PUSHNIL:
	case PUSH0:
	case PUSH1:
	case PUSH2:
	case PUSHLOCAL0:
	case PUSHLOCAL1:
	case PUSHLOCAL2:
	case PUSHLOCAL3:
	case PUSHLOCAL4:
	case PUSHLOCAL5:
	case PUSHLOCAL6:
	case PUSHLOCAL7:
	case PUSHLOCAL8:
	case PUSHLOCAL9:
	case PUSHINDEXED:
	case STORELOCAL0:
	case STORELOCAL1:
	case STORELOCAL2:
	case STORELOCAL3:
	case STORELOCAL4:
	case STORELOCAL5:
	case STORELOCAL6:
	case STORELOCAL7:
	case STORELOCAL8:
	case STORELOCAL9:
	case STOREINDEXED0:
	case ADJUST0:
	case EQOP:
	case LTOP:
	case LEOP:
	case GTOP:
	case GEOP:
	case ADDOP:
	case SUBOP:
	case MULTOP:
	case DIVOP:
	case POWOP:
	case CONCOP:
	case MINUSOP:
	case NOTOP:
	case POP:
	case RETCODE0:
		p++;
		break;
	case PUSHBYTE:
	case PUSHLOCAL:
	case STORELOCAL:
	case STOREINDEXED:
	case STORELIST0:
	case ADJUST:
	case RETCODE:
		p+=2;
		break;
	case STORELIST:
	case CALLFUNC:
		p+=3;
		break;
	case PUSHFUNCTION:
		p+=5;
		break;
	case PUSHWORD:
	case PUSHSELF:
	case CREATEARRAY:
	case ONTJMP:
	case ONFJMP:
	case JMP:
	case UPJMP:
	case IFFJMP:
	case IFFUPJMP:
	case SETLINE:
	case PUSHSTRING:
	case PUSHGLOBAL:
	case STOREGLOBAL:
	{
		Byte t;
		t=p[1]; p[1]=p[2]; p[2]=t;
		p+=3;
		break;
	}
	case PUSHFLOAT:
	{
		Byte t;
		t=p[1]; p[1]=p[4]; p[4]=t;
		t=p[2]; p[2]=p[3]; p[3]=t;
		p+=5;
		break;
	}
	case STORERECORD:
	{
		int n=*++p;
		p++;
		while (n--)
		{
			Byte t;
			t=p[0]; p[0]=p[1]; p[1]=t;
			p+=2;
		}
		break;
	}
	default:
		lua_error("corrupt binary file");
		break;
	}
 }
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
  Byte* p=(Byte*)&w;				/* TODO: need union? */
  Byte t;
  t=p[0]; p[0]=p[1]; p[1]=t;
 }
 return w;
}

static int LoadSize(FILE* D)
{
 Word hi=LoadWord(D);
 Word lo=LoadWord(D);
 int s=(hi<<16)|lo;
 if ((Word)s != s) lua_error("code too long");
 return s;
}

static char* LoadBlock(int size, FILE* D)
{
 char* b=luaI_malloc(size);
 fread(b,size,1,D);
 return b;
}

static char* LoadString(FILE* D)
{
 int size=LoadWord(D);
 char *b=luaI_buffer(size);
 fread(b,size,1,D);
 return b;
}

static char* LoadNewString(FILE* D)
{
 return LoadBlock(LoadWord(D),D);
}

static void LoadFunction(FILE* D)
{
 TFunc* tf=new(TFunc);
 tf->next=NULL;
 tf->locvars=NULL;
 tf->size=LoadSize(D);
 tf->lineDefined=LoadWord(D);
 if (IsMain(tf))				/* new main */
 {
  tf->fileName=LoadNewString(D);
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
 tf->code=LoadBlock(tf->size,D);
 if (swapword || swapfloat) FixCode(tf->code,tf->code+tf->size);
 while (1)					/* unthread */
 {
  int c=getc(D);
  if (c==ID_VAR)				/* global var */
  {
   int i=LoadWord(D);
   char* s=LoadString(D);
   int v=luaI_findsymbolbyname(s);
   Unthread(tf->code,i,v);
  }
  else if (c==ID_STR)				/* constant string */
  {
   int i=LoadWord(D);
   char* s=LoadString(D);
   int v=luaI_findconstantbyname(s);
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
 if (*s!=0) lua_error("bad signature");
}

static void LoadHeader(FILE* D)			/* TODO: error handling */
{
 Word w,tw=TEST_WORD;
 float f,tf=TEST_FLOAT;
 LoadSignature(D);
 getc(D);					/* skip version */
 fread(&w,sizeof(w),1,D);			/* test word */
 if (w!=tw)
 {
  swapword=1;
  warn("different byte order");
 }
 fread(&f,sizeof(f),1,D);			/* test float */
 if (f!=tf)
 {
  Byte* p=(Byte*)&f;				/* TODO: need union? */
  Byte t;
  swapfloat=1;
  t=p[0]; p[0]=p[3]; p[3]=t;
  t=p[1]; p[1]=p[2]; p[2]=t;
  if (f!=tf)					/* TODO: try another perm? */
   lua_error("different float representation");
  else
   warn("different byte order in floats");
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
}

/*
** load one chunk from a file.
** return list of functions found, headed by main, or NULL at EOF.
*/
TFunc* luaI_undump1(FILE* D)
{
 while (1)
 {
  int c=getc(D);
  if (c==ID_CHUNK)
  {
   LoadChunk(D);
   return Main;
  }
  else if (c==EOF)
   return NULL;
  else
   lua_error("not a lua binary file");
 }
}

/*
** load and run all chunks in a file
*/
int luaI_undump(FILE* D)
{
 TFunc* m;
 while ((m=luaI_undump1(D)))
 {
  int status=luaI_dorun(m);
  luaI_freefunc(m);
  if (status!=0) return status;
 }
 return 0;
}
