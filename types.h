/*
** TeCGraf - PUC-Rio
** $Id: types.h,v 1.1 1994/12/20 21:31:01 roberto Exp celes $
*/

#ifndef types_h
#define types_h

#include <limits.h>

#ifndef real
#define real float
#endif

typedef int Bool;  /* boolean values */

typedef unsigned char  Byte;  /* unsigned 8 bits */

typedef unsigned short Word;  /* unsigned 16 bits */

#define MAX_WORD  (USHRT_MAX-2)  /* maximum value of a word (-2 for safety) */
#define MAX_INT   (INT_MAX-2)  /* maximum value of a int (-2 for safety) */

typedef signed long  Long;  /* 32 bits */

typedef unsigned int IntPoint; /* unsigned with same size as a pointer (for hashing) */

#endif
