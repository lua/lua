/*
** $Id: opt.c,v 1.12 1999/07/02 19:34:26 lhf Exp $
** optimize bytecodes
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "luac.h"

static void FixArg(Byte* p, int i, int j, int isconst)
{
 if (j==i)
  ;
 else if (i<=MAX_BYTE)		/* j<i, so j fits where i did */
  p[1]=j;
 else if (i<=MAX_WORD)
 {
  if (isconst && j<=MAX_BYTE)	/* may use byte variant instead */
  {
   p[0]++;			/* byte variant follows word variant */
   p[1]=j;
   p[2]=NOP;
  }
  else 				/* stuck with word variant */
  {
   p[1]=j>>8;
   p[2]=j;
  }
 }
 else 				/* previous instruction must've been LONGARG */
 {
  if (isconst && j<=MAX_WORD) p[-2]=p[-1]=NOP; else p[-1]=j>>16;
  p[1]=j>>8;
  p[2]=j;
 }
}

static void FixConstants(TProtoFunc* tf, int* C)
{
 Byte* code=tf->code;
 Byte* p=code;
 int longarg=0;
 for (;;)
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  int i=OP.arg+longarg;
  longarg=0;
  if (op==PUSHCONSTANT || op==GETGLOBAL || op==GETDOTTED ||
      op==PUSHSELF     || op==SETGLOBAL || op==CLOSURE)
   FixArg(p,i,C[i],1);
  else if (op==LONGARG) longarg=i<<16;
  else if (op==ENDCODE) break;
  p+=n;
 }
}

#define	UNREF	1			/* "type" of unused constants */
#define	BIAS	128			/* mark for used constants */

static void NoUnrefs(TProtoFunc* tf)
{
 int i,n=tf->nconsts;
 Byte* code=tf->code;
 Byte* p=code;
 int longarg=0;
 for (;;)				/* mark all used constants */
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  int i=OP.arg+longarg;
  longarg=0;
  if (op==PUSHCONSTANT || op==GETGLOBAL || op==GETDOTTED ||
      op==PUSHSELF     || op==SETGLOBAL || op==CLOSURE)
  {
   TObject* o=tf->consts+i;
   if (ttype(o)<=0) ttype(o)+=BIAS;	/* mark as used */
  }
  else if (op==LONGARG) longarg=i<<16;
  else if (op==ENDCODE) break;
  p+=n;
 }
 for (i=0; i<n; i++)			/* mark all unused constants */
 {
  TObject* o=tf->consts+i;
  if (ttype(o)<=0)
   ttype(o)=UNREF;			/* mark as unused */
  else
   ttype(o)-=BIAS;			/* unmark used constant */
 }
}

#define CMP(oa,ob,f)	memcmp(&f(oa),&f(ob),sizeof(f(oa)))

static int compare(TProtoFunc* tf, int ia, int ib)
{
 TObject* oa=tf->consts+ia;
 TObject* ob=tf->consts+ib;
 int t=ttype(oa)-ttype(ob);
 if (t) return t;
 switch (ttype(oa))
 {
  case LUA_T_NUMBER:	return CMP(oa,ob,nvalue);
  case LUA_T_STRING:	return CMP(oa,ob,tsvalue);
  case LUA_T_PROTO:	return CMP(oa,ob,tfvalue);
  case LUA_T_NIL:	return 0;
  case UNREF:		return 0;
  default:		return ia-ib;	/* cannot happen */
 }
}

static TProtoFunc* TF;			/* for sort */

static int compare1(const void* a, const void* b)
{
 int ia=*(int*)a;
 int ib=*(int*)b;
 int t=compare(TF,ia,ib);
 return (t) ? t : ia-ib;
}

