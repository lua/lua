/*
** $Id: luac.h,v 1.6 1998/07/12 00:17:37 lhf Exp $
** definitions for luac
** See Copyright Notice in lua.h
*/

#include "lauxlib.h"
#include "lfunc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

typedef struct
{
 char* name;
 int size;
 int op;
 int class;
 int arg;
 int arg2;
} Opcode;

int OpcodeInfo(TProtoFunc* tf, Byte* p, Opcode* I, char* xFILE, int xLINE);
int CodeSize(TProtoFunc* tf);

#define INFO(tf,p,I)	OpcodeInfo(tf,p,I,__FILE__,__LINE__)
#define fileName(tf)	( (tf->fileName)==NULL ? NULL : tf->fileName->str )

#define NOP	255
#define STACK	-1
#define ARGS	-2
#define VARARGS	-3
