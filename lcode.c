/*
** $Id: lcode.c,v 1.5 2000/03/03 20:30:47 roberto Exp roberto $
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
    case PUSHINT: SETARG_S(*previous, -GETARG_S(*previous)); return;
    case PUSHNUM: SET_OPCODE(*previous, PUSHNEGNUM); return;
    case PUSHNEGNUM: SET_OPCODE(*previous, PUSHNUM); return;
    default: luaK_primitivecode(ls, CREATE_0(MINUSOP));
  }
}


static void luaK_gettable (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case PUSHSTRING: SET_OPCODE(*previous, GETDOTTED); break;
    default: luaK_primitivecode(ls, CREATE_0(GETTABLE));
  }
}


static void luaK_add (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case PUSHINT: SET_OPCODE(*previous, ADDI); break;
    default: luaK_primitivecode(ls, CREATE_0(ADDOP));
  }
}


static void luaK_sub (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case PUSHINT:
      SET_OPCODE(*previous, ADDI);
      SETARG_S(*previous, -GETARG_S(*previous));
      break;
    default: luaK_primitivecode(ls, CREATE_0(SUBOP));
  }
}


static void luaK_conc (LexState *ls) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, -1);
  switch(GET_OPCODE(*previous)) {
    case CONCOP: SETARG_U(*previous, GETARG_U(*previous)+1); break;
    default: luaK_primitivecode(ls, CREATE_U(CONCOP, 2));
  }
}


void luaK_retcode (LexState *ls, int nlocals, int nexps) {
  Instruction *previous = previous_instruction(ls);
  if (nexps > 0 && GET_OPCODE(*previous) == CALL) {
    LUA_ASSERT(ls->L, GETARG_B(*previous) == MULT_RET, "call should be open");
    SET_OPCODE(*previous, TAILCALL);
    SETARG_B(*previous, nlocals);
  }
  else
    luaK_primitivecode(ls, CREATE_U(RETCODE, nlocals));
}


static void luaK_pushnil (LexState *ls, int n) {
  Instruction *previous = previous_instruction(ls);
  luaK_deltastack(ls, n);
  switch(GET_OPCODE(*previous)) {
    case PUSHNIL:
      SETARG_U(*previous, GETARG_U(*previous)+n);
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
      luaK_U(ls, PUSHLOCAL, var->u.index, 1);
      break;
    case VGLOBAL:
      luaK_U(ls, GETGLOBAL, var->u.index, 1);
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
      luaK_U(ls, SETLOCAL, var->u.index, -1);
      break;
    case VGLOBAL:
      luaK_U(ls, SETGLOBAL, var->u.index, -1);
      assertglobal(ls, var->u.index);  /* make sure that there is a global */
      break;
    case VINDEXED:
      luaK_0(ls, SETTABLEPOP, -3);
      break;
    default:
      LUA_INTERNALERROR(ls->L, "invalid var kind to store");
  }
}


static OpCode invertjump (OpCode op) {
  switch (op) {
    case IFNEQJMP: return IFEQJMP;
    case IFEQJMP: return IFNEQJMP;
    case IFLTJMP: return IFGEJMP;
    case IFLEJMP: return IFGTJMP;
    case IFGTJMP: return IFLEJMP;
    case IFGEJMP: return IFLTJMP;
    default:
      LUA_INTERNALERROR(NULL, "invalid jump instruction");
      return ENDCODE;  /* to avoid warnings */
  }
}


static void insert_last (FuncState *fs, int *list) {
  int temp = *list;
  *list = fs->pc-1;
  if (temp == 0)  /* chain list */
    SETARG_S(fs->f->code[*list], 0);
  else
    SETARG_S(fs->f->code[*list], temp-fs->pc);
}


static void luaK_patchlistaux (LexState *ls, int list, int target,
                               OpCode special, int special_target) {
  if (list != 0) {
    Instruction *code = ls->fs->f->code;
    for (;;) {
      Instruction *i = &code[list];
      OpCode op = GET_OPCODE(*i);
      int temp = GETARG_S(*i);
      if (op == special)
        SETARG_S(*i, special_target-(list+1));
      else {
        SETARG_S(*i, target-(list+1));
        if (op == ONTJMP)
          SET_OPCODE(*i, IFTJMP);
        else if (op == ONFJMP)
          SET_OPCODE(*i, IFFJMP);
      }
      if (temp == 0) return;
      list += temp+1;
    }
  }
}


void luaK_patchlist (LexState *ls, int list, int target) {
  luaK_patchlistaux(ls, list, target, ENDCODE, 0);
}


