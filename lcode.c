/*
** $Id: lcode.c,v 1.9 2000/03/10 14:38:10 roberto Exp roberto $
** Code generator for Lua
** See Copyright Notice in lua.h
*/


#define LUA_REENTRANT

#include "lcode.h"
#include "ldo.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstring.h"


void luaK_error (LexState *ls, const char *msg) {
  luaX_error(ls, msg, ls->token);
}


/*
** Returns the address of the previous instruction, for optimizations. 
** If there is a jump target between this and the current instruction,
** returns the address of a dummy instruction to avoid wrong optimizations.
*/
static Instruction *previous_instruction (LexState *ls) {
  FuncState *fs = ls->fs;
  if (fs->pc > fs->lasttarget)  /* no jumps to current position? */
    return &fs->f->code[fs->pc-1];  /* returns previous instruction */
  else {
    static Instruction dummy = CREATE_0(OP_END);
    return &dummy;  /* no optimizations after an `END' */
  }
}


int luaK_primitivecode (LexState *ls, Instruction i) {
  FuncState *fs = ls->fs;
  luaM_growvector(ls->L, fs->f->code, fs->pc, 1, Instruction, codeEM, MAXARG_S);
  fs->f->code[fs->pc] = i;
  return fs->pc++;
}


static void luaK_minus (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  switch(GET_OPCODE(*previous)) {
    case OP_PUSHINT: SETARG_S(*previous, -GETARG_S(*previous)); return;
    case OP_PUSHNUM: SET_OPCODE(*previous, OP_PUSHNEGNUM); return;
    case OP_PUSHNEGNUM: SET_OPCODE(*previous, OP_PUSHNUM); return;
    default: luaK_primitivecode(ls, CREATE_0(OP_MINUS));
  }
}


static void luaK_gettable (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case OP_PUSHSTRING: SET_OPCODE(*previous, OP_GETDOTTED); break;
    default: luaK_primitivecode(ls, CREATE_0(OP_GETTABLE));
  }
}


static void luaK_add (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case OP_PUSHINT: SET_OPCODE(*previous, OP_ADDI); break;
    default: luaK_primitivecode(ls, CREATE_0(OP_ADD));
  }
}


static void luaK_sub (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case OP_PUSHINT:
      SET_OPCODE(*previous, OP_ADDI);
      SETARG_S(*previous, -GETARG_S(*previous));
      break;
    default: luaK_primitivecode(ls, CREATE_0(OP_SUB));
  }
}


static void luaK_conc (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case OP_CONC: SETARG_U(*previous, GETARG_U(*previous)+1); break;
    default: luaK_primitivecode(ls, CREATE_U(OP_CONC, 2));
  }
}


static void luaK_eq (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  if (*previous == CREATE_U(OP_PUSHNIL, 1)) {
    *previous = CREATE_0(OP_NOT);
    luaK_deltastack(ls, -1);  /* undo effect of PUSHNIL */
  }
  else
    luaK_S(ls, OP_IFEQJMP, 0, -2);
}


static void luaK_neq (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  if (*previous == CREATE_U(OP_PUSHNIL, 1)) {
    ls->fs->pc--;  /* remove PUSHNIL */
    luaK_deltastack(ls, -1);  /* undo effect of PUSHNIL */
  }
  else
    luaK_S(ls, OP_IFNEQJMP, 0, -2);
}


void luaK_retcode (LexState *ls, int nlocals, int nexps) {
  Instruction *previous = previous_instruction(ls);
  if (nexps > 0 && GET_OPCODE(*previous) == OP_CALL) {
    LUA_ASSERT(ls->L, GETARG_B(*previous) == MULT_RET, "call should be open");
    SET_OPCODE(*previous, OP_TAILCALL);
    SETARG_B(*previous, nlocals);
  }
  else
    luaK_primitivecode(ls, CREATE_U(OP_RETURN, nlocals));
}


static void luaK_pushnil (LexState *ls, int n) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, n);
  switch(GET_OPCODE(*previous)) {
    case OP_PUSHNIL: SETARG_U(*previous, GETARG_U(*previous)+n); break;
    default: luaK_primitivecode(ls, CREATE_U(OP_PUSHNIL, n));
  }
}


int luaK_code (LexState *ls, Instruction i, int delta) {
  luaK_deltastack(ls, delta);
  return luaK_primitivecode(ls, i);
}


void luaK_fixjump (LexState *ls, int pc, int dest) {
  FuncState *fs = ls->fs;
  Instruction *jmp = &fs->f->code[pc];
  /* jump is relative to position following jump instruction */
  SETARG_S(*jmp, dest-(pc+1));
}


