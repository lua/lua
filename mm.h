/*
** mm.h
** Waldemar Celes Filho
** Sep 16, 1992
*/


#ifndef mm_h
#define mm_h

#include <stdlib.h>

#ifdef _MM_

/* switch off the debugger functions */
#define malloc(s)	MmMalloc(s,__FILE__,__LINE__)
#define calloc(n,s)	MmCalloc(n,s,__FILE__,__LINE__)
#define realloc(a,s)	MmRealloc(a,s,__FILE__,__LINE__,#a)
#define free(a)		MmFree(a,__FILE__,__LINE__,#a)
#define strdup(s)	MmStrdup(s,__FILE__,__LINE__)
#endif

typedef void (*Ferror) (char *);

/* Exported functions */
void  MmInit (Ferror f, Ferror w);
void *MmMalloc (unsigned size, char *file, int line);
void *MmCalloc (unsigned n, unsigned size, char *file, int line);
void  MmFree (void *a, char *file, int line, char *var);
void *MmRealloc (void *old, unsigned size, char *file, int line, char *var);
char *MmStrdup (char *s, char *file, int line);
unsigned MmGetBytes (void);
void MmListAllocated (void);
void MmCheck (void);
void MmStatistics (void);


#endif

