/*
** $Id: llimits.h,v 1.2 2000/03/24 19:49:23 roberto Exp roberto $
** Limits, basic types, and some other "instalation-dependent" definitions
** See Copyright Notice in lua.h
*/

#ifndef llimits_h
#define llimits_h


#include <limits.h>


/*
** Define the type `number' of Lua
** GREP LUA_NUMBER to change that
*/
#ifndef LUA_NUM_TYPE
#define LUA_NUM_TYPE double
#endif

typedef LUA_NUM_TYPE Number;



#define MAX_INT (INT_MAX-2)  /* maximum value of an int (-2 for safety) */

/*
** conversion of pointer to int (for hashing only)
** (the shift removes bits that are usually 0 because of alignment)
*/
#define IntPoint(L, p)  (((unsigned long)(p)) >> 3)


/*
** number of `blocks' for garbage collection: each reference to other
** objects count 1, and each 32 bytes of `raw' memory count 1; we add
** 2 to the total as a minimum (and also to count the overhead of malloc)
*/
#define numblocks(L, o,b)       ((o)+((b)>>5)+2)


#define MINPOWER2       4       /* minimum size for "growing" vectors */


/*
** type for virtual-machine instructions
** must be an unsigned with 4 bytes (see details in lopcodes.h)
** For a very small machine, you may change that to 2 bytes (and adjust
** the following limits accordingly)
*/
typedef unsigned long Instruction;


/*
** limits for opcode arguments.
** For an instruction with 2 bytes, size is 16, and size_b can be 5
*/
#define SIZE_INSTRUCTION        32
#define SIZE_OP         6
#define SIZE_B          9

#define SIZE_U          (SIZE_INSTRUCTION-SIZE_OP)
#define POS_U           SIZE_OP
#define SIZE_S          (SIZE_INSTRUCTION-SIZE_OP)
#define POS_S           SIZE_OP
#define POS_B           SIZE_OP
#define SIZE_A          (SIZE_INSTRUCTION-(SIZE_OP+SIZE_B))
#define POS_A           (SIZE_OP+SIZE_B)

#define MAXARG_U        ((1<<SIZE_U)-1)
#define MAXARG_S        ((1<<(SIZE_S-1))-1)  /* `S' is signed */
#define MAXARG_A        ((1<<SIZE_A)-1)
#define MAXARG_B        ((1<<SIZE_B)-1)


/*
** we use int to manipulate most arguments, so they must fit
*/
#if MAXARG_U > MAX_INT
#undef MAXARG_U
#define MAXARG_U        MAX_INT
#endif

#if MAXARG_S > MAX_INT
#undef MAXARG_S
#define MAXARG_S        MAX_INT
#endif

#if MAXARG_A > MAX_INT
#undef MAXARG_A
#define MAXARG_A        MAX_INT
#endif

#if MAXARG_B > MAX_INT
#undef MAXARG_B
#define MAXARG_B        MAX_INT
#endif


/* maximum stack size in a function */
#define MAXSTACK	MAXARG_B


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


/* special code for multiple returns */
#define MULT_RET        255	/* (<=MAXARG_B) */
#if MULT_RET>MAXARG_B
#undef MULT_RET
#define MULT_RET	MAXARG_B
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
#define LFIELDS_PER_FLUSH	(MAXSTACK/4)

/* number of record items to accumulate before a SETMAP instruction */
/* (each item counts 2 elements on the stack: an index and a value) */
#define RFIELDS_PER_FLUSH	(LFIELDS_PER_FLUSH/2)


/* maximum number of values printed in one call to `print' */
#ifndef MAXPRINT
#define MAXPRINT        40	/* arbitrary limit */
#endif


/* maximum depth of nested $ifs */
#ifndef MAX_IFS
#define MAX_IFS 5  /* arbitrary limit */
#endif


/* maximum size of a pragma line */
#ifndef PRAGMASIZE
#define PRAGMASIZE      80  /* arbitrary limit */
#endif


/* maximum lookback to find a real constant (for code generation) */
#ifndef LOOKBACKNUMS
#define LOOKBACKNUMS    20      /* arbitrary constant */
#endif


#endif
