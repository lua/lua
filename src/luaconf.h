/*
** $Id: luaconf.h,v 1.193 2014/03/21 14:27:16 roberto Exp $
** Configuration file for Lua
** See Copyright Notice in lua.h
*/


#ifndef lconfig_h
#define lconfig_h

#include <limits.h>
#include <stddef.h>


/*
** ==================================================================
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** ===================================================================
@@ LUA_INT_INT / LUA_INT_LONG / LUA_INT_LONGLONG defines size for
@* Lua integers;
@@ LUA_REAL_FLOAT / LUA_REAL_DOUBLE / LUA_REAL_LONGDOUBLE defines size for
@* Lua floats.
**
** These definitions set the numeric types for Lua. Lua should work
** fine with 32-bit or 64-bit integers mixed with 32-bit or 64-bit
** floats. The usual configurations are 64-bit integers and floats (the
** default) and 32-bit integers and floats (Small Lua, for restricted
** hardware).
** =====================================================================
*/
#define LUA_INT_LONGLONG
#define LUA_REAL_DOUBLE


/*
@@ LUA_ANSI controls the use of non-ansi features.
** CHANGE it (define it) if you want Lua to avoid the use of any
** non-ansi feature or library.
*/
#if !defined(LUA_ANSI) && defined(__STRICT_ANSI__)
#define LUA_ANSI
#endif


#if !defined(LUA_ANSI) && defined(_WIN32) && !defined(_WIN32_WCE)
#define LUA_WIN		/* enable goodies for regular Windows platforms */
#endif

#if defined(LUA_WIN)
#define LUA_DL_DLL
#define LUA_USE_AFORMAT		/* assume 'printf' handles 'aA' specifiers */
#endif



#if defined(LUA_USE_LINUX)
#define LUA_USE_C99
#define LUA_USE_POSIX
#define LUA_USE_DLOPEN		/* needs an extra library: -ldl */
#define LUA_USE_READLINE	/* needs some extra libraries */
#endif

#if defined(LUA_USE_MACOSX)
#define LUA_USE_C99
#define LUA_USE_POSIX
#define LUA_USE_DLOPEN		/* does not need -ldl */
#define LUA_USE_READLINE	/* needs an extra library: -lreadline */
#endif


/*
@@ LUA_USE_C99 includes all functionality from C 99.
** CHANGE it (define it) if your system is compatible.
*/
#if defined(LUA_USE_C99)
#define LUA_USE_AFORMAT		/* assume 'printf' handles 'aA' specifiers */
#endif


/*
@@ LUA_USE_POSIX includes all functionality listed as X/Open System
@* Interfaces Extension (XSI).
** CHANGE it (define it) if your system is XSI compatible.
*/
#if defined(LUA_USE_POSIX)
#endif



/*
@@ LUA_PATH_DEFAULT is the default path that Lua uses to look for
@* Lua libraries.
@@ LUA_CPATH_DEFAULT is the default path that Lua uses to look for
@* C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define LUA_LDIR	"!\\lua\\"
#define LUA_CDIR	"!\\"
#define LUA_PATH_DEFAULT  \
		LUA_LDIR"?.lua;"  LUA_LDIR"?\\init.lua;" \
		LUA_CDIR"?.lua;"  LUA_CDIR"?\\init.lua;" \
		".\\?.lua;" ".\\?\\init.lua"
#define LUA_CPATH_DEFAULT \
		LUA_CDIR"?.dll;" LUA_CDIR"loadall.dll;" ".\\?.dll"

#else			/* }{ */

#define LUA_VDIR	LUA_VERSION_MAJOR "." LUA_VERSION_MINOR "/"
#define LUA_ROOT	"/usr/local/"
#define LUA_LDIR	LUA_ROOT "share/lua/" LUA_VDIR
#define LUA_CDIR	LUA_ROOT "lib/lua/" LUA_VDIR
#define LUA_PATH_DEFAULT  \
		LUA_LDIR"?.lua;"  LUA_LDIR"?/init.lua;" \
		LUA_CDIR"?.lua;"  LUA_CDIR"?/init.lua;" \
		"./?.lua;" "./?/init.lua"
