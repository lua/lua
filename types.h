/*
** TeCGraf - PUC-Rio
** $Id: $
*/

#ifndef types_h
#define types_h

#ifndef real
#define real float
#endif

typedef int Bool;  /* boolean values */

typedef unsigned char  Byte;  /* unsigned 8 bits */

typedef unsigned short Word;  /* unsigned 16 bits */

#define MAX_WORD  0xFFFD  /* maximum value of a word (FFFF-2 for safety) */

typedef signed long  Long;  /* 32 bits */

typedef unsigned int IntPoint; /* unsigned with same size as a pointer (for hashing) */

#endif
