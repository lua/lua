/*
** $Id: luaconf.h,v 1.37 2005/03/18 18:55:45 roberto Exp roberto $
** Configuration file for Lua
** See Copyright Notice in lua.h
*/


#ifndef lconfig_h
#define lconfig_h

#include <limits.h>
#include <stddef.h>


/*
** {==================================================================
** Index (search for keyword to find corresponding entry)
** ===================================================================
*/


/* }================================================================== */



/*
*@ LUA_PATH_DEFAULT is the default path that Lua uses to look for
** Lua libraries.
*@ LUA_CPATH_DEFAULT is the default path that Lua uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/
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


/*
*@ LUA_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Lua automatically uses "\".)
*/
#if defined(_WIN32)
#define LUA_DIRSEP	"\\"
#else
#define LUA_DIRSEP	"/"
#endif


/*
*@ LUA_PATHSEP is the character that separates templates in a path.
** CHANGE it if for some reason your system cannot use a
** semicolon. (E.g., a semicolon may be a too common character in
** file/directory names.) Probably you do not need to change this.
*/
#define LUA_PATHSEP	';'


/*
*@ LUA_PATH_MARK is ths string that marks the substitution points in a
** template.
** CHANGE it if for some reason your system cannot use an interogation
** mark.  (E.g., an interogation mark may be a too common character in
** file/directory names.) Probably you do not need to change this.
*/
#define LUA_PATH_MARK	"?"


/*
*@ LUA_INTEGER is the integral type used by lua_pushinteger/lua_tointeger.
** CHANGE that if ptrdiff_t is not adequate on your machine. (On most
** machines, ptrdiff_t gives a good choice between int or long.)
*/
#define LUA_INTEGER	ptrdiff_t


/*
*@ LUA_API is a mark for all core API functions.
*@ LUALIB_API is a mark for all standard library functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create two Windows DLLs, one with the
** core, the other with the libraries, you may want to use the following
** definition:
**
**	#ifdef LUA_CORE
**	#define LUA_API __declspec(__dllexport)
**	#else
**	#define LUA_API __declspec(__dllimport)
**	#endif
**	#ifdef LUA_LIB
**	#define LUALIB_API __declspec(__dllexport)
**	#else
**	#define LUALIB_API __declspec(__dllimport)
**	#endif
*/
#define LUA_API		extern
#define LUALIB_API	extern


/*
*@ lua_assert describes the internal assertions in Lua.
** CHANGE that only if you need to debug Lua.
*/
#define lua_assert(c)		((void)0)



/*
** {==================================================================
** Stand-alone configuration
** ===================================================================
*/

#ifdef lua_c

/* CONFIG: definition of isatty */
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#define lua_stdin_is_tty()	isatty(0)
#elif defined(_WIN32)
#include <io.h>
#include <stdio.h>
#define lua_stdin_is_tty()	_isatty(_fileno(stdin))
#else
#define lua_stdin_is_tty()	1  /* assume stdin is a tty */
#endif


#define LUA_PROMPT		"> "
#define LUA_PROMPT2		">> "
#define LUA_PROGNAME		"lua"

/*
*@ LUA_MAXINPUT is the maximum length for an input line
** CHANGE it if you need longer lines.
*/
#define LUA_MAXINPUT	512


/*
*@ lua_readline defines how to show a prompt and then read a line from
** the standard input.
*@ lua_saveline defines how to "save" a read line.
** CHANGE them if you want to improve this functionality (e.g., using GNU
** readline and history facilities). (Lua already tries to use those
** facilities when it detects a GNU compiler.)
*/
#ifdef __GNUC__
#include <readline/readline.h>
#include <readline/history.h>
#define lua_readline(L,b,p)	(((b)=readline(p)) != NULL)
#define lua_saveline(L,idx) \
	if (lua_strlen(L,idx) > 0)  /* non-empty line? */ \
	  add_history(lua_tostring(L, idx));  /* add it to history */
#else
#define lua_readline(L,b,p)	\
	(fputs(p, stdout), fflush(stdout),  /* show prompt */ \
	fgets(b, LUA_MAXINPUT, stdin) != NULL)  /* get line */
