/*
** TeCGraf - PUC-Rio
** $Id: types.h,v 1.3 1995/02/06 19:32:43 roberto Exp $
*/

#ifndef types_h
#define types_h

#include <limits.h>

#ifndef real
#define real float
#endif

typedef int Bool;  /* boolean values */

#define Byte lua_Byte	/* some systems have Byte as a predefined type */
typedef unsigned char  Byte;  /* unsigned 8 bits */

#define Word lua_Word	/* some systems have Word as a predefined type */
typedef unsigned short Word;  /* unsigned 16 bits */

#define MAX_WORD  (USHRT_MAX-2)  /* maximum value of a word (-2 for safety) */
#define MAX_INT   (INT_MAX-2)  /* maximum value of a int (-2 for safety) */

#define Long lua_Long	/* some systems have Long as a predefined type */
typedef signed long  Long;  /* 32 bits */

typedef unsigned int IntPoint; /* unsigned with same size as a pointer (for hashing) */

#endif
