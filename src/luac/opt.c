/*
** $Id: opt.c,v 1.4 1998/04/02 20:44:08 lhf Exp $
** optimize bytecodes
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include "luac.h"
#include "lmem.h"

static void FixConstants(TProtoFunc* tf, int* C)
{
 Byte* code=tf->code;
 Byte* p=code;
 while (1)
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  int i=OP.arg;
  if (op==ENDCODE) break;
  if (	op==PUSHCONSTANT || op==GETDOTTED || op==PUSHSELF ||
	op==GETGLOBAL    || op==SETGLOBAL)
  {
   int j=C[i];
   if (j==i)
    ;
   else if (n==1)
   {
    p[0]=op+j+1;
   }
   else if (n==2)
   {
    if (j<8) { p[0]=op+j+1; p[1]=NOP; } else p[1]=j;
   }
   else
   {
    if (j<=255)
    {
     p[0]=op;
     p[1]=j;
     p[2]=NOP;
    }
    else 
    {
     p[1]= 0x0000FF & (j>>8);
     p[2]= 0x0000FF &  j;
    }
   }
  }
  p+=n;
 }
}

static TProtoFunc* TF;

static int compare(const void* a, const void *b)
{
 int ia=*(int*)a;
 int ib=*(int*)b;
 int t;
 TObject* oa=TF->consts+ia;
 TObject* ob=TF->consts+ib;
 t=ttype(oa)-ttype(ob);		if (t) return t;
 t=oa->value.i-ob->value.i;	if (t) return t;
 return ia-ib;
}

static void OptConstants(TProtoFunc* tf)
{
 static int* C=NULL;
 static int* D=NULL;
 int i,k;
 int n=tf->nconsts;
 if (n==0) return;
 C=luaM_reallocvector(C,n,int);
 D=luaM_reallocvector(D,n,int);
 for (i=0; i<n; i++) C[i]=D[i]=i;	/* group duplicates */
 TF=tf; qsort(C,n,sizeof(C[0]),compare);
 k=C[0];				/* build duplicate table */
 for (i=1; i<n; i++)
 {
  int j=C[i];
  TObject* oa=tf->consts+k;
  TObject* ob=tf->consts+j;
  if (ttype(oa)==ttype(ob) && oa->value.i==ob->value.i) D[j]=k; else k=j;
 }
 k=0;					/* build rename map & pack constants */
 for (i=0; i<n; i++)
 {
  if (D[i]==i) { tf->consts[k]=tf->consts[i]; C[i]=k++; } else C[i]=C[D[i]];
 }
 if (k>=n) return;
printf("\t\"%s\":%d reduced constants from %d to %d\n",
	tf->fileName->str,tf->lineDefined,n,k);
 tf->nconsts=k;
 FixConstants(tf,C);
}

static int NoDebug(TProtoFunc* tf)
{
 Byte* code=tf->code;
 Byte* p=code;
 int nop=0;
 while (1)				/* change SETLINE to NOP */
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  if (op==ENDCODE) break;
  if (op==NOP) ++nop;
  if (op==SETLINE) { nop+=n; memset(p,NOP,n); }
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
  if (op==ENDCODE) break;
  if (op==NOP) ++nop;
  p+=n;
 }
 return nop;
}

static void FixJumps(TProtoFunc* tf)
{
 Byte* code=tf->code;
 Byte* p=code;
 while (1)
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  int i=OP.arg;
  int nop;
  if (op==ENDCODE) break;
  nop=0;
  if (op==IFTUPJMP || op==IFFUPJMP) nop=FixJump(tf,p-i+n,p); else
  if (op==ONTJMP || op==ONFJMP || op==JMP || op==IFFJMP) nop=FixJump(tf,p,p+i+n);
  if (nop>0)
  {
   int j=i-nop;
   if (n==2)
    p[1]=j;
   else
#if 0
   {
    if (j<=255)				/* does NOT work for nested loops */
    {
     if (op==IFTUPJMP || op==IFFUPJMP) --j;
     p[0]=OP.op-1;			/* *JMP and *JMPW are consecutive */
     p[1]=j;
     p[2]=NOP;
    }
    else 
#endif
    {
     p[1]= 0x0000FF & (j>>8);
     p[2]= 0x0000FF &  j;
    }
#if 0
   }
#endif
  }
  p+=n;
 }
}

static void PackCode(TProtoFunc* tf)
{
 Byte* code=tf->code;
 Byte* p=code;
 Byte* q=code;
 while (1)
 {
  Opcode OP;
  int n=INFO(tf,p,&OP);
  int op=OP.class;
  if (op!=NOP) { memcpy(q,p,n); q+=n; }
  p+=n;
  if (op==ENDCODE) break;
 }
printf("\t\"%s\":%d reduced code from %d to %d\n",
	tf->fileName->str,tf->lineDefined,(int)(p-code),(int)(q-code));
}

static void OptCode(TProtoFunc* tf)
{
 int nop=NoDebug(tf);
 if (nop==0) return;			/* cannot improve code */
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
 tf->locvars=NULL;			/* remove local variables table */
 OptConstants(tf);
 OptCode(tf);
 OptFunctions(tf);
}

void OptChunk(TProtoFunc* Main)
{
 OptFunction(Main);
}
