/*
** $Id: luaconf.h,v 1.6 2004/06/02 19:07:55 roberto Exp roberto $
** Configuration file for Lua
** See Copyright Notice in lua.h
*/


#ifndef lconfig_h
#define lconfig_h

#include <limits.h>
#include <stddef.h>


/*
** {======================================================
** Index (search for keyword to find corresponding entry)
** =======================================================
*/


/* }====================================================== */




/*
** {======================================================
** Generic configuration
** =======================================================
*/

/* type of numbers in Lua */
#define LUA_NUMBER	double

/* formats for Lua numbers */
#define LUA_NUMBER_SCAN		"%lf"
#define LUA_NUMBER_FMT		"%.14g"


/* type for integer functions */
#define LUA_INTEGER	long


/* mark for all API functions */
#define LUA_API		extern

/* mark for auxlib functions */
#define LUALIB_API      extern

/* buffer size used by lauxlib buffer system */
#define LUAL_BUFFERSIZE   BUFSIZ


/* first index for arrays */
#define LUA_FIRSTINDEX		1

/* assertions in Lua (mainly for internal debugging) */
#define lua_assert(c)		/* empty */

/* }====================================================== */



/*
** {======================================================
** Stand-alone configuration
** =======================================================
*/

#ifdef lua_c

/* definition of `isatty' */
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#define stdin_is_tty()		isatty(0)
#else
#define stdin_is_tty()		1  /* assume stdin is a tty */
#endif


#define PROMPT		"> "
#define PROMPT2		">> "
#define PROGNAME	"lua"


#define LUA_EXTRALIBS   /* empty */

#define lua_userinit(L)		openstdlibs(L)



/*
** this macro can be used by some `history' system to save lines
** read in manual input
*/
#define lua_saveline(L,line)	/* empty */



#endif

/* }====================================================== */



/*
** {======================================================
** Core configuration
** =======================================================
*/

#ifdef LUA_CORE

/* LUA-C API assertions */
#define api_check(L, o)		/* empty */


/* an unsigned integer with at least 32 bits */
#define LUA_UINT32	unsigned long

/* a signed integer with at least 32 bits */
#define LUA_INT32	long
#define LUA_MAXINT32	LONG_MAX


/* maximum depth for calls (unsigned short) */
#define LUA_MAXCALLS	4096

/*
** maximum depth for C calls (unsigned short): Not too big, or may
** overflow the C stack...
*/
#define LUA_MAXCCALLS	200


/* maximum size for the virtual stack of a C function */
#define MAXCSTACK	2048


/*
** maximum number of syntactical nested non-terminals: Not too big,
** or may overflow the C stack...
*/
#define LUA_MAXPARSERLEVEL	200


/* maximum number of variables declared in a function */
#define MAXVARS	200		/* <MAXSTACK */


/* maximum number of upvalues per function */
#define MAXUPVALUES		32	/* <MAXSTACK */


/* maximum size of expressions for optimizing `while' code */
#define MAXEXPWHILE		100


/* function to convert a lua_Number to int (with any rounding method) */
#if defined(__GNUC__) && defined(__i386)
#define lua_number2int(i,d)	__asm__ ("fistpl %0":"=m"(i):"t"(d):"st")
#elif 0
/* on machines compliant with C99, you can try `lrint' */
#include <math.h>
#define lua_number2int(i,d)	((i)=lrint(d))
#else
#define lua_number2int(i,d)	((i)=(int)(d))
#endif

/* function to convert a lua_Number to lua_Integer (with any rounding method) */
#define lua_number2integer(i,n)		lua_number2int(i,n)


/* function to convert a lua_Number to a string */
#include <stdio.h>
#define lua_number2str(s,n)	sprintf((s), LUA_NUMBER_FMT, (n))

/* function to convert a string to a lua_Number */
#define lua_str2number(s,p)	strtod((s), (p))



/* result of a `usual argument conversion' over lua_Number */
#define LUA_UACNUMBER	double


/* number of bits in an `int' */
/* avoid overflows in comparison */
#if INT_MAX-20 < 32760
#define LUA_BITSINT	16
#elif INT_MAX > 2147483640L
/* machine has at least 32 bits */
#define LUA_BITSINT	32
#else
#error "you must define LUA_BITSINT with number of bits in an integer"
#endif


/* type to ensure maximum alignment */
#define LUSER_ALIGNMENT_T	union { double u; void *s; long l; }


/* exception handling */
#ifndef __cplusplus
/* default handling with long jumps */
#include <setjmp.h>
#define L_THROW(c)	longjmp((c)->b, 1)
#define L_TRY(c,a)	if (setjmp((c)->b) == 0) { a }
#define l_jmpbuf	jmp_buf

#else
/* C++ exceptions */
#define L_THROW(c)	throw(c)
#define L_TRY(c,a)	try { a } catch(...) \
	{ if ((c)->status == 0) (c)->status = -1; }
#define l_jmpbuf	int  /* dummy variable */
#endif



/*
** macros for thread synchronization inside Lua core machine:
** all accesses to the global state and to global objects are synchronized.
** Because threads can read the stack of other threads
** (when running garbage collection),
** a thread must also synchronize any write-access to its own stack.
** Unsynchronized accesses are allowed only when reading its own stack,
** or when reading immutable fields from global objects
** (such as string values and udata values).
*/
#define lua_lock(L)     ((void) 0)
#define lua_unlock(L)   ((void) 0)

/*
** this macro allows a thread switch in appropriate places in the Lua
** core
*/
#define lua_threadyield(L)	{lua_unlock(L); lua_lock(L);}



/* allows user-specific initialization on new threads */
#define lua_userstateopen(l)	/* empty */


#endif

/* }====================================================== */



/*
** {======================================================
** Library configuration
** =======================================================
*/

#ifdef LUA_LIB



/* `assert' options */
/* name of global that holds table with loaded packages */
#define REQTAB		"_LOADED"

/* name of global that holds the search path for packages */
#define LUA_PATH	"LUA_PATH"

/* separator of templates in a path */
#define LUA_PATH_SEP	';'

/* wild char in each template */
#define LUA_PATH_MARK	'?'

/* default path */
#define LUA_PATH_DEFAULT	"?;?.lua"


/* maximum number of captures in pattern-matching */
#define MAX_CAPTURES	32  /* arbitrary limit */


/*
** by default, gcc does not get `tmpname'
*/ 
#ifdef __GNUC__
#define USE_TMPNAME	0
#else
#define USE_TMPNAME	1 
#endif



#endif

/* }====================================================== */




/* Local configuration */

#undef USE_TMPNAME
#define USE_TMPNAME	1

#endif