#define LUA_CPATH_DEFAULT \
		LUA_CDIR"?.so;" LUA_CDIR"loadall.so;" "./?.so"
#endif			/* } */


/*
@@ LUA_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Lua automatically uses "\".)
*/
#if defined(_WIN32)
#define LUA_DIRSEP	"\\"
#else
#define LUA_DIRSEP	"/"
#endif


/*
@@ LUA_ENV is the name of the variable that holds the current
@@ environment, used to access global names.
** CHANGE it if you do not like this name.
*/
#define LUA_ENV		"_ENV"


/*
@@ LUA_API is a mark for all core API functions.
@@ LUALIB_API is a mark for all auxiliary library functions.
@@ LUAMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** LUA_BUILD_AS_DLL to get it).
*/
#if defined(LUA_BUILD_AS_DLL)	/* { */

#if defined(LUA_CORE) || defined(LUA_LIB)	/* { */
#define LUA_API __declspec(dllexport)
#else						/* }{ */
#define LUA_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define LUA_API		extern

#endif				/* } */


/* more often than not the libs go together with the core */
#define LUALIB_API	LUA_API
#define LUAMOD_API	LUALIB_API


/*
@@ LUAI_FUNC is a mark for all extern functions that are not to be
@* exported to outside modules.
@@ LUAI_DDEF and LUAI_DDEC are marks for all extern (const) variables
@* that are not to be exported to outside modules (LUAI_DDEF for
@* definitions and LUAI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Lua is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define LUAI_FUNC	__attribute__((visibility("hidden"))) extern
#define LUAI_DDEC	LUAI_FUNC
#define LUAI_DDEF	/* empty */

#else				/* }{ */
#define LUAI_FUNC	extern
#define LUAI_DDEC	extern
#define LUAI_DDEF	/* empty */
#endif				/* } */



/*
@@ LUA_QL describes how error messages quote program elements.
** CHANGE it if you want a different appearance.
*/
#define LUA_QL(x)	"'" x "'"
#define LUA_QS		LUA_QL("%s")


/*
@@ LUA_IDSIZE gives the maximum size for the description of the source
@* of a function in debug information.
** CHANGE it if you want a different size.
*/
#define LUA_IDSIZE	60


/*
@@ luai_writestring/luai_writeline define how 'print' prints its results.
** They are only used in libraries and the stand-alone program. (The #if
** avoids including 'stdio.h' everywhere.)
*/
#if defined(LUA_LIB) || defined(lua_c)
#include <stdio.h>
#define luai_writestring(s,l)	fwrite((s), sizeof(char), (l), stdout)
#define luai_writeline()	(luai_writestring("\n", 1), fflush(stdout))
#endif

/*
@@ luai_writestringerror defines how to print error messages.
** (A format string with one argument is enough for Lua...)
*/
#define luai_writestringerror(s,p) \
	(fprintf(stderr, (s), (p)), fflush(stderr))


/*
@@ LUAI_MAXSHORTLEN is the maximum length for short strings, that is,
** strings that are internalized. (Cannot be smaller than reserved words
** or tags for metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/
#define LUAI_MAXSHORTLEN        40



/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ LUA_COMPAT_ALL controls all compatibility options.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(LUA_COMPAT_ALL)	/* { */

/*
@@ LUA_COMPAT_BITLIB controls the presence of library 'bit32'.
*/
#define LUA_COMPAT_BITLIB

/*
@@ LUA_COMPAT_UNPACK controls the presence of global 'unpack'.
** You can replace it with 'table.unpack'.
*/
#define LUA_COMPAT_UNPACK

/*
@@ LUA_COMPAT_LOADERS controls the presence of table 'package.loaders'.
** You can replace it with 'package.searchers'.
*/
#define LUA_COMPAT_LOADERS

/*
@@ macro 'lua_cpcall' emulates deprecated function lua_cpcall.
** You can call your C function directly (with light C functions).
*/
#define lua_cpcall(L,f,u)  \
	(lua_pushcfunction(L, (f)), \
	 lua_pushlightuserdata(L,(u)), \
	 lua_pcall(L,1,0,0))


