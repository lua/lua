/*
** ugly.h
** TecCGraf - PUC-Rio
** $Id: $
*/

#ifndef ugly_h
#define ugly_h

/* This enum must have the same order of the array 'reserved' in lex.c */

enum {
 U_and=128,
 U_do,
 U_else,
 U_elseif,
 U_end,
 U_function,
 U_if,
 U_local,
 U_nil,
 U_not,
 U_or,
 U_repeat,
 U_return,
 U_then,
 U_until,
 U_while,
 U_eq = '='+128,
 U_le = '<'+128,
 U_ge = '>'+128,
 U_ne = '~'+128,
 U_sc = '.'+128
};

#endif
