/*
** $Id: lcode.c,v 1.5 2000/03/03 20:30:47 roberto Exp roberto $
** Code generator for Lua
** See Copyright Notice in lua.h
*/


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
    static Instruction dummy = CREATE_0(ENDCODE);
    return &dummy;  /* no optimizations after an `ENDCODE' */
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
    case PUSHINT: *previous = SETARG_S(*previous, -GETARG_S(*previous)); return;
    case PUSHNUM: *previous = SET_OPCODE(*previous, PUSHNEGNUM); return;
    case PUSHNEGNUM: *previous = SET_OPCODE(*previous, PUSHNUM); return;
    default: luaK_primitivecode(ls, CREATE_0(MINUSOP));
  }
}


static void luaK_gettable (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case PUSHSTRING: *previous = SET_OPCODE(*previous, GETDOTTED); break;
    default: luaK_primitivecode(ls, CREATE_0(GETTABLE));
  }
}


static void luaK_add (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case PUSHINT: *previous = SET_OPCODE(*previous, ADDI); break;
    default: luaK_primitivecode(ls, CREATE_0(ADDOP));
  }
}


static void luaK_sub (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case PUSHINT:
      *previous = SET_OPCODE(*previous, ADDI);
      *previous = SETARG_S(*previous, -GETARG_S(*previous));
      break;
    default: luaK_primitivecode(ls, CREATE_0(SUBOP));
  }
}


void luaK_retcode (LexState *ls, int nlocals, int nexps) {
  Instruction *previous = previous_instruction(ls);
  if (nexps > 0 && GET_OPCODE(*previous) == CALL) {
    LUA_ASSERT(ls->L, GETARG_B(*previous) == MULT_RET, "call should be open");
    *previous = SET_OPCODE(*previous, TAILCALL);
    *previous = SETARG_B(*previous, nlocals);
  }
  else
    luaK_primitivecode(ls, CREATE_U(RETCODE, nlocals));
}


static void luaK_pushnil (LexState *ls, int n) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, n);
  switch(GET_OPCODE(*previous)) {
    case PUSHNIL:
      *previous = SETARG_U(*previous, GETARG_U(*previous)+n);
      break;
    default: luaK_primitivecode(ls, CREATE_U(PUSHNIL, n));
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
  *jmp = SETARG_S(*jmp, dest-(pc+1));
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
  luaK_U(ls, PUSHSTRING, c, 1);
}


#ifndef LOOKBACKNUMS
#define LOOKBACKNUMS	20	/* arbitrary limit */
#endif

static int real_constant (LexState *ls, real r) {
  /* check whether `r' has appeared within the last LOOKBACKNUMS entries */
  TProtoFunc *f = ls->fs->f;
  int c = f->nknum;
  int lim = c < LOOKBACKNUMS ? 0 : c-LOOKBACKNUMS;
  while (--c >= lim)
    if (f->knum[c] == r) return c;
  /* not found; create a new entry */
  luaM_growvector(ls->L, f->knum, f->nknum, 1, real, constantEM, MAXARG_U);
  c = f->nknum++;
  f->knum[c] = r;
  return c;
}


void luaK_number (LexState *ls, real f) {
  if (f <= (real)MAXARG_S && (int)f == f)
    luaK_S(ls, PUSHINT, (int)f, 1);  /* f has a short integer value */
  else
    luaK_U(ls, PUSHNUM, real_constant(ls, f), 1);
}


void luaK_adjuststack (LexState *ls, int n) {
  if (n > 0)
    luaK_U(ls, POP, n, -n);
  else if (n < 0)
    luaK_pushnil(ls, -n);
}


int luaK_lastisopen (LexState *ls) {
  /* check whether last instruction is an (open) function call */
  Instruction *i = previous_instruction(ls);
  if (GET_OPCODE(*i) == CALL) {
    LUA_ASSERT(ls->L, GETARG_B(*i) == MULT_RET, "call should be open");
    return 1;
  }
  else return 0;
}


void luaK_setcallreturns (LexState *ls, int nresults) {
  Instruction *i = previous_instruction(ls);
  if (GET_OPCODE(*i) == CALL) {  /* expression is a function call? */
    LUA_ASSERT(ls->L, GETARG_B(*i) == MULT_RET, "call should be open");
    *i = SETARG_B(*i, nresults);  /* set nresults */
    luaK_deltastack(ls, nresults);  /* push results */
  }
}


static void assertglobal (LexState *ls, int index) {
  luaS_assertglobal(ls->L, ls->fs->f->kstr[index]);
}


void luaK_tostack (LexState *ls, expdesc *var) {
  switch (var->k) {
    case VLOCAL:
      luaK_U(ls, PUSHLOCAL, var->info, 1);
      break;
    case VGLOBAL:
      luaK_U(ls, GETGLOBAL, var->info, 1);
      assertglobal(ls, var->info);  /* make sure that there is a global */
      break;
    case VINDEXED:
      luaK_gettable(ls);
      break;
    case VEXP:
      return;  /* exp result is already on stack */
  }
  var->k = VEXP;
}


void luaK_1tostack (LexState *ls, expdesc *var) {
  if (var->k == VEXP)
    luaK_setcallreturns(ls, 1);  /* call must return 1 value */
  else
    luaK_tostack(ls, var);
}


void luaK_storevar (LexState *ls, const expdesc *var) {
  switch (var->k) {
    case VLOCAL:
      luaK_U(ls, SETLOCAL, var->info, -1);
      break;
    case VGLOBAL:
      luaK_U(ls, SETGLOBAL, var->info, -1);
      assertglobal(ls, var->info);  /* make sure that there is a global */
      break;
    case VINDEXED:
      luaK_0(ls, SETTABLEPOP, -3);
      break;
    default:
      LUA_INTERNALERROR(ls->L, "invalid var kind to store");
  }
}


void luaK_prefix (LexState *ls, int op, expdesc *v) {
  luaK_1tostack(ls, v);
  if (op == '-') luaK_minus(ls);
  else luaK_0(ls, NOTOP, 0);
}


void luaK_infix (LexState *ls, int op, expdesc *v) {
  luaK_1tostack(ls, v);
  if (op == AND)
    v->info = luaK_0(ls, ONFJMP, -1);
  else if (op == OR)
    v->info = luaK_0(ls, ONTJMP, -1);
} 


void luaK_posfix (LexState *ls, int op, expdesc *v1, expdesc *v2) {
  luaK_1tostack(ls, v2);
  switch (op) {
    case AND:  case OR:
      luaK_fixjump(ls, v1->info, luaK_getlabel(ls));
      break;
    case '+': luaK_add(ls); break;
    case '-': luaK_sub(ls); break;
    case '*': luaK_0(ls, MULTOP, -1); break;
    case '/': luaK_0(ls, DIVOP, -1); break;
    case '^': luaK_0(ls, POWOP, -1); break;
    case CONC: luaK_0(ls, CONCOP, -1); break;
    case EQ: luaK_0(ls, EQOP, -1); break;
    case NE: luaK_0(ls, NEQOP, -1); break;
    case '>': luaK_0(ls, GTOP, -1); break;
    case '<': luaK_0(ls, LTOP, -1); break;
    case GE: luaK_0(ls, GEOP, -1); break;
    case LE: luaK_0(ls, LEOP, -1); break;
  }
}

