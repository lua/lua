/*
** print.c
** print bytecodes
*/

char* rcs_print="$Id: print.c,v 1.11 1996/11/18 11:24:16 lhf Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "luac.h"
#include "print.h"

static LocVar* V=NULL;

static char* LocStr(int i)
{
 if (V==NULL) return ""; else return V[i].varname->str;
}

static void PrintCode(Byte* code, Byte* end)
{
 Byte* p;
 for (p=code; p!=end;)
 {
	OpCode op=(OpCode)*p;
	if (op>SETLINE) op=SETLINE+1;
	printf("%6d\t%s",(int)(p-code),OpCodeName[op]);
	switch (op)
	{
	case PUSHNIL:
	case PUSH0:
	case PUSH1:
	case PUSH2:
	case PUSHINDEXED:
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
	{
		int i=op-PUSHLOCAL0;
		printf("\t%d\t; %s",i,LocStr(i));
		p++;
		break;
	}
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
	{
		int i=op-STORELOCAL0;
		printf("\t%d\t; %s",i,LocStr(i));
		p++;
		break;
	}
	case PUSHLOCAL:
	case STORELOCAL:
	{
		int i=*(p+1);
		printf("\t%d\t; %s",i,LocStr(i));
		p+=2;
		break;
	}
	case PUSHBYTE:
	case STOREINDEXED:
	case STORELIST0:
	case ADJUST:
	case RETCODE:
		printf("\t%d",*(p+1));
		p+=2;
		break;
	case PUSHWORD:
	case CREATEARRAY:
	case ONTJMP:
	case ONFJMP:
	case JMP:
	case UPJMP:
	case IFFJMP:
	case IFFUPJMP:
	case SETLINE:
	{
		Word w;
		p++;
		get_word(w,p);
		printf("\t%d",w);
		break;
	}
	case PUSHFLOAT:
	{
		float f;
		p++;
		get_float(f,p);
		printf("\t%g",f);
		break;
	}
	case PUSHSELF:
	case PUSHSTRING:
	{
		Word w;
		p++;
		get_word(w,p);
		printf("\t%d\t; \"%s\"",w,StrStr(w));
		break;
	}
	case PUSHFUNCTION:
	{
		TFunc* tf;
		p++;
		get_code(tf,p);
		printf("\t%p\t; \"%s\":%d",tf,tf->fileName,tf->lineDefined);
		break;
	}
	case PUSHGLOBAL:
	case STOREGLOBAL:
	{
		Word w;
		p++;
		get_word(w,p);
		printf("\t%d\t; %s",w,VarStr(w));
		break;
	}
	case STORELIST:
	case CALLFUNC:
		printf("\t%d %d",*(p+1),*(p+2));
		p+=3;
		break;
	case STORERECORD:
	{
		int n=*++p;
		printf("\t%d",n);
		p++;
		while (n--)
		{
			Word w;
			printf("\n%6d\t      FIELD",(int)(p-code));
			get_word(w,p);
			printf("\t%d\t; \"%s\"",w,StrStr(w));
		}
		break;
	}
	default:
		printf("\tcannot happen:  opcode=%d",*p);
		exit(1);
		break;
	}
	printf("\n");
 }
}

void PrintFunction(TFunc* tf)
{
 if (IsMain(tf))
  printf("\nmain of \"%s\" (%d bytes at %p)\n",tf->fileName,tf->size,tf);
 else
  printf("\nfunction \"%s\":%d (%d bytes at %p); used at main+%d\n",
	tf->fileName,tf->lineDefined,tf->size,tf,tf->marked);
 V=tf->locvars;
 PrintCode(tf->code,tf->code+tf->size);
}
