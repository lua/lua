/*
** $Id: llimits.h,v 1.31 2001/08/27 15:16:28 roberto Exp $
** Limits, basic types, and some other `installation-dependent' definitions
** See Copyright Notice in lua.h
*/

#ifndef llimits_h
#define llimits_h


#include <limits.h>
#include <stddef.h>


#include "lua.h"


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
** the following types define integer types for values that may not
** fit in a `small int' (16 bits), but may waste space in a
** `large long' (64 bits). The current definitions should work in
** any machine, but may not be optimal.
*/

/* an unsigned integer to hold hash values */
typedef unsigned int lu_hash;
/* its signed equivalent */
typedef int ls_hash;

/* an unsigned integer big enough to count the total memory used by Lua */
typedef unsigned long lu_mem;

/* an integer big enough to count the number of strings in use */
typedef long ls_nstr;

/* a bitmap with one bit for each upvalue used by a function */
typedef unsigned long ls_bitup;


/* chars used as small naturals (so that `char' is reserved for characteres) */
typedef unsigned char lu_byte;


#define MAX_SIZET	((size_t)(~(size_t)0)-2)


#define MAX_INT (INT_MAX-2)  /* maximum value of an int (-2 for safety) */

/*
** conversion of pointer to integer
** this is for hashing only; there is no problem if the integer
** cannot hold the whole pointer value
** (the shift removes bits that are usually 0 because of alignment)
*/
#define IntPoint(p)  ((((lu_hash)(p)) >> 4) ^ (lu_hash)(p))



#define MINPOWER2       4       /* minimum size for `growing' vectors */



#ifndef DEFAULT_STACK_SIZE
#define DEFAULT_STACK_SIZE      1024
#endif



/* type to ensure maximum alignment */
#ifndef LUSER_ALIGNMENT_T
#define LUSER_ALIGNMENT_T	double
#endif
union L_Umaxalign { LUSER_ALIGNMENT_T u; void *s; long l; };



/*
** type for virtual-machine instructions
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
typedef unsigned long Instruction;


/* maximum stack for a Lua function */
#define MAXSTACK	250


/* maximum number of local variables */
#ifndef MAXLOCALS
#define MAXLOCALS 200           /* arbitrary limit (<MAXSTACK) */
#endif


/* maximum number of upvalues */
#ifndef MAXUPVALUES
#define MAXUPVALUES	(sizeof(ls_bitup)*CHAR_BIT)
#endif


/* maximum number of parameters in a function */
#ifndef MAXPARAMS
#define MAXPARAMS 100           /* arbitrary limit (<MAXLOCALS) */
#endif


/* number of list items to accumulate before a SETLIST instruction */
/* (must be a power of 2) */
#define LFIELDS_PER_FLUSH	64



#endif
