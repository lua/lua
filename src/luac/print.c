/*
** print.c
** print bytecodes
*/

char* rcs_print="$Id: print.c,v 1.6 1996/03/12 20:00:24 lhf Exp $";

#include <stdio.h>
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
	printf("%6d\t%s",p-code,OpCodeName[op]);
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
		CodeWord c;
		p++;
		get_word(c,p);
		printf("\t%d",c.w);
		break;
	}
	case PUSHFLOAT:
	{
		CodeFloat c;
		p++;
		get_float(c,p);
		printf("\t%g",c.f);
		break;
	}
	case PUSHSELF:
	case PUSHSTRING:
	{
		CodeWord c;
		p++;
		get_word(c,p);
		printf("\t%d\t; \"%s\"",c.w,StrStr(c.w));
		break;
	}
	case PUSHFUNCTION:
	{
		CodeCode c;
		p++;
		get_code(c,p);
		printf("\t%p\t; \"%s\":%d",c.tf,c.tf->fileName,c.tf->lineDefined);
		break;
	}
	case PUSHGLOBAL:
	case STOREGLOBAL:
	{
		CodeWord c;
		p++;
		get_word(c,p);
		printf("\t%d\t; %s",c.w,VarStr(c.w));
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
			CodeWord c;
			printf("\n%6d\t      FIELD",p-code);
			get_word(c,p);
			printf("\t%d\t; \"%s\"",c.w,StrStr(c.w));
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