#define lua_saveline(L,idx)	((void)0)
#endif



#endif

/* }================================================================== */


/*
** CHANGE here (undefining both luaL_getn and luaL_setn) if you want
** exact compatibility with the behavior of setn/getn in Lua 5.0.
*/
#define luaL_getn(L,i)		lua_objsize(L, i)
#define luaL_setn(L,i,j)	((void)0)  /* no op! */


/*
** CHANGE luai_apicheck if you want Lua to perform some checks in the
** parameters it gets from API calls. This may slow down the interpreter
** a bit, but may be quite useful when debugging C code that interfaces
** with Lua. A useful redefinition may be like this:
**
**	#include <assert.h>
**	#define luai_apicheck(L,o) assert(o)
**
** (By default lua_assert is empty, so luai_apicheck is also empty.)
*/
#define luai_apicheck(L,o)		lua_assert(o)


/*
** CHANGE here if Lua cannot automatically detect the number of bits of
** your machine. (In that case, simply define LUA_BITSINT.) Probably you
** do not need to change this.
*/
/* avoid overflows in comparison */
#if INT_MAX-20 < 32760
#define LUAI_BITSINT	16
#elif INT_MAX > 2147483640L
/* int has at least 32 bits */
#define LUAI_BITSINT	32
#else
#error "you must define LUA_BITSINT with number of bits in an integer"
#endif


/*
*@ LUAI_UINT32 is an unsigned integer with at least 32 bits.
*@ LUAI_INT32 is an signed integer with at least 32 bits.
*@ LUAI_UMEM is an an unsigned integer big enough to count the total
** memory used by Lua.
*@ LUAI_MEM is an a signed integer big enough to count the total memory
** used by Lua.
** CHANGE here if for some weird reason the default definitions are not
** good enough for your machine. The `else' definition always works, but
** may waste space on machines with 64-bit longs. Probably you do not
** need to change this.
*/
#if LUAI_BITSINT >= 32
#define LUAI_UINT32	unsigned int
#define LUAI_INT32	int
#define LUAI_MAXINT32	INT_MAX
#define LUAI_UMEM	size_t
#define LUAI_MEM	ptrdiff_t
#else
/* 16-bit ints */
#define LUAI_UINT32	unsigned long
#define LUAI_INT32	long
#define LUAI_MAXINT32	LONG_MAX
#define LUAI_UMEM	unsigned long
#define LUAI_MEM	long
#endif


/*
*@ LUAI_MAXCALLS limits the number of nested calls.
** CHANGE it if you need really deep recursive calls. This limit is
** arbitrary; its only purpose is to stop infinite recursion before
** exhausting memory.
*/
#define LUAI_MAXCALLS	20000


/*
*@ LUAI_MAXCSTACK limits the number of slots that a C function can use.
** CHANGE it if you need lots of (Lua) stack space for your C
** functions. This limit is arbitrary; its only purpose is to stop C
** functions to consume unlimited stack space.
*/
#define LUAI_MAXCSTACK	2048



/*
** {==================================================================
** CHANGE (to smaller values) the following definitions if your system
** has a small C stack. (Or you may want to change them to larger
** values if your system has a large C stack and these limits are
** too rigid for you.) Some of these constants control the size of
** stack-allocated arrays used by the compiler or the interpreter, while
** others limit the maximum number of recursive calls that the compiler
** or the interpreter can perform. Values too large may cause a C stack
** overflow for some forms of deep constructs.
** ===================================================================
*/


/*
*@ LUAI_MAXCCALLS is the maximum depth for nested C calls (unsigned short).
*/
#define LUAI_MAXCCALLS	200


/*
*@ LUAI_MAXPARSERLEVEL is the maximum number of syntactical nested
** non-terminals in a program.
*/
#define LUAI_MAXPARSERLEVEL	200


/*
*@ LUAI_MAXVARS is the maximum number of local variables per function
** (must be smaller than 250).
*/
#define LUAI_MAXVARS		200


