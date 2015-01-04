/*
** $Id: llimits.h,v 1.19 2000/10/26 12:47:05 roberto Exp $
** Limits, basic types, and some other "installation-dependent" definitions
** See Copyright Notice in lua.h
*/

#ifndef llimits_h
#define llimits_h


#include <limits.h>
#include <stddef.h>



/*
** try to find number of bits in an integer
*/
#ifndef BITS_INT
/* avoid overflows in comparison */
#if INT_MAX-20 < 32760
#define	BITS_INT	16
#else
#if INT_MAX > 2147483640L
/* machine has at least 32 bits */
#define BITS_INT	32
#else
#error "you must define BITS_INT with number of bits in an integer"
#endif
#endif
#endif


/*
** Define the type `number' of Lua
** GREP LUA_NUMBER to change that
*/
#ifndef LUA_NUM_TYPE
#define LUA_NUM_TYPE double
#endif

typedef LUA_NUM_TYPE Number;

/* function to convert a Number to a string */
#define NUMBER_FMT	"%.16g"		/* LUA_NUMBER */
#define lua_number2str(s,n)	sprintf((s), NUMBER_FMT, (n))

/* function to convert a string to a Number */
#define lua_str2number(s,p)	strtod((s), (p))



typedef unsigned long lint32;  /* unsigned int with at least 32 bits */


#define MAX_SIZET	((size_t)(~(size_t)0)-2)


#define MAX_INT (INT_MAX-2)  /* maximum value of an int (-2 for safety) */

/*
** conversion of pointer to int (for hashing only)
** (the shift removes bits that are usually 0 because of alignment)
*/
#define IntPoint(p)  (((unsigned long)(p)) >> 3)



#define MINPOWER2       4       /* minimum size for "growing" vectors */



#ifndef DEFAULT_STACK_SIZE
#define DEFAULT_STACK_SIZE      1024
#endif



/* type to ensure maximum alignment */
union L_Umaxalign { double d; char *s; long l; };



/*
** type for virtual-machine instructions
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
** For a very small machine, you may change that to 2 bytes (and adjust
** the following limits accordingly)
*/
typedef unsigned long Instruction;


/*
** size and position of opcode arguments.
** For an instruction with 2 bytes, size is 16, and size_b can be 5
** (accordingly, size_u will be 10, and size_a will be 5)
*/
#define SIZE_INSTRUCTION        32
#define SIZE_B          9

#define SIZE_OP         6
#define SIZE_U          (SIZE_INSTRUCTION-SIZE_OP)
#define POS_U           SIZE_OP
#define POS_B           SIZE_OP
#define SIZE_A          (SIZE_INSTRUCTION-(SIZE_OP+SIZE_B))
#define POS_A           (SIZE_OP+SIZE_B)


/*
** limits for opcode arguments.
** we use (signed) int to manipulate most arguments,
** so they must fit in BITS_INT-1 bits (-1 for sign)
*/
#if SIZE_U < BITS_INT-1
#define MAXARG_U        ((1<<SIZE_U)-1)
#define MAXARG_S        (MAXARG_U>>1)		/* `S' is signed */
#else
#define MAXARG_U        MAX_INT
#define MAXARG_S        MAX_INT
#endif

#if SIZE_A < BITS_INT-1
#define MAXARG_A        ((1<<SIZE_A)-1)
#else
#define MAXARG_A        MAX_INT
#endif

#if SIZE_B < BITS_INT-1
#define MAXARG_B        ((1<<SIZE_B)-1)
#else
#define MAXARG_B        MAX_INT
#endif


/* maximum stack size in a function */
#ifndef MAXSTACK
#define MAXSTACK	250
#endif

#if MAXSTACK > MAXARG_B
#undef MAXSTACK
#define MAXSTACK	MAXARG_B
#endif


/* maximum number of local variables */
#ifndef MAXLOCALS
#define MAXLOCALS 200           /* arbitrary limit (<MAXSTACK) */
#endif
#if MAXLOCALS>=MAXSTACK
#undef MAXLOCALS
#define MAXLOCALS	(MAXSTACK-1)
#endif


/* maximum number of upvalues */
#ifndef MAXUPVALUES
#define MAXUPVALUES 32          /* arbitrary limit (<=MAXARG_B) */
#endif
#if MAXUPVALUES>MAXARG_B
#undef MAXUPVALUES
#define MAXUPVALUES	MAXARG_B
#endif


/* maximum number of variables in the left side of an assignment */
#ifndef MAXVARSLH
#define MAXVARSLH 100           /* arbitrary limit (<MULT_RET) */
#endif
#if MAXVARSLH>=MULT_RET
#undef MAXVARSLH
#define MAXVARSLH	(MULT_RET-1)
#endif


/* maximum number of parameters in a function */
#ifndef MAXPARAMS
#define MAXPARAMS 100           /* arbitrary limit (<MAXLOCALS) */
#endif
#if MAXPARAMS>=MAXLOCALS
#undef MAXPARAMS
#define MAXPARAMS	(MAXLOCALS-1)
#endif


/* number of list items to accumulate before a SETLIST instruction */
#define LFIELDS_PER_FLUSH	64
#if LFIELDS_PER_FLUSH>(MAXSTACK/4)
#undef LFIELDS_PER_FLUSH
#define LFIELDS_PER_FLUSH	(MAXSTACK/4)
#endif

/* number of record items to accumulate before a SETMAP instruction */
/* (each item counts 2 elements on the stack: an index and a value) */
#define RFIELDS_PER_FLUSH	(LFIELDS_PER_FLUSH/2)


/* maximum lookback to find a real constant (for code generation) */
#ifndef LOOKBACKNUMS
#define LOOKBACKNUMS    20      /* arbitrary constant */
#endif


#endif