/*
@@ LUA_COMPAT_LOG10 defines the function 'log10' in the math library.
** You can rewrite 'log10(x)' as 'log(x, 10)'.
*/
#define LUA_COMPAT_LOG10

/*
@@ LUA_COMPAT_LOADSTRING defines the function 'loadstring' in the base
** library. You can rewrite 'loadstring(s)' as 'load(s)'.
*/
#define LUA_COMPAT_LOADSTRING

/*
@@ LUA_COMPAT_MAXN defines the function 'maxn' in the table library.
*/
#define LUA_COMPAT_MAXN

/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
*/
#define lua_strlen(L,i)		lua_rawlen(L, (i))

#define lua_objlen(L,i)		lua_rawlen(L, (i))

#define lua_equal(L,idx1,idx2)		lua_compare(L,(idx1),(idx2),LUA_OPEQ)
#define lua_lessthan(L,idx1,idx2)	lua_compare(L,(idx1),(idx2),LUA_OPLT)

/*
@@ LUA_COMPAT_MODULE controls compatibility with previous
** module functions 'module' (Lua) and 'luaL_register' (C).
*/
#define LUA_COMPAT_MODULE

#endif				/* } */

/* }================================================================== */



/*
@@ LUAI_BITSINT defines the number of bits in an int.
** CHANGE here if Lua cannot automatically detect the number of bits of
** your machine. Probably you do not need to change this.
*/
/* avoid overflows in comparison */
#if INT_MAX-20 < 32760		/* { */
#define LUAI_BITSINT	16
#elif INT_MAX > 2147483640L	/* }{ */
/* int has at least 32 bits */
#define LUAI_BITSINT	32
#else				/* }{ */
#error "you must define LUA_BITSINT with number of bits in an integer"
#endif				/* } */


/*
@@ LUA_INT32 is an signed integer with exactly 32 bits.
@@ LUAI_UMEM is an unsigned integer big enough to count the total
@* memory used by Lua.
@@ LUAI_MEM is a signed integer big enough to count the total memory
@* used by Lua.
** CHANGE here if for some weird reason the default definitions are not
** good enough for your machine. Probably you do not need to change
** this.
*/
#if LUAI_BITSINT >= 32		/* { */
#define LUA_INT32	int
#define LUAI_UMEM	size_t
#define LUAI_MEM	ptrdiff_t
#else				/* }{ */
/* 16-bit ints */
#define LUA_INT32	long
#define LUAI_UMEM	unsigned long
#define LUAI_MEM	long
#endif				/* } */


/*
@@ LUAI_MAXSTACK limits the size of the Lua stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Lua from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
*/
#if LUAI_BITSINT >= 32
#define LUAI_MAXSTACK		1000000
#else
#define LUAI_MAXSTACK		15000
#endif

/* reserve some space for error handling */
#define LUAI_FIRSTPSEUDOIDX	(-LUAI_MAXSTACK - 1000)




/*
@@ LUAL_BUFFERSIZE is the buffer size used by the lauxlib buffer system.
** CHANGE it if it uses too much C-stack space.
*/
#define LUAL_BUFFERSIZE		BUFSIZ


/*
** {==================================================================
** Configuration for Numbers.
** Change these definitions if no predefined LUA_REAL_* / LUA_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ LUA_NUMBER is the floating-point type used by Lua.
**
@@ LUAI_UACNUMBER is the result of an 'usual argument conversion'
@* over a floating number.
**
@@ LUA_NUMBER_FRMLEN is the length modifier for writing floats.
@@ LUA_NUMBER_SCAN is the format for reading floats.
@@ LUA_NUMBER_FMT is the format for writing floats.
@@ lua_number2str converts a float to a string.
**
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations
**
@@ lua_str2number converts a decimal numeric string to a number.
*/

#if defined(LUA_REAL_FLOAT)		/* { single float */

#define LUA_NUMBER	float

#define LUAI_UACNUMBER	double

#define LUA_NUMBER_FRMLEN	""
#define LUA_NUMBER_SCAN		"%f"
#define LUA_NUMBER_FMT		"%.7g"

#define l_mathop(op)		op##f

#define lua_str2number(s,p)	strtof((s), (p))


#elif defined(LUA_REAL_LONGDOUBLE)	/* }{ long double */