/*
** returns current `pc' and marks it as a jump target (to avoid wrong
** optimizations with consecutive instructions not in the same basic block).
*/
int luaK_getlabel (LexState *ls) {
  FuncState *fs = ls->fs;
  fs->lasttarget = fs->pc;
  return fs->pc;
}


void luaK_deltastack (LexState *ls, int delta) {
  FuncState *fs = ls->fs;
  fs->stacksize += delta;
  if (delta > 0 && fs->stacksize > fs->f->maxstacksize) {
    if (fs->stacksize > MAXSTACK)
      luaK_error(ls, "function or expression too complex");
    fs->f->maxstacksize = fs->stacksize;
  }
}


void luaK_kstr (LexState *ls, int c) {
  luaK_U(ls, OP_PUSHSTRING, c, 1);
}


#ifndef LOOKBACKNUMS
#define LOOKBACKNUMS	20	/* arbitrary limit */
#endif

static int real_constant (LexState *ls, Number r) {
  /* check whether `r' has appeared within the last LOOKBACKNUMS entries */
  Proto *f = ls->fs->f;
  int c = f->nknum;
  int lim = c < LOOKBACKNUMS ? 0 : c-LOOKBACKNUMS;
  while (--c >= lim)
    if (f->knum[c] == r) return c;
  /* not found; create a new entry */
  luaM_growvector(ls->L, f->knum, f->nknum, 1, Number, constantEM, MAXARG_U);
  c = f->nknum++;
  f->knum[c] = r;
  return c;
}


void luaK_number (LexState *ls, Number f) {
  if (f <= (Number)MAXARG_S && (int)f == f)
    luaK_S(ls, OP_PUSHINT, (int)f, 1);  /* f has a short integer value */
  else
    luaK_U(ls, OP_PUSHNUM, real_constant(ls, f), 1);
}


void luaK_adjuststack (LexState *ls, int n) {
  if (n > 0)
    luaK_U(ls, OP_POP, n, -n);
  else if (n < 0)
    luaK_pushnil(ls, -n);
}


int luaK_lastisopen (LexState *ls) {
  /* check whether last instruction is an (open) function call */
  Instruction *i = previous_instruction(ls);
  if (GET_OPCODE(*i) == OP_CALL) {
    LUA_ASSERT(ls->L, GETARG_B(*i) == MULT_RET, "call should be open");
    return 1;
  }
  else return 0;
}


void luaK_setcallreturns (LexState *ls, int nresults) {
  Instruction *i = previous_instruction(ls);
  if (GET_OPCODE(*i) == OP_CALL) {  /* expression is a function call? */
    LUA_ASSERT(ls->L, GETARG_B(*i) == MULT_RET, "call should be open");
    SETARG_B(*i, nresults);  /* set nresults */
    luaK_deltastack(ls, nresults);  /* push results */
  }
}


static void assertglobal (LexState *ls, int index) {
  luaS_assertglobal(ls->L, ls->fs->f->kstr[index]);
}


static int discharge (LexState *ls, expdesc *var) {
  switch (var->k) {
    case VLOCAL:
      luaK_U(ls, OP_PUSHLOCAL, var->u.index, 1);
      break;
    case VGLOBAL:
      luaK_U(ls, OP_GETGLOBAL, var->u.index, 1);
      assertglobal(ls, var->u.index);  /* make sure that there is a global */
      break;
    case VINDEXED:
      luaK_gettable(ls);
      break;
    case VEXP:
      return 0;  /* nothing to do */
  }
  var->k = VEXP;
  var->u.l.t = var->u.l.f = 0;
  return 1;
}


static void discharge1 (LexState *ls, expdesc *var) {
  discharge(ls, var);
 /* if it has jumps it is already discharged */
  if (var->u.l.t == 0 && var->u.l.f  == 0)
    luaK_setcallreturns(ls, 1);  /* call must return 1 value */
}
  

void luaK_storevar (LexState *ls, const expdesc *var) {
  switch (var->k) {
    case VLOCAL:
      luaK_U(ls, OP_SETLOCAL, var->u.index, -1);
      break;
    case VGLOBAL:
      luaK_U(ls, OP_SETGLOBAL, var->u.index, -1);
      assertglobal(ls, var->u.index);  /* make sure that there is a global */
      break;
    case VINDEXED:
      luaK_0(ls, OP_SETTABLEPOP, -3);
      break;
    default:
      LUA_INTERNALERROR(ls->L, "invalid var kind to store");
  }
}


