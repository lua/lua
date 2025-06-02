#define ltypes_c
#define LUA_CORE

#include "ltypes.h"
#include "lparser.h" // For FuncState, expdesc, Vardesc, etc.
#include "ldebug.h"  // For luaG_runerror, luaG_addinfo
#include "lstate.h"  // For lua_State
#include "lstring.h" // For TString, getstr
#include "llex.h"    // For LexState, luaX_syntaxerror (though report_type_error uses luaG_runerror)
#include "lopcodes.h"// For opcodes (future use in infer_exp_type)
#include "lvm.h"     // For luaV_execute and other VM operations (future use)


const char *get_declared_type_name(enum DeclaredType type) {
    switch (type) {
        case DEC_UNTYPED: return "untyped";
        case DEC_NUM:     return "num";
        case DEC_STR:     return "str";
        case DEC_BOOL:    return "bool";
        case DEC_NIL:     return "nil";
        case DEC_VOID:    return "void";
        // case DEC_TABLE: return "table"; // Example for future
        default:          return "unknown_type";
    }
}

void report_type_error(struct LexState *ls,
                       const char *context,
                       enum DeclaredType expected,
                       enum DeclaredType actual,
                       int line) {
    const char *source_name = getstr(ls->source);
    // Using luaG_addinfo then luaD_throw might be better for consistency if this is a syntax/compile-time error
    // For now, using luaG_runerror as a general error reporting function
    // luaX_syntaxerror is also an option if we want it to behave exactly like other parsing errors.
    // Let's use luaX_syntaxerror for parsing phase errors.
    if (line <= 0) { // If line info is not available from context, use current parser line
        line = ls->linenumber;
    }
    luaX_syntaxerror(ls, luaO_pushfstring(ls->L, "%s:%d: type error: %s: expected %s, got %s",
        source_name, line, context,
        get_declared_type_name(expected), get_declared_type_name(actual)));
}

// Initial, simplified version of infer_exp_type
enum DeclaredType infer_exp_type(struct FuncState *fs, struct expdesc *e) {
    if (e == NULL) return DEC_UNTYPED;

    switch (e->k) {
        case VNIL:
            return DEC_NIL;
        case VTRUE:
        case VFALSE:
            return DEC_BOOL;
        case VKINT:
        case VKFLT: // Lua uses VKFLT for all numbers in k array, VKINT for literals
            return DEC_NUM;
        case VKSTR: // String literal stored in constants table
            return DEC_STR;
        
        case VLOCAL: {
            // e->u.var.vidx is the variable's index in the 'actvar' array,
            // relative to the current function's 'firstlocal'.
            Vardesc *vd = &fs->ls->dyd->actvar.arr[fs->firstlocal + e->u.var.vidx];
            return (enum DeclaredType)vd->vd.declaredtype;
        }

        case VUPVAL: {
            // Upvaldesc (fs->f->upvalues[e->u.info]) does not directly store the variable's DeclaredType.
            // It stores 'kind', 'idx', and 'instack' to find the upvalue.
            // Actual type would need to be traced to the original LocVar in an outer Proto.
            return DEC_UNTYPED; // Placeholder as per plan
        }

        // Simplified handling for operations for now.
        // Lua's 'expdesc' after parsing an operation typically has k = VRELOCABLE or VNONRELOC.
        // The specific operation (e.g. OP_ADD) is in the instruction stream.
        // The plan's direct use of VADD, VCONCAT in switch(e->k) is not how it works.
        // These will fall into default or specific VRELOCABLE/VNONRELOC cases.
        // For this step, we mainly focus on literals and variable access.
        
        case VJMP: // Result of a jump (e.g. short-circuit 'and'/'or')
            // The type is the type of the expression that didn't jump.
            // This is complex; for now, untyped.
            return DEC_UNTYPED;

        // VNOT is an UnOpr, not an expkind. Result of 'not x' will be VRELOCABLE/VNONRELOC.
        // case VNOT: // Logical not always results in a boolean
        //     return DEC_BOOL; 
        
        case VCALL:
             // Future: Look up Proto for function e->u.call.func, get its returntype.
            return DEC_UNTYPED; // Placeholder

        // For other expression kinds that imply a value will be on the stack or in a register
        // (like VRELOCABLE, VNONRELOC from operations, VINDEXED etc.),
        // without deeper analysis, we can't know the type yet.
        case VINDEXED:
        case VINDEXUP:
        case VINDEXI:
        case VINDEXSTR:
        case VRELOC: // Corrected from VRELOCABLE
        case VNONRELOC: // This includes results of many operations.
        default:
            return DEC_UNTYPED;
    }
}
