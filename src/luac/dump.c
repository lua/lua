/*
** $Id: dump.c,v 1.30 2000/10/31 16:57:23 lhf Exp $
** save bytecodes to file
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "luac.h"

#define DumpVector(b,n,size,D)	fwrite(b,size,n,D)
#define DumpBlock(b,size,D)	fwrite(b,size,1,D)
#define	DumpByte		fputc

static void DumpInt(int x, FILE* D)
{
 DumpBlock(&x,sizeof(x),D);
}

static void DumpSize(size_t x, FILE* D)
{
 DumpBlock(&x,sizeof(x),D);
}

static void DumpNumber(Number x, FILE* D)
{
 DumpBlock(&x,sizeof(x),D);
}

static void DumpString(const TString* s, FILE* D)
{
 if (s==NULL || s->str==NULL)
  DumpSize(0,D);
 else
 {
  size_t size=s->len+1;			/* include trailing '\0' */
  DumpSize(size,D);
  DumpBlock(s->str,size,D);
 }
}

static void DumpCode(const Proto* tf, FILE* D)
{
 DumpInt(tf->ncode,D);
 DumpVector(tf->code,tf->ncode,sizeof(*tf->code),D);
}

static void DumpLocals(const Proto* tf, FILE* D)
{
 int i,n=tf->nlocvars;
 DumpInt(n,D);
 for (i=0; i<n; i++)
 {
  DumpString(tf->locvars[i].varname,D);
  DumpInt(tf->locvars[i].startpc,D);
  DumpInt(tf->locvars[i].endpc,D);
 }
}

static void DumpLines(const Proto* tf, FILE* D)
{
 DumpInt(tf->nlineinfo,D);
 DumpVector(tf->lineinfo,tf->nlineinfo,sizeof(*tf->lineinfo),D);
}

static void DumpFunction(const Proto* tf, FILE* D);

static void DumpConstants(const Proto* tf, FILE* D)
{
 int i,n;
 DumpInt(n=tf->nkstr,D);
 for (i=0; i<n; i++)
  DumpString(tf->kstr[i],D);
 DumpInt(tf->nknum,D);
 DumpVector(tf->knum,tf->nknum,sizeof(*tf->knum),D);
 DumpInt(n=tf->nkproto,D);
 for (i=0; i<n; i++)
  DumpFunction(tf->kproto[i],D);
}

static void DumpFunction(const Proto* tf, FILE* D)
{
 DumpString(tf->source,D);
 DumpInt(tf->lineDefined,D);
 DumpInt(tf->numparams,D);
 DumpByte(tf->is_vararg,D);
 DumpInt(tf->maxstacksize,D);
 DumpLocals(tf,D);
 DumpLines(tf,D);
 DumpConstants(tf,D);
 DumpCode(tf,D);
 if (ferror(D))
 {
  perror("luac: write error");
  exit(1);
 }
}

static void DumpHeader(FILE* D)
{
 DumpByte(ID_CHUNK,D);
 fputs(SIGNATURE,D);
 DumpByte(VERSION,D);
 DumpByte(luaU_endianess(),D);
 DumpByte(sizeof(int),D);
 DumpByte(sizeof(size_t),D);
 DumpByte(sizeof(Instruction),D);
 DumpByte(SIZE_INSTRUCTION,D);
 DumpByte(SIZE_OP,D);
 DumpByte(SIZE_B,D);
 DumpByte(sizeof(Number),D);
 DumpNumber(TEST_NUMBER,D);
}

void luaU_dumpchunk(const Proto* Main, FILE* D)
{
 DumpHeader(D);
 DumpFunction(Main,D);
}
