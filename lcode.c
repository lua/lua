/*
** $Id: lcode.c,v 1.1 2000/02/22 13:30:11 roberto Exp roberto $
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


static Instruction *last_i (FuncState *fs) {
  static Instruction dummy = SET_OPCODE(0, ENDCODE);
  if (fs->last_pc < 0)
    return &dummy;
  else
    return &fs->f->code[fs->last_pc];
}


int luaK_primitivecode (LexState *ls, Instruction i) {
  FuncState *fs = ls->fs;
  luaM_growvector(ls->L, fs->f->code, fs->pc, 1, Instruction, codeEM, MAXARG_S);
  fs->f->code[fs->pc] = i;
  return fs->pc++;
}



int luaK_code (LexState *ls, Instruction i) {
  FuncState *fs = ls->fs;
  Instruction *last = last_i(fs);
  switch (GET_OPCODE(i)) {

    case MINUSOP:
      switch(GET_OPCODE(*last)) {
        case PUSHINT: *last = SETARG_S(*last, -GETARG_S(*last)); break;
        case PUSHNUM: *last = SET_OPCODE(*last, PUSHNEGNUM); break;
        case PUSHNEGNUM: *last = SET_OPCODE(*last, PUSHNUM); break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    case GETTABLE:
      switch(GET_OPCODE(*last)) {
        case PUSHSTRING: *last = SET_OPCODE(*last, GETDOTTED); break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    case RETCODE:
      switch(GET_OPCODE(*last)) {
        case CALL:
          *last = SET_OPCODE(*last, TAILCALL);
          *last = SETARG_B(*last, GETARG_U(i));
          break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    case ADDOP:
      switch(GET_OPCODE(*last)) {
        case PUSHINT: *last = SET_OPCODE(*last, ADDI); break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    case SUBOP:
      switch(GET_OPCODE(*last)) {
        case PUSHINT:
          *last = SET_OPCODE(*last, ADDI);
          *last = SETARG_S(*last, -GETARG_S(*last));
          break;
        default: fs->last_pc = luaK_primitivecode(ls, i);
      }
      break;

    default: fs->last_pc = luaK_primitivecode(ls, i);
  }
  return fs->last_pc;
}


void luaK_fixjump (LexState *ls, int pc, int dest) {
  FuncState *fs = ls->fs;
  Instruction *jmp = &fs->f->code[pc];
  /* jump is relative to position following jump instruction */
  *jmp = SETARG_S(*jmp, dest-(pc+1));
  fs->last_pc = pc;
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


static int aux_code (LexState *ls, OpCode op, Instruction i, int delta) {
  luaK_deltastack(ls, delta);
  return luaK_code(ls, SET_OPCODE(i, op));
}


int luaK_0 (LexState *ls, OpCode op, int delta) {
  return aux_code(ls, op, 0, delta);
}


int luaK_U (LexState *ls, OpCode op, int u, int delta) {
  Instruction i = SETARG_U(0, u);
  return aux_code(ls, op, i, delta);
}


int luaK_S (LexState *ls, OpCode op, int s, int delta) {
  Instruction i = SETARG_S(0, s);
  return aux_code(ls, op, i, delta);
}


int luaK_AB (LexState *ls, OpCode op, int a, int b, int delta) {
  Instruction i = SETARG_A(0, a);
  i = SETARG_B(i, b);
  return aux_code(ls, op, i, delta);
}


int luaK_kstr (LexState *ls, int c) {
  return luaK_U(ls, PUSHSTRING, c, 1);
}


#ifndef LOOKBACKNUMS
#define LOOKBACKNUMS	20	/* arbitrary limit */
#endif

static int real_constant (LexState *ls, real r) {
  /* check whether `r' has appeared within the last LIM entries */
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


int luaK_number (LexState *ls, real f) {
  if (f <= (real)MAXARG_S && (int)f == f)
    return luaK_S(ls, PUSHINT, (int)f, 1);  /* f has a short integer value */
  else
    return luaK_U(ls, PUSHNUM, real_constant(ls, f), 1);
}


int luaK_adjuststack (LexState *ls, int n) {
  if (n > 0)
    return luaK_U(ls, POP, n, -n);
  else if (n < 0)
    return luaK_U(ls, PUSHNIL, (-n)-1, -n);
  else return 0;
}


int luaK_iscall (LexState *ls, int pc) {
  return (GET_OPCODE(ls->fs->f->code[pc]) == CALL);
}


void luaK_setcallreturns (LexState *ls, int pc, int nresults) {
  if (luaK_iscall(ls, pc)) {  /* expression is a function call? */
    Instruction *i = &ls->fs->f->code[pc];
    int old_nresults = GETARG_B(*i);
    if (old_nresults != MULT_RET)
      luaK_deltastack(ls, -old_nresults);  /* pop old nresults */
    *i = SETARG_B(*i, nresults);  /* set nresults */
    if (nresults != MULT_RET)
      luaK_deltastack(ls, nresults);  /* push results */
  }
}


static void assertglobal (LexState *ls, int index) {
  luaS_assertglobal(ls->L, ls->fs->f->kstr[index]);
}


void luaK_2stack (LexState *ls, expdesc *var) {
  switch (var->k) {
    case VLOCAL:
      var->info = luaK_U(ls, PUSHLOCAL, var->info, 1);
      break;
    case VGLOBAL:
      assertglobal(ls, var->info);  /* make sure that there is a global */
      var->info = luaK_U(ls, GETGLOBAL, var->info, 1);
      break;
    case VINDEXED:
      var->info = luaK_0(ls, GETTABLE, -1);
      break;
    case VEXP:
      luaK_setcallreturns(ls, var->info, 1);  /* call must return 1 value */
      break;
  }
  var->k = VEXP;
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
  luaK_2stack(ls, v);
  if (op == '-')
    v->info = luaK_0(ls, MINUSOP, 0);
  else
    v->info = luaK_0(ls, NOTOP, 0);
}


void luaK_infix (LexState *ls, expdesc *v) {
  luaK_2stack(ls, v);
  if (ls->token == AND)
    v->info = luaK_0(ls, ONFJMP, -1);
  else if (ls->token == OR)
    v->info = luaK_0(ls, ONTJMP, -1);
} 


void luaK_posfix (LexState *ls, int op, expdesc *v1, expdesc *v2) {
  luaK_2stack(ls, v2);
  switch (op) {
    case AND:  case OR:
      luaK_fixjump(ls, v1->info, ls->fs->pc);
      break;
    case '+': v1->info = luaK_0(ls, ADDOP, -1); break;
    case '-': v1->info = luaK_0(ls, SUBOP, -1); break;
    case '*': v1->info = luaK_0(ls, MULTOP, -1); break;
    case '/': v1->info = luaK_0(ls, DIVOP, -1); break;
    case '^': v1->info = luaK_0(ls, POWOP, -1); break;
    case CONC: v1->info = luaK_0(ls, CONCOP, -1); break;
    case EQ: v1->info = luaK_0(ls, EQOP, -1); break;
    case NE: v1->info = luaK_0(ls, NEQOP, -1); break;
    case '>': v1->info = luaK_0(ls, GTOP, -1); break;
    case '<': v1->info = luaK_0(ls, LTOP, -1); break;
    case GE: v1->info = luaK_0(ls, GEOP, -1); break;
    case LE: v1->info = luaK_0(ls, LEOP, -1); break;
  }
}

