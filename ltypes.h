#ifndef ltypes_h
#define ltypes_h

#include "lua.h"
#include "lobject.h" // Provides DeclaredType, TValue, etc.
#include "llex.h"    // Provides LexState

// Forward declarations for structures from other modules to avoid circular dependencies
struct FuncState; // Defined in lparser.h
struct expdesc;   // Defined in lparser.h


/*
** Type System Utilities
*/

// Get a string representation for a DeclaredType
const char *get_declared_type_name(enum DeclaredType type);

// Infer the type of an expression
enum DeclaredType infer_exp_type(struct FuncState *fs, struct expdesc *e);

// Report a type error
void report_type_error(struct LexState *ls,
                       const char *context,
                       enum DeclaredType expected,
                       enum DeclaredType actual,
                       int line);

#endif
