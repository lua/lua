/*
** $Id: lprefix.h,v 1.1 2014/11/03 15:12:44 roberto Exp $
** Definitions for Lua code that must come before any other header file
** See Copyright Notice in lua.h
*/

#ifndef lprefix_h
#define lprefix_h


/*
** Allows POSIX/XSI stuff
*/
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE           600
#endif

/*
** Allows manipulation of large files in gcc and some other compilers
*/
#if !defined(_FILE_OFFSET_BITS)
#define _LARGEFILE_SOURCE       1
#define _FILE_OFFSET_BITS       64
#endif


/*
** Windows stuff
*/
#if defined(_WIN32) 	/* { */

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS  /* avoid warnings about ISO C functions */
#endif

#endif			/* } */

#endif