static void OptConstants(TProtoFunc* tf)
{
 static int* C=NULL;
 static int* D=NULL;
 int i,k;
 int n=tf->nconsts;
 if (n==0) return;
 luaM_reallocvector(C,n,int);
 luaM_reallocvector(D,n,int);
 NoUnrefs(tf);
 for (i=0; i<n; i++) C[i]=D[i]=i;	/* group duplicates */
 TF=tf; qsort(C,n,sizeof(C[0]),compare1);
 k=C[0];				/* build duplicate table */
 for (i=1; i<n; i++)
 {
  int j=C[i];
  if (compare(tf,k,j)==0) D[j]=k; else k=j;
 }
 k=0;					/* build rename map & pack constants */
 for (i=0; i<n; i++)
 {
  if (D[i]==i)				/* new value */
  {
   TObject* o=tf->consts+i;
   if (ttype(o)!=UNREF)
   {
    tf->consts[k]=tf->consts[i];
    C[i]=k++;
   }
  }
  else C[i]=C[D[i]];
 }
 if (k<n)
 {
printf("\t" SOURCE " reduced constants from %d to %d\n",
	 tf->source->str,tf->lineDefined,n,k);
  FixConstants(tf,C);
  tf->nconsts=k;
 }
}

static int NoDebug(TProtoFunc* tf)
{
 Byte* code=tf->code;
 Byte* p=code;
 int lop=NOP;				/* last opcode */
 int nop=0;
 for (;;)				/* change SETLINE to NOP */
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  if (op==NOP) ++nop;
  else if (op==SETLINE)
  {
   int m;
   if (lop==LONGARG) m=2; else if (lop==LONGARGW) m=3; else m=0;
   nop+=n+m; memset(p-m,NOP,n+m);
  }
  else if (op==ENDCODE) break;
  lop=OP.op;
  p+=n;
 }
 return nop;
}

static int FixJump(TProtoFunc* tf, Byte* a, Byte* b)
{
 Byte* p;
 int nop=0;
 for (p=a; p<b; )
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  if (op==NOP) ++nop;
  else if (op==ENDCODE) break;
  p+=n;
 }
 return nop;
}

static void FixJumps(TProtoFunc* tf)
{
 Byte* code=tf->code;
 Byte* p=code;
 int longarg=0;
 for (;;)
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  int i=OP.arg+longarg;
  int nop=0;
  longarg=0;
  if (op==ENDCODE) break;
  else if (op==IFTUPJMP || op==IFFUPJMP)
   nop=FixJump(tf,p-i+n,p);
  else if (op==ONTJMP || op==ONFJMP || op==JMP || op==IFFJMP)
   nop=FixJump(tf,p,p+i+n);
  else if (op==LONGARG) longarg=i<<16;
  if (nop>0) FixArg(p,i,i-nop,0);
  p+=n;
 }
}

static void PackCode(TProtoFunc* tf)
{
 Byte* code=tf->code;
 Byte* p=code;
 Byte* q=code;
 for (;;)
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  if (op!=NOP) { memcpy(q,p,n); q+=n; }
  p+=n;
  if (op==ENDCODE) break;
 }
printf("\t" SOURCE " reduced code from %d to %d\n",
	tf->source->str,tf->lineDefined,(int)(p-code),(int)(q-code));
}

static void OptCode(TProtoFunc* tf)
{
 if (NoDebug(tf)==0) return;		/* cannot improve code */
 FixJumps(tf);
 PackCode(tf);
}

static void OptFunction(TProtoFunc* tf);

static void OptFunctions(TProtoFunc* tf)
{
 int i,n=tf->nconsts;
 for (i=0; i<n; i++)
 {
  TObject* o=tf->consts+i;
  if (ttype(o)==LUA_T_PROTO) OptFunction(tfvalue(o));
 }
}

static void OptFunction(TProtoFunc* tf)
{
 OptConstants(tf);
 OptCode(tf);
 OptFunctions(tf);
 tf->source=luaS_new("");
 tf->locvars=NULL;
}

void luaU_optchunk(TProtoFunc* Main)
{
 OptFunction(Main);
}