/*
*@ LUAI_MAXUPVALUES is the maximum number of upvalues per function
** (must be smaller than 250).
*/
#define LUAI_MAXUPVALUES	60


/*
*@ LUAI_MAXEXPWHILE is the maximum size of code for expressions
** controling a `while' loop.
*/
#define LUAI_MAXEXPWHILE	100


/*
*@ LUAL_BUFFERSIZE is the buffer size used by the lauxlib buffer system.
*/
#define LUAL_BUFFERSIZE		BUFSIZ

/* }================================================================== */



/*
*@ lua_number2int is a macro to convert lua_Number to int.
** CHANGE that if you know a faster way to convert a lua_Number to
** int (with any rounding method and without throwing errors) in your
** system. In Pentium machines, a naive typecast from doulbe to int
** in C is extremely slow, so any alternative is worth trying.
*/

/* On a GNU/Pentium, resort to assembler */
#if defined(__GNUC__) && defined(__i386)
#define lua_number2int(i,d)	__asm__ ("fistpl %0":"=m"(i):"t"(d):"st")

/* On  Windows/Pentium, resort to assembler */
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

/* on Pentium machines compliant with C99, you can try lrint */
#elif defined (__i386) && defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199900L)
#define lua_number2int(i,d)	((i)=lrint(d))

/* this option always work, but may be slow */
#else
#define lua_number2int(i,d)	((i)=(int)(d))

#endif


/*
*@ lua_number2integer is a macro to convert lua_Number to lua_Integer.
** CHANGE (see lua_number2int).
*/
/* On a GNU or Windows/Pentium, resort to assembler */
#if (defined(__GNUC__) && defined(__i386)) || \
    (defined(_MSC_VER) && defined(_M_IX86))
#define lua_number2integer(i,n)		lua_number2int((i), (n))

/* this option always work, but may be slow */
#else
#define lua_number2integer(i,d)     ((i)=(lua_Integer)(d))

#endif



/*
** {==================================================================
** CHANGE the following definitions only if you want to build Lua
** with a number type different from double. You may also need to
** change lua_number2int & lua_number2integer.
** ===================================================================
*/


/*
*@ LUA_NUMBER is the type of numbers in Lua.
*@ LUAI_UACNUMBER is the result of an `usual argument conversion'
** over a number.
*/
#define LUA_NUMBER	double
#define LUAI_UACNUMBER	LUA_NUMBER


/*
*@ LUA_NUMBER_SCAN is the format for reading numbers.
*@ LUA_NUMBER_FMT is the format for writing numbers.
*@ lua_number2str converts a number to a string.
*@ LUAI_MAXNUMBER2STR is maximum size of previous conversion.
*@ lua_str2number converts a string to a number.
*/
#define LUA_NUMBER_SCAN		"%lf"
#define LUA_NUMBER_FMT		"%.14g"
#define lua_number2str(s,n)	sprintf((s), LUA_NUMBER_FMT, (n))
#define LUAI_MAXNUMBER2STR	32 /* 16 digits, sign, point, and \0 */
#define lua_str2number(s,p)	strtod((s), (p))


/*
*@ The luai_num* macros define the primitive operations over numbers.
*/
#define luai_numadd(a,b)	((a)+(b))
#define luai_numsub(a,b)	((a)-(b))
#define luai_nummul(a,b)	((a)*(b))
#define luai_numdiv(a,b)	((a)/(b))
#define luai_nummod(a,b)	((a) - floor((a)/(b))*(b))
#define luai_numpow(a,b)	pow(a,b)
#define luai_numunm(a)		(-(a))
#define luai_numeq(a,b)		((a)==(b))
#define luai_numlt(a,b)		((a)<(b))
#define luai_numle(a,b)		((a)<=(b))

/* }================================================================== */


/*
*@ LUAI_USER_ALIGNMENT_T is a type that requires maximum alignment.
** CHANGE it if your system requires alignments larger than double. (For
** instance, if your system supports long double and those long doubles
** must be aligned in 16-byte boundaries, then you should add long
** double in the union.) Probably you do not need to change this.
*/
#define LUAI_USER_ALIGNMENT_T	union { double u; void *s; long l; }


