/*
** $Id: lcode.c,v 1.3 2000/03/03 14:58:26 roberto Exp roberto $
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


static Instruction *last_i (LexState *ls, expdesc *v) {
  FuncState *fs = ls->fs;
  int last_pc = (v->info != NOJUMPS) ? v->info : fs->pc-1;
  return &fs->f->code[last_pc];
}


int luaK_primitivecode (LexState *ls, Instruction i) {
  FuncState *fs = ls->fs;
  luaM_growvector(ls->L, fs->f->code, fs->pc, 1, Instruction, codeEM, MAXARG_S);
  fs->f->code[fs->pc] = i;
  return fs->pc++;
}


static void luaK_minus (LexState *ls, expdesc *v) {
  Instruction *last = last_i(ls, v);
  switch(GET_OPCODE(*last)) {
    case PUSHINT: *last = SETARG_S(*last, -GETARG_S(*last)); return;
    case PUSHNUM: *last = SET_OPCODE(*last, PUSHNEGNUM); return;
    case PUSHNEGNUM: *last = SET_OPCODE(*last, PUSHNUM); return;
    default: luaK_primitivecode(ls, CREATE_0(MINUSOP));
  }
}


static void luaK_gettable (LexState *ls, expdesc *v) {
  Instruction *last = last_i(ls, v);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*last)) {
    case PUSHSTRING: *last = SET_OPCODE(*last, GETDOTTED); break;
    default: luaK_primitivecode(ls, CREATE_0(GETTABLE));
  }
}


static void luaK_add (LexState *ls, expdesc *v) {
  Instruction *last = last_i(ls, v);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*last)) {
    case PUSHINT: *last = SET_OPCODE(*last, ADDI); break;
    default: luaK_primitivecode(ls, CREATE_0(ADDOP));
  }
}


static void luaK_sub (LexState *ls, expdesc *v) {
  Instruction *last = last_i(ls, v);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*last)) {
    case PUSHINT:
      *last = SET_OPCODE(*last, ADDI);
      *last = SETARG_S(*last, -GETARG_S(*last));
      break;
    default: luaK_primitivecode(ls, CREATE_0(SUBOP));
  }
}


void luaK_retcode (LexState *ls, int nlocals, listdesc *e) {
  if (e->n > 0 && luaK_iscall(ls, e->info)) {
    Instruction *last = &ls->fs->f->code[ls->fs->pc-1];
    *last = SET_OPCODE(*last, TAILCALL);
    *last = SETARG_B(*last, nlocals);
  }
  else
    luaK_U(ls, RETCODE, nlocals, 0);
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
    luaK_U(ls, PUSHNIL, (-n)-1, -n);
}


int luaK_iscall (LexState *ls, int hasjumps) {
  if (hasjumps) return 0;   /* a call cannot have internal jumps */
  else  /* check whether last instruction is a function call */
    return (GET_OPCODE(ls->fs->f->code[ls->fs->pc-1]) == CALL);
}


void luaK_setcallreturns (LexState *ls, int hasjumps, int nresults) {
  if (!hasjumps) {  /* if `hasjumps' cannot be a function call */
    Instruction *i = &ls->fs->f->code[ls->fs->pc-1];
    if (GET_OPCODE(*i) == CALL) {  /* expression is a function call? */
      int old_nresults = GETARG_B(*i);
      if (old_nresults != MULT_RET)
        luaK_deltastack(ls, -old_nresults);  /* pop old nresults */
      *i = SETARG_B(*i, nresults);  /* set nresults */
      if (nresults != MULT_RET)
        luaK_deltastack(ls, nresults);  /* push results */
    }
  }
}


static void assertglobal (LexState *ls, int index) {
  luaS_assertglobal(ls->L, ls->fs->f->kstr[index]);
}


void luaK_2stack (LexState *ls, expdesc *var) {
  switch (var->k) {
    case VLOCAL:
      luaK_U(ls, PUSHLOCAL, var->info, 1);
      break;
    case VGLOBAL:
      luaK_U(ls, GETGLOBAL, var->info, 1);
      assertglobal(ls, var->info);  /* make sure that there is a global */
      break;
    case VINDEXED:
      luaK_gettable(ls, var);
      break;
    case VEXP:
      luaK_setcallreturns(ls, var->info, 1);  /* call must return 1 value */
      return;  /* does not change var->info */
  }
  var->k = VEXP;
  var->info = NOJUMPS;
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
  if (op == '-') luaK_minus(ls, v);
  else luaK_0(ls, NOTOP, 0);
  v->info = NOJUMPS;
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
      return;  /* keep v1->info != NOJUMPS */
    case '+': luaK_add(ls, v2); break;
    case '-': luaK_sub(ls, v2); break;
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
  v1->info = NOJUMPS;
}