#define LUA_NUMBER	long double

#define LUAI_UACNUMBER	long double

#define LUA_NUMBER_FRMLEN	"L"
#define LUA_NUMBER_SCAN		"%Lf"
#define LUA_NUMBER_FMT		"%.19Lg"

#define l_mathop(op)		op##l

#define lua_str2number(s,p)	strtold((s), (p))

#elif defined(LUA_REAL_DOUBLE)		/* }{ double */

#define LUA_NUMBER	double

#define LUAI_UACNUMBER	double

#define LUA_NUMBER_FRMLEN	""
#define LUA_NUMBER_SCAN		"%lf"
#define LUA_NUMBER_FMT		"%.14g"

#define l_mathop(op)		op

#define lua_str2number(s,p)	strtod((s), (p))

#else					/* }{ */

#error "numeric real type not defined"

#endif					/* } */


#if defined(LUA_ANSI)
/* C89 does not support 'opf' variants for math functions */
#undef l_mathop
#define l_mathop(op)		(lua_Number)op
#endif


#if defined(LUA_ANSI) || defined(_WIN32)
/* C89 and Windows do not support 'strtof'... */
#undef lua_str2number
#define lua_str2number(s,p)	((lua_Number)strtod((s), (p)))
#endif


#define l_floor(x)		(l_mathop(floor)(x))

#define lua_number2str(s,n)	sprintf((s), LUA_NUMBER_FMT, (n))


/*
@@ The luai_num* macros define the primitive operations over numbers.
@* They should work for any size of floating numbers.
*/

/* the following operations need the math library */
#if defined(lobject_c) || defined(lvm_c)
#include <math.h>
#define luai_nummod(L,a,b)	((void)L, (a) - l_floor((a)/(b))*(b))
#define luai_numpow(L,a,b)	((void)L, l_mathop(pow)(a,b))
#endif

/* these are quite standard operations */
#if defined(LUA_CORE)
#define luai_numadd(L,a,b)	((a)+(b))
#define luai_numsub(L,a,b)	((a)-(b))
#define luai_nummul(L,a,b)	((a)*(b))
#define luai_numdiv(L,a,b)	((a)/(b))
#define luai_numunm(L,a)	(-(a))
#define luai_numeq(a,b)		((a)==(b))
#define luai_numlt(a,b)		((a)<(b))
#define luai_numle(a,b)		((a)<=(b))
#define luai_numisnan(a)	(!luai_numeq((a), (a)))
#endif


/*
** The following macro checks whether an operation is not safe to be
** performed by the constant folder. It should result in zero only if
** the operation is safe.
*/
#define luai_numinvalidop(op,a,b)	0


/*
@@ LUA_INTEGER is the integer type used by Lua.
**
@@ LUA_UNSIGNED is the unsigned version of LUA_INTEGER.
**
@@ LUA_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ LUA_INTEGER_SCAN is the format for reading integers.
@@ LUA_INTEGER_FMT is the format for writing integers.
@@ lua_integer2str converts an integer to a string.
*/

#if defined(LUA_INT_INT)		/* { int */

#define LUA_INTEGER		int
#define LUA_INTEGER_FRMLEN	""

#elif defined(LUA_INT_LONG)	/* }{ long */

#define LUA_INTEGER		long
#define LUA_INTEGER_FRMLEN	"l"

#elif defined(LUA_INT_LONGLONG)	/* }{ long long */

#if defined(_WIN32)
#define LUA_INTEGER		__int64
#define LUA_INTEGER_FRMLEN	"I64"
#else
#define LUA_INTEGER		long long
#define LUA_INTEGER_FRMLEN	"ll"
#endif

#else				/* }{ */

#error "numeric integer type not defined"

#endif				/* } */


#define LUA_INTEGER_SCAN	"%" LUA_INTEGER_FRMLEN "d"
#define LUA_INTEGER_FMT		"%" LUA_INTEGER_FRMLEN "d"
#define lua_integer2str(s,n)	sprintf((s), LUA_INTEGER_FMT, (n))

#define LUA_UNSIGNED		unsigned LUA_INTEGER

/* }================================================================== */




/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/





#endif

