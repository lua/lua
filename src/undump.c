/*
** undump.c
** load bytecodes from files
*/

char* rcs_undump="$Id: undump.c,v 1.24 1997/06/17 18:19:17 roberto Exp $";

#include <stdio.h>
#include <string.h>
#include "auxlib.h"
#include "opcode.h"
#include "luamem.h"
#include "table.h"
#include "undump.h"
#include "zio.h"

static int swapword=0;
static int swapfloat=0;
static TFunc* Main=NULL;			/* functions in a chunk */
static TFunc* lastF=NULL;

static void FixCode(Byte* code, Byte* end)	/* swap words */
{
 Byte* p;
 for (p=code; p!=end;)
 {
	int op=*p;
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
	case VARARGS:
	case STOREMAP:
		p+=2;
		break;
	case STORELIST:
	case CALLFUNC:
		p+=3;
		break;
	case PUSHFUNCTION:
		p+=5;			/* TODO: use sizeof(TFunc*) or old? */
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
	case PUSHFLOAT:			/* assumes sizeof(float)==4 */
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
		luaL_verror("corrupt binary file: bad opcode %d at %d\n",
			op,(int)(p-code));
		break;
	}
 }
}

static void Unthread(Byte* code, int i, int v)
{
 while (i!=0)
 {
  Word w;
  Byte* p=code+i;
  memcpy(&w,p,sizeof(w));
  i=w; w=v;
  memcpy(p,&w,sizeof(w));
 }
}

static int LoadWord(ZIO* Z)
{
 Word w;
 zread(Z,&w,sizeof(w));
 if (swapword)
 {
  Byte* p=(Byte*)&w;
  Byte t;
  t=p[0]; p[0]=p[1]; p[1]=t;
 }
 return w;
}

static int LoadSize(ZIO* Z)
{
 Word hi=LoadWord(Z);
 Word lo=LoadWord(Z);
 int s=(hi<<16)|lo;
 if ((Word)s != s) lua_error("code too long");
 return s;
}

static void* LoadBlock(int size, ZIO* Z)
{
 void* b=luaI_malloc(size);
 zread(Z,b,size);
 return b;
}

static char* LoadString(ZIO* Z)
{
 int size=LoadWord(Z);
 char *b=luaI_buffer(size);
 zread(Z,b,size);
 return b;
}

static char* LoadNewString(ZIO* Z)
{
 return LoadBlock(LoadWord(Z),Z);
}

static void LoadFunction(ZIO* Z)
{
 TFunc* tf=new(TFunc);
 tf->next=NULL;
 tf->locvars=NULL;
 tf->size=LoadSize(Z);
 tf->lineDefined=LoadWord(Z);
 if (IsMain(tf))				/* new main */
 {
  tf->fileName=LoadNewString(Z);
  Main=lastF=tf;
 }
 else						/* fix PUSHFUNCTION */
 {
  tf->marked=LoadWord(Z);
  tf->fileName=Main->fileName;
  memcpy(Main->code+tf->marked,&tf,sizeof(tf));
  lastF=lastF->next=tf;
 }
 tf->code=LoadBlock(tf->size,Z);
 if (swapword || swapfloat) FixCode(tf->code,tf->code+tf->size);
 while (1)					/* unthread */
 {
  int c=zgetc(Z);
  if (c==ID_VAR)				/* global var */
  {
   int i=LoadWord(Z);
   char* s=LoadString(Z);
   int v=luaI_findsymbolbyname(s);
   Unthread(tf->code,i,v);
  }
  else if (c==ID_STR)				/* constant string */
  {
   int i=LoadWord(Z);
   char* s=LoadString(Z);
   int v=luaI_findconstantbyname(s);
   Unthread(tf->code,i,v);
  }
  else
  {
   zungetc(Z);
   break;
  }
 }
}

static void LoadSignature(ZIO* Z)
{
 char* s=SIGNATURE;
 while (*s!=0 && zgetc(Z)==*s)
  ++s;
 if (*s!=0) lua_error("cannot load binary file: bad signature");
}

static void LoadHeader(ZIO* Z)
{
 Word w,tw=TEST_WORD;
 float f,tf=TEST_FLOAT;
 int version;
 LoadSignature(Z);
 version=zgetc(Z);
 if (version>0x23)				/* after 2.5 */
 {
  int oldsizeofW=zgetc(Z);
  int oldsizeofF=zgetc(Z);
  int oldsizeofP=zgetc(Z);
  if (oldsizeofW!=2)
   luaL_verror(
	"cannot load binary file created on machine with sizeof(Word)=%d; "
	"expected 2",oldsizeofW);
  if (oldsizeofF!=4)
   luaL_verror(
	"cannot load binary file created on machine with sizeof(float)=%d; "
	"expected 4\nnot an IEEE machine?",oldsizeofF);
  if (oldsizeofP!=sizeof(TFunc*))		/* TODO: pack? */
   luaL_verror(
	"cannot load binary file created on machine with sizeof(TFunc*)=%d; "
	"expected %d",oldsizeofP,(int)sizeof(TFunc*));
 }
 zread(Z,&w,sizeof(w));			/* test word */
 if (w!=tw)
 {
  swapword=1;
 }
 zread(Z,&f,sizeof(f));			/* test float */
 if (f!=tf)
 {
  Byte* p=(Byte*)&f;
  Byte t;
  swapfloat=1;
  t=p[0]; p[0]=p[3]; p[3]=t;
  t=p[1]; p[1]=p[2]; p[2]=t;
  if (f!=tf)					/* TODO: try another perm? */
   lua_error("cannot load binary file: unknown float representation");
 }
}

static void LoadChunk(ZIO* Z)
{
 LoadHeader(Z);
 while (1)
 {
  int c=zgetc(Z);
  if (c==ID_FUN) LoadFunction(Z); else { zungetc(Z); break; }
 }
}

/*
** load one chunk from a file.
** return list of functions found, headed by main, or NULL at EOF.
*/
TFunc* luaI_undump1(ZIO* Z)
{
 int c=zgetc(Z);
 if (c==ID_CHUNK)
 {
  LoadChunk(Z);
  return Main;
 }
 else if (c!=EOZ)
   lua_error("not a lua binary file");
 return NULL;
}

/*
** load and run all chunks in a file
*/
int luaI_undump(ZIO* Z)
{
 TFunc* m;
 while ((m=luaI_undump1(Z)))
 {
  int status=luaI_dorun(m);
  luaI_freefunc(m);
  if (status!=0) return status;
 }
 return 0;
}
