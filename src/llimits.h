/*
** $Id: llimits.h,v 1.59 2004/06/23 15:57:29 roberto Exp $
** Limits, basic types, and some other `installation-dependent' definitions
** See Copyright Notice in lua.h
*/

#ifndef llimits_h
#define llimits_h


#include <limits.h>
#include <stddef.h>


#include "lua.h"



typedef LUA_UINT32 lu_int32;

typedef LUA_INT32 l_int32;


/*
** an unsigned integer big enough to count the total memory used by Lua;
** it should be at least as large as `size_t'
*/
typedef lu_int32 lu_mem;


/*
** a signed integer big enough to count the total memory used by Lua;
** it should be at least as large as `size_t'
*/
typedef l_int32 l_mem;
#define MAXLMEM	LUA_MAXINT32



/* chars used as small naturals (so that `char' is reserved for characters) */
typedef unsigned char lu_byte;


#define MAX_SIZET	((size_t)(~(size_t)0)-2)


#define MAX_INT (INT_MAX-2)  /* maximum value of an int (-2 for safety) */

/*
** conversion of pointer to integer
** this is for hashing only; there is no problem if the integer
** cannot hold the whole pointer value
*/
#define IntPoint(p)  ((unsigned int)(p))



/* type to ensure maximum alignment */
typedef LUSER_ALIGNMENT_T L_Umaxalign;


/* result of a `usual argument conversion' over lua_Number */
typedef LUA_UACNUMBER l_uacNumber;


#ifndef check_exp
#define check_exp(c,e)	(e)
#endif


#ifndef UNUSED
#define UNUSED(x)	((void)(x))	/* to avoid warnings */
#endif


#ifndef cast
#define cast(t, exp)	((t)(exp))
#endif



/*
** type for virtual-machine instructions
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
typedef lu_int32 Instruction;



/* maximum stack for a Lua function */
#define MAXSTACK	250



/* minimum size for the string table (must be power of 2) */
#ifndef MINSTRTABSIZE
#define MINSTRTABSIZE	32
#endif


/* minimum size for string buffer */
#ifndef LUA_MINBUFFER
#define LUA_MINBUFFER	32
#endif


#endif