static int has_jumps (FuncState *fs, int list, OpCode ignore) {
  if (list == 0) return 0;
  else {
    Instruction *code = fs->f->code;
    for (;;) {
      int temp = GETARG_S(code[list]);
      if (GET_OPCODE(code[list]) != ignore) return 1;
      else if (temp == 0) return 0;
      list += temp+1;
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
      int temp = GETARG_S(fs->f->code[list]);
      if (temp == 0) {  /* end of list? */
        SETARG_S(fs->f->code[list], l2-(list+1));  /* end points to `l2' */
        return;
      }
      list += temp+1;
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
    OpCode jump = keepvalue ? ONFJMP : IFFJMP;
    luaK_S(ls, jump, 0, -1);
  }
  insert_last(fs, &v->u.l.f);
  luaK_patchlist(ls, v->u.l.t, luaK_getlabel(ls));
  v->u.l.t = 0;
}


void luaK_goiffalse (LexState *ls, expdesc *v, int keepvalue) {
  FuncState *fs = ls->fs;
  Instruction *previous;
  discharge1(ls, v);
  previous = &fs->f->code[fs->pc-1];
  if (!ISJUMP(GET_OPCODE(*previous))) {
    OpCode jump = keepvalue ? ONTJMP : IFTJMP;
    luaK_S(ls, jump, 0, -1);
  }
  insert_last(fs, &v->u.l.t);
  luaK_patchlist(ls, v->u.l.f, luaK_getlabel(ls));
  v->u.l.f = 0;
}


void luaK_tostack (LexState *ls, expdesc *v, int onlyone) {
  if (discharge(ls, v)) return;
  else {  /* is an expression */
    FuncState *fs = ls->fs;
    Instruction *previous = &fs->f->code[fs->pc-1];
    if (!ISJUMP(GET_OPCODE(*previous)) && v->u.l.f == 0 && v->u.l.t == 0) {
      /* it is an expression without jumps */
      if (onlyone && v->k == VEXP)
        luaK_setcallreturns(ls, 1);  /* call must return 1 value */
      return;
    }
    else {  /* expression has jumps... */
      int p_nil = 0;  /* position of an eventual PUSHNIL */
      int p_1 = 0;  /* position of an eventual PUSHINT */
      int final;  /* position after whole expression */
      if (ISJUMP(GET_OPCODE(*previous))) {
        insert_last(fs, &v->u.l.t);
        p_nil = luaK_0(ls, PUSHNILJMP, 0);
        p_1 = luaK_S(ls, PUSHINT, 1, 1);
      }
      else {  /* still may need a PUSHNIL or a PUSHINT */
        int need_nil = has_jumps(fs, v->u.l.f, ONFJMP);  /* needs a PUSHNIL? */
        int need_1 = has_jumps(fs, v->u.l.t, ONTJMP);  /* needs a PUSHINT? */
        if (need_nil && need_1) {
          luaK_S(ls, JMP, 2, 0);
          p_nil = luaK_0(ls, PUSHNILJMP, 0);
          p_1 = luaK_S(ls, PUSHINT, 1, 1);
        }
        else if (need_nil || need_1) {
          luaK_S(ls, JMP, 1, 0);
          if (need_nil)
            p_nil = luaK_0(ls, PUSHNIL, 1);
          else  /* need_1 */
            p_1 = luaK_S(ls, PUSHINT, 1, 1);
        }
      }
      final = luaK_getlabel(ls);
      luaK_patchlistaux(ls, v->u.l.f, p_nil, ONFJMP, final);
      luaK_patchlistaux(ls, v->u.l.t, p_1, ONTJMP, final);
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
      luaK_0(ls, NOTOP, 0);
    /* interchange true and false lists */
    { int temp = v->u.l.f; v->u.l.f = v->u.l.t; v->u.l.t = temp; }
  }
}


void luaK_infix (LexState *ls, int op, expdesc *v) {
  if (op == AND)
    luaK_goiftrue(ls, v, 1);
  else if (op == OR)
    luaK_goiffalse(ls, v, 1);
  else
    luaK_tostack(ls, v, 1);
} 


void luaK_posfix (LexState *ls, int op, expdesc *v1, expdesc *v2) {
  if (op == AND) {
    LUA_ASSERT(ls->L, v1->u.l.t == 0, "list must be closed");
    discharge1(ls, v2);
    v1->u.l.t = v2->u.l.t;
    concatlists(ls, &v1->u.l.f, v2->u.l.f);
  }
  else if (op == OR) {
    LUA_ASSERT(ls->L, v1->u.l.f == 0, "list must be closed");
    discharge1(ls, v2);
    v1->u.l.f = v2->u.l.f;
    concatlists(ls, &v1->u.l.t, v2->u.l.t);
  }
  else {
    luaK_tostack(ls, v2, 1);  /* `v2' must have a value */
    switch (op) {
      case '+': luaK_add(ls); break;
      case '-': luaK_sub(ls); break;
      case '*': luaK_0(ls, MULTOP, -1); break;
      case '/': luaK_0(ls, DIVOP, -1); break;
      case '^': luaK_0(ls, POWOP, -1); break;
      case CONC: luaK_conc(ls); break;
      case EQ: luaK_S(ls, IFEQJMP, 0, -2); break;
      case NE: luaK_S(ls, IFNEQJMP, 0, -2); break;
      case '>': luaK_S(ls, IFGTJMP, 0, -2); break;
      case '<': luaK_S(ls, IFLTJMP, 0, -2); break;
      case GE: luaK_S(ls, IFGEJMP, 0, -2); break;
      case LE: luaK_S(ls, IFLEJMP, 0, -2); break;
    }
  }
}