/*
*@ LUAI_THROW/LUAI_TRY define how Lua does exception handling.
** CHANGE them if you prefer to use longjmp/setjmp even with C++.  By
** default, Lua handles errors with longjmp/setjmp when compiling as C
** code and with exceptions when compiling as C++ code.
*/
#ifndef __cplusplus
/* default handling with long jumps */
#define LUAI_THROW(L,c)	longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)	if (setjmp((c)->b) == 0) { a }
#define luai_jmpbuf	jmp_buf

#else
/* C++ exceptions */
#define LUAI_THROW(L,c)	throw(c)
#define LUAI_TRY(L,c,a)	try { a } catch(...) \
	{ if ((c)->status == 0) (c)->status = -1; }
#define luai_jmpbuf	int  /* dummy variable */
#endif


/*
*@ LUA_MAXCAPTURES is the maximum number of captures that a pattern
** can do during pattern-matching.
** CHANGE it if you need more captures. This limit is arbitrary.
*/
#define LUA_MAXCAPTURES		32


/*
*@ lua_tmpnam is the function that the OS library uses to create a
** temporary name.
** CHANGE it if you have an alternative to tmpnam (which is considered
** insecure) or if you want the original tmpnam anyway.  By default,
** Lua uses tmpnam except when compiled when POSIX is available, where
** it uses mkstemp.
*/
#ifdef _POSIX_C_SOURCE
#include <unistd.h>
#define LUA_TMPNAMBUFSIZE	32
#define lua_tmpnam(b,e)	{ \
	strcpy(b, "/tmp/lua_XXXXXX"); \
	e = mkstemp(b); \
	if (e != -1) close(e); \
	e = (e == -1); }
#else
#define LUA_TMPNAMBUFSIZE	L_tmpnam
#define lua_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }
#endif


/*
*@ LUA_USE_* define which dynamic-library system Lua should use.
** CHANGE here if Lua has problems choosing the appropriate
** dynamic-library system for your platform (either Windows' DLL, Mac's
** dyld, or Unix's dlopen). If your system is some kind of Unix, there
** is a good chance that LUA_USE_DLOPEN will work for it. (You may need
** to adapt also the makefile.) If you do not want any kind of dynamic
** library, undefine all these options (or just remove these definitions).
*/
#if defined(_WIN32)
#define LUA_USE_DLL
#elif defined(__APPLE__) && defined(__MACH__)
#define LUA_USE_DYLD
#elif defined(__linux) || defined(sun) || defined(sgi) || defined(BSD)
#define LUA_USE_DLOPEN
#endif


/*
*@ lua_lock/lua_unlock are macros for thread synchronization inside the
** Lua core. This is an attempt to simplify the implementation of a
** multithreaded version of Lua.
** CHANGE them only if you know what you are doing. all accesses to
** the global state and to global objects are synchronized.  Because
** threads can read the stack of other threads (when running garbage
** collection), a thread must also synchronize any write-access to its
** own stack.  Unsynchronized accesses are allowed only when reading its
** own stack, or when reading immutable fields from global objects (such
** as string values and udata values).
*/
#define lua_lock(L)	((void) 0)
#define lua_unlock(L)	((void) 0)


/*
*@ lua_threadyield allows a thread switch in appropriate places in the core.
** CHANGE it only if you know what you are doing. (See lua_lock.)
*/
#define luai_threadyield(L)	{lua_unlock(L); lua_lock(L);}


/*
*@ LUAI_EXTRASPACE allows you to add user-specific data in a lua_State
** (the data goes just *before* the lua_State pointer).
** CHANGE (define) this if you really need that. This value must be
** a multiple of the maximum alignment required for your machine.
*/
#define LUAI_EXTRASPACE		0


/*
*@ luai_userstateopen allows user-specific initialization on new threads.
** CHANGE it if you defined LUAI_EXTRASPACE and need to initialize that
** data whenever a new lua_State is created.
*/
#define luai_userstateopen(L)	((void)0)



/* =================================================================== */

/* Local configuration */



#endif
