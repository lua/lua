/*
** $Id: luaconf.h,v 1.33 2005/03/08 18:09:16 roberto Exp roberto $
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

/* CONFIG: default path */
#if defined(_WIN32)
#define LUA_ROOT	"C:\\Program Files\\Lua51"
#define LUA_LDIR	LUA_ROOT "\\lua"
#define LUA_CDIR	LUA_ROOT "\\dll"
#define LUA_PATH_DEFAULT  \
		"?.lua;"  LUA_LDIR"\\?.lua;"  LUA_LDIR"\\?\\init.lua"
#define LUA_CPATH_DEFAULT	\
	"?.dll;"  "l?.dll;"  LUA_CDIR"\\?.dll;"  LUA_CDIR"\\l?.dll"

#else
#define LUA_ROOT	"/usr/local"
#define LUA_LDIR	LUA_ROOT "/share/lua/5.1"
#define LUA_CDIR	LUA_ROOT "/lib/lua/5.1"
#define LUA_PATH_DEFAULT  \
		"./?.lua;"  LUA_LDIR"/?.lua;"  LUA_LDIR"/?/init.lua"
#define LUA_CPATH_DEFAULT	\
	"./?.so;"  "./l?.so;"  LUA_CDIR"/?.so;"  LUA_CDIR"/l?.so"
#endif


/* CONFIG: directory separator (for submodules) */
#if defined(_WIN32)
#define LUA_DIRSEP	"\\"
#else
#define LUA_DIRSEP	"/"
#endif


/* CONFIG: environment variables that hold the search path for packages */
#define LUA_PATH	"LUA_PATH"
#define LUA_CPATH	"LUA_CPATH"

/* CONFIG: prefix for open functions in C libraries */
#define LUA_POF		"luaopen_"

/* CONFIG: separator for open functions in C libraries */
#define LUA_OFSEP	"_"

/* CONFIG: separator of templates in a path */
#define LUA_PATHSEP	';'

/* CONFIG: wild char in each template */
#define LUA_PATH_MARK	"?"



/* CONFIG: type of numbers in Lua */
#define LUA_NUMBER	double

/* CONFIG: formats for Lua numbers */
#define LUA_NUMBER_SCAN		"%lf"
#define LUA_NUMBER_FMT		"%.14g"


/*
** CONFIG: type for integer functions
** on most machines, `ptrdiff_t' gives a reasonable size for integers
*/
#define LUA_INTEGER	ptrdiff_t


/* CONFIG: mark for all API functions */
#define LUA_API		extern

/* CONFIG: mark for auxlib functions */
#define LUALIB_API	extern

/* CONFIG: buffer size used by lauxlib buffer system */
#define LUAL_BUFFERSIZE		BUFSIZ


/* CONFIG: assertions in Lua (mainly for internal debugging) */
#define lua_assert(c)		((void)0)

/* }====================================================== */



/*
** {======================================================
** Stand-alone configuration
** =======================================================
*/

#ifdef lua_c

/* CONFIG: definition of `isatty' */
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#define stdin_is_tty()		isatty(0)
#elif defined(_WIN32)
#include <io.h>
#include <stdio.h>
#define stdin_is_tty()		_isatty(_fileno(stdin))
#else
#define stdin_is_tty()		1  /* assume stdin is a tty */
#endif


#define PROMPT		"> "
#define PROMPT2		">> "
#define PROGNAME	"lua"




/*
** CONFIG: this macro can be used by some `history' system to save lines
** read in manual input
*/
#define lua_saveline(L,line)	/* empty */



#endif

/* }====================================================== */



/* CONFIG: LUA-C API assertions */
#define luac_apicheck(L,o)		lua_assert(o)


/* number of bits in an `int' */
/* avoid overflows in comparison */
#if INT_MAX-20 < 32760
#define LUAC_BITSINT	16
#elif INT_MAX > 2147483640L
/* `int' has at least 32 bits */
#define LUAC_BITSINT	32
#else
#error "you must define LUA_BITSINT with number of bits in an integer"
#endif


/*
** CONFIG:
** LUAC_UINT32: unsigned integer with at least 32 bits
** LUAC_INT32: signed integer with at least 32 bits
** LUAC_UMEM: an unsigned integer big enough to count the total memory
**           used by Lua
** LUAC_MEM: a signed integer big enough to count the total memory used by Lua
*/
#if LUAC_BITSINT >= 32
#define LUAC_UINT32	unsigned int
#define LUAC_INT32	int
#define LUAC_MAXINT32	INT_MAX
#define LUAC_UMEM	size_t
#define LUAC_MEM		ptrdiff_t
#else
/* 16-bit ints */
#define LUAC_UINT32	unsigned long
#define LUAC_INT32	long
#define LUAC_MAXINT32	LONG_MAX
#define LUAC_UMEM	LUAC_UINT32
#define LUAC_MEM	ptrdiff_t
#endif


/* CONFIG: maximum depth for calls (unsigned short) */
#define LUAC_MAXCALLS	10000

/*
** CONFIG: maximum depth for C calls (unsigned short): Not too big, or may
** overflow the C stack...
*/
#define LUAC_MAXCCALLS	200


/* CONFIG: maximum size for the virtual stack of a C function */
#define LUAC_MAXCSTACK	2048


/*
** CONFIG: maximum number of syntactical nested non-terminals: Not too big,
** or may overflow the C stack...
*/
#define LUAC_MAXPARSERLEVEL	200


