/*
** lualoc.h
** TecCGraf - PUC-Rio
** $Id: lualoc.h,v 1.1 1997/07/02 18:45:09 roberto Exp roberto $
*/

#ifndef lualoc_h
#define lualoc_h


#ifndef OLD_ANSI
#include <locale.h>
#else

#define	strcoll(a,b)	strcmp(a,b)
#define setlocale(a,b)	0
#define LC_ALL		0
#define LC_COLLATE	0
#define LC_CTYPE	0
#define LC_MONETARY	0
#define LC_NUMERIC	0
#define LC_TIME		0

#define strerror(e)	"O.S. is unable to define the error"

#endif

#endif
