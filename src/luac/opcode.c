/*
** $Id: opcode.c,v 1.4 1998/07/12 00:17:37 lhf Exp $
** opcode information
** See Copyright Notice in lua.h
*/

#include "luac.h"

static Opcode Info[]=			/* ORDER lopcodes.h */
{
#include "opcode.h"
};

#define NOPCODES	(sizeof(Info)/sizeof(Info[0]))

int OpcodeInfo(TProtoFunc* tf, Byte* p, Opcode* I, char* xFILE, int xLINE)
{
 Opcode OP;
 Byte* code=tf->code;
 int op=*p;
 if (p==code)
 {
  OP.name="STACK";
  OP.size=1;
  OP.op=STACK;
  OP.class=STACK;
  OP.arg=op;
 }
 else if (p==code+1)
 {
  OP.size=1;
  if (op>=ZEROVARARG)
  {
   OP.name="VARARGS";
   OP.op=VARARGS;
   OP.class=VARARGS;
   OP.arg=op-ZEROVARARG;
  }
  else
  {
   OP.name="ARGS";
   OP.op=ARGS;
   OP.class=ARGS;
   OP.arg=op;
  }
 }
 else if (op==NOP)
 {
  OP.name="NOP";
  OP.size=1;
  OP.op=NOP;
  OP.class=NOP;
 }
 else if (op>=NOPCODES)			/* cannot happen */
 {
  luaL_verror("internal error at %s:%d: bad opcode %d at %d in tf=%p",
	xFILE, xLINE,op,(int)(p-code),tf);
  return 0;
 }
 else
 {
  OP=Info[op];
  if (op==SETLIST || op==CLOSURE || op==CALLFUNC)
  {
   OP.arg=p[1];
   OP.arg2=p[2];
  }
  else if (OP.size==2) OP.arg=p[1];
  else if (OP.size>=3) OP.arg=(p[1]<<8)+p[2];
  if (op==SETLISTW || op==CLOSUREW) OP.arg2=p[3];
 }
 *I=OP;
 return OP.size;
}

int CodeSize(TProtoFunc* tf)
{
 Byte* code=tf->code;
 Byte* p=code;
 while (1)
 {
  Opcode OP;
  p+=INFO(tf,p,&OP);
  if (OP.op==ENDCODE) break;
 }
 return p-code;
}