static OpCode invertjump (OpCode op) {
  switch (op) {
    case OP_IFNEQJMP: return OP_IFEQJMP;
    case OP_IFEQJMP: return OP_IFNEQJMP;
    case OP_IFLTJMP: return OP_IFGEJMP;
    case OP_IFLEJMP: return OP_IFGTJMP;
    case OP_IFGTJMP: return OP_IFLEJMP;
    case OP_IFGEJMP: return OP_IFLTJMP;
    case OP_IFTJMP: case OP_ONTJMP:  return OP_IFFJMP;
    case OP_IFFJMP: case OP_ONFJMP:  return OP_IFTJMP;
    default:
      LUA_INTERNALERROR(NULL, "invalid jump instruction");
      return OP_END;  /* to avoid warnings */
  }
}


static void luaK_jump (LexState *ls, OpCode jump) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  if (*previous == CREATE_0(OP_NOT))
    *previous = CREATE_S(invertjump(jump), 0);
  else
    luaK_primitivecode(ls, CREATE_S(jump, 0));
}


static void insert_last (FuncState *fs, int *list) {
  int first = *list;
  *list = fs->pc-1;  /* insert last instruction in the list */
  if (first == 0)
    SETARG_S(fs->f->code[*list], 0);
  else
    SETARG_S(fs->f->code[*list], first-fs->pc);
}


static void luaK_patchlistaux (LexState *ls, int list, int target,
                               OpCode special, int special_target) {
  if (list != 0) {
    Instruction *code = ls->fs->f->code;
    for (;;) {
      Instruction *i = &code[list];
      OpCode op = GET_OPCODE(*i);
      int next = GETARG_S(*i);
      if (op == special)  /* this `op' already has a value */
        SETARG_S(*i, special_target-(list+1));
      else {
        SETARG_S(*i, target-(list+1));  /* do the patch */
        if (op == OP_ONTJMP)  /* remove eventual values */
          SET_OPCODE(*i, OP_IFTJMP);
        else if (op == OP_ONFJMP)
          SET_OPCODE(*i, OP_IFFJMP);
      }
      if (next == 0) return;
      list += next+1;
    }
  }
}


void luaK_patchlist (LexState *ls, int list, int target) {
  luaK_patchlistaux(ls, list, target, OP_END, 0);
}


static int need_value (FuncState *fs, int list, OpCode hasvalue) {
  if (list == 0) return 0;
  else {  /* check whether list has a jump without a value */
    Instruction *code = fs->f->code;
    for (;;) {
      int next = GETARG_S(code[list]);
      if (GET_OPCODE(code[list]) != hasvalue) return 1;
      else if (next == 0) return 0;
      list += next+1;
    }
  }
}


static void concatlists (LexState *ls, int *l1, int l2) {
  if (*l1 == 0)
    *l1 = l2;
  else if (l2 != 0) {
    FuncState *fs = ls->fs;
    int list = *l1;
    for (;;) {  /* traverse `l1' */
      int next = GETARG_S(fs->f->code[list]);
      if (next == 0) {  /* end of list? */
        SETARG_S(fs->f->code[list], l2-(list+1));  /* end points to `l2' */
        return;
      }
      list += next+1;
    }
  }
}


void luaK_goiftrue (LexState *ls, expdesc *v, int keepvalue) {
  FuncState *fs = ls->fs;
  Instruction *previous;
  discharge1(ls, v);
  previous = &fs->f->code[fs->pc-1];
  if (ISJUMP(GET_OPCODE(*previous)))
    SET_OPCODE(*previous, invertjump(GET_OPCODE(*previous)));
  else {
    OpCode jump = keepvalue ? OP_ONFJMP : OP_IFFJMP;
    luaK_jump(ls, jump);
  }
  insert_last(fs, &v->u.l.f);
  luaK_patchlist(ls, v->u.l.t, luaK_getlabel(ls));
  v->u.l.t = 0;
}


void luaK_goiffalse (LexState *ls, expdesc *v, int keepvalue) {
  FuncState *fs = ls->fs;
  Instruction previous;
  discharge1(ls, v);
  previous = fs->f->code[fs->pc-1];
  if (!ISJUMP(GET_OPCODE(previous))) {
    OpCode jump = keepvalue ? OP_ONTJMP : OP_IFTJMP;
    luaK_jump(ls, jump);
  }
  insert_last(fs, &v->u.l.t);
  luaK_patchlist(ls, v->u.l.f, luaK_getlabel(ls));
  v->u.l.f = 0;
}


