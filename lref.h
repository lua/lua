/*
** $Id: $
** REF mechanism
** See Copyright Notice in lua.h
*/

#ifndef lref_h
#define lref_h

#include "lobject.h"

int luaR_ref (const TObject *o, int lock);
const TObject *luaR_getref (int ref);
void luaR_invalidaterefs (void);


#endif
