/*
** Libraries to be used in LUA programs
** Grupo de Tecnologia em Computacao Grafica
** TeCGraf - PUC-Rio
** $Id: lualib.h,v 1.1 1993/12/17 19:01:46 celes Exp celes $
*/

#ifndef lualib_h
#define lualib_h

#ifdef __cplusplus
extern "C"
{
#endif
 

void iolib_open   (void);
void strlib_open  (void);
void mathlib_open (void);

#ifdef __cplusplus
}
#endif

#endif