void luaK_tostack (LexState *ls, expdesc *v, int onlyone) {
  if (discharge(ls, v)) return;
  else {  /* is an expression */
    FuncState *fs = ls->fs;
    OpCode previous = GET_OPCODE(fs->f->code[fs->pc-1]);
    if (!ISJUMP(previous) && v->u.l.f == 0 && v->u.l.t == 0) {
      /* it is an expression without jumps */
      if (onlyone && v->k == VEXP)
        luaK_setcallreturns(ls, 1);  /* call must return 1 value */
      return;
    }
    else {  /* expression has jumps... */
      int p_nil = 0;  /* position of an eventual PUSHNIL */
      int p_1 = 0;  /* position of an eventual PUSHINT */
      int final;  /* position after whole expression */
      if (ISJUMP(previous)) {
        insert_last(fs, &v->u.l.t);  /* put `previous' in true list */
        p_nil = luaK_0(ls, OP_PUSHNILJMP, 0);
        p_1 = luaK_S(ls, OP_PUSHINT, 1, 1);
      }
      else {  /* still may need a PUSHNIL or a PUSHINT */
        int need_nil = need_value(fs, v->u.l.f, OP_ONFJMP);
        int need_1 = need_value(fs, v->u.l.t, OP_ONTJMP);
        if (need_nil && need_1) {
          luaK_S(ls, OP_JMP, 2, 0);  /* skip both pushes */
          p_nil = luaK_0(ls, OP_PUSHNILJMP, 0);
          p_1 = luaK_S(ls, OP_PUSHINT, 1, 0);
        }
        else if (need_nil || need_1) {
          luaK_S(ls, OP_JMP, 1, 0);  /* skip one push */
          if (need_nil)
            p_nil = luaK_U(ls, OP_PUSHNIL, 1, 0);
          else  /* need_1 */
            p_1 = luaK_S(ls, OP_PUSHINT, 1, 0);
        }
      }
      final = luaK_getlabel(ls);
      luaK_patchlistaux(ls, v->u.l.f, p_nil, OP_ONFJMP, final);
      luaK_patchlistaux(ls, v->u.l.t, p_1, OP_ONTJMP, final);
      v->u.l.f = v->u.l.t = 0;
    }
  }
}


void luaK_prefix (LexState *ls, int op, expdesc *v) {
  if (op == '-') {
    luaK_tostack(ls, v, 1);
    luaK_minus(ls);
  }
  else {  /* op == NOT */
    FuncState *fs = ls->fs;
    Instruction *previous;
    discharge1(ls, v);
    previous = &fs->f->code[fs->pc-1];
    if (ISJUMP(GET_OPCODE(*previous)))
      SET_OPCODE(*previous, invertjump(GET_OPCODE(*previous)));
    else
      luaK_0(ls, OP_NOT, 0);
    /* interchange true and false lists */
    { int temp = v->u.l.f; v->u.l.f = v->u.l.t; v->u.l.t = temp; }
  }
}


void luaK_infix (LexState *ls, int op, expdesc *v) {
  if (op == TK_AND)
    luaK_goiftrue(ls, v, 1);
  else if (op == TK_OR)
    luaK_goiffalse(ls, v, 1);
  else
    luaK_tostack(ls, v, 1);  /* all other binary operators need a value */
} 


void luaK_posfix (LexState *ls, int op, expdesc *v1, expdesc *v2) {
  if (op == TK_AND) {
    LUA_ASSERT(ls->L, v1->u.l.t == 0, "list must be closed");
    discharge1(ls, v2);
    v1->u.l.t = v2->u.l.t;
    concatlists(ls, &v1->u.l.f, v2->u.l.f);
  }
  else if (op == TK_OR) {
    LUA_ASSERT(ls->L, v1->u.l.f == 0, "list must be closed");
    discharge1(ls, v2);
    v1->u.l.f = v2->u.l.f;
    concatlists(ls, &v1->u.l.t, v2->u.l.t);
  }
  else {
    luaK_tostack(ls, v2, 1);  /* `v2' must be a value */
    switch (op) {
      case '+': luaK_add(ls); break;
      case '-': luaK_sub(ls); break;
      case '*': luaK_0(ls, OP_MULT, -1); break;
      case '/': luaK_0(ls, OP_DIV, -1); break;
      case '^': luaK_0(ls, OP_POW, -1); break;
      case TK_CONC: luaK_conc(ls); break;
      case TK_EQ: luaK_eq(ls); break;
      case TK_NE: luaK_neq(ls); break;
      case '>': luaK_S(ls, OP_IFGTJMP, 0, -2); break;
      case '<': luaK_S(ls, OP_IFLTJMP, 0, -2); break;
      case TK_GE: luaK_S(ls, OP_IFGEJMP, 0, -2); break;
      case TK_LE: luaK_S(ls, OP_IFLEJMP, 0, -2); break;
    }
  }
}