/* CONFIG: maximum number of variables declared in a function */
#define LUAC_MAXVARS	200		/* <MAXSTACK */


/* CONFIG: maximum number of upvalues per function */
#define LUAC_MAXUPVALUES	60	/* <MAXSTACK */


/* CONFIG: maximum size of expressions for optimizing `while' code */
#define LUAC_MAXEXPWHILE	100


/* CONFIG: function to convert a lua_Number to int (with any rounding method) */
#if defined(__GNUC__) && defined(__i386)
#define lua_number2int(i,d)	__asm__ ("fistpl %0":"=m"(i):"t"(d):"st")

#elif defined(_MSC_VER) && defined(_M_IX86)
#pragma warning(disable: 4514)
__inline int l_lrint (double flt)
{     int i;
      _asm {
      fld flt
      fistp i
      };
      return i;
}
#define lua_number2int(i,d)   ((i)=l_lrint((d)))

#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199900L)
/* on machines compliant with C99, you can try `lrint' */
#define lua_number2int(i,d)	((i)=lrint(d))

#else
#define lua_number2int(i,d)	((i)=(int)(d))

#endif


/* CONFIG: function to convert a lua_Number to lua_Integer (with any rounding method) */
#define lua_number2integer(i,n)		lua_number2int((i), (n))


/* CONFIG: function to convert a lua_Number to a string */
#define lua_number2str(s,n)	sprintf((s), LUA_NUMBER_FMT, (n))
/* maximum size of previous conversion */
#define LUAC_MAXNUMBER2STR	32 /* 16 digits, sign, point, and \0 */

/* CONFIG: function to convert a string to a lua_Number */
#define lua_str2number(s,p)	strtod((s), (p))



/* CONFIG: result of a `usual argument conversion' over lua_Number */
#define LUAC_UACNUMBER	double


/* CONFIG: primitive operators for numbers */
#define luac_numadd(a,b)	((a)+(b))
#define luac_numsub(a,b)	((a)-(b))
#define luac_nummul(a,b)	((a)*(b))
#define luac_numdiv(a,b)	((a)/(b))
#define luac_numunm(a)		(-(a))
#define luac_numeq(a,b)		((a)==(b))
#define luac_numlt(a,b)		((a)<(b))
#define luac_numle(a,b)		((a)<=(b))
#define luac_nummod(a,b)	((a) - floor((a)/(b))*(b))
#define luac_numpow(a,b)	pow(a,b)



/* CONFIG: type to ensure maximum alignment */
#define LUAC_USER_ALIGNMENT_T	union { double u; void *s; long l; }


/*
** CONFIG: exception handling: by default, Lua handles errors with
** longjmp/setjmp when compiling as C code and with exceptions
** when compiling as C++ code.  Change that if you prefer to use
** longjmp/setjmp even with C++.
*/
#ifndef __cplusplus
/* default handling with long jumps */
#define LUAC_THROW(L,c)	longjmp((c)->b, 1)
#define LUAC_TRY(L,c,a)	if (setjmp((c)->b) == 0) { a }
#define luac_jmpbuf	jmp_buf

#else
/* C++ exceptions */
#define LUAC_THROW(L,c)	throw(c)
#define LUAC_TRY(L,c,a)	try { a } catch(...) \
	{ if ((c)->status == 0) (c)->status = -1; }
#define luac_jmpbuf	int  /* dummy variable */
#endif



/*
** CONFIG: macros for thread synchronization inside Lua core
** machine: This is an attempt to simplify the implementation of a
** multithreaded version of Lua. Do not change that unless you know
** what you are doing. all accesses to the global state and to global
** objects are synchronized.  Because threads can read the stack of
** other threads (when running garbage collection), a thread must also
** synchronize any write-access to its own stack.  Unsynchronized
** accesses are allowed only when reading its own stack, or when reading
** immutable fields from global objects (such as string values and udata
** values).
*/
#define lua_lock(L)	((void) 0)
#define lua_unlock(L)	((void) 0)


/*
** CONFIG: this macro allows a thread switch in appropriate places in
** the Lua core
*/
#define lua_threadyield(L)	{lua_unlock(L); lua_lock(L);}



/* CONFIG: allows user-specific initialization on new threads */
#define lua_userstateopen(L)	((void)0)





/* CONFIG: maximum number of captures in pattern-matching (arbitrary limit) */
#define LUA_MAXCAPTURES		32


/*
** CONFIG: by default, gcc does not get `os.tmpname', because it
** generates a warning when using `tmpname'. Change that if you really
** want (or do not want) `os.tmpname' available.
*/
#ifdef __GNUC__
#define LUA_USETMPNAME	0
#else
#define LUA_USETMPNAME	1 
#endif


/*
** CONFIG: Configuration for loadlib: Lua tries to guess the
** dynamic-library system that your platform uses (either Windows' DLL,
** Mac's dyld, or dlopen). If your system is some kind of Unix, there is
** a good chance that LUA_USEDLOPEN will work for it. You may need to adapt
** also the makefile.
*/
#if defined(_WIN32)
#define LUA_USEDLL
#elif defined(__APPLE__) && defined(__MACH__)
#define LUA_USEDYLD
#elif defined(__linux) || defined(sun) || defined(sgi) || defined(BSD)
#define LUA_USEDLOPEN
#endif



/* ======================================================= */

/* Local configuration */

#undef LUA_USETMPNAME
#define LUA_USETMPNAME	1

#endif
