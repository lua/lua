/*
** $Id: lcode.c,v 1.19 2000/04/04 20:48:44 roberto Exp roberto $
** Code generator for Lua
** See Copyright Notice in lua.h
*/


#include "stdlib.h"

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
** Returns the the previous instruction, for optimizations. 
** If there is a jump target between this and the current instruction,
** returns a dummy instruction to avoid wrong optimizations.
*/
static Instruction previous_instruction (FuncState *fs) {
  if (fs->pc > fs->lasttarget)  /* no jumps to current position? */
    return fs->f->code[fs->pc-1];  /* returns previous instruction */
  else
    return CREATE_0(OP_END);  /* no optimizations after an `END' */
}


int luaK_code (FuncState *fs, Instruction i, int delta) {
  luaK_deltastack(fs, delta);
  luaM_growvector(fs->L, fs->f->code, fs->pc, 1, Instruction, codeEM, MAX_INT);
  fs->f->code[fs->pc] = i;
  return fs->pc++;
}

int luaK_0(FuncState *fs, OpCode o, int d) {
  return luaK_code(fs, CREATE_0(o), d);
}

int luaK_U(FuncState *fs, OpCode o, int u, int d) {
  return luaK_code(fs, CREATE_U(o,u), d);
}

int luaK_S(FuncState *fs, OpCode o, int s, int d) {
  return luaK_code(fs, CREATE_S(o,s), d);
}

int luaK_AB(FuncState *fs, OpCode o, int a, int b, int d) {
  return luaK_code(fs, CREATE_AB(o,a,b), d);
}


static Instruction prepare (FuncState *fs, Instruction i, int delta) {
  Instruction previous = previous_instruction(fs);
  luaK_code(fs, i, delta);
  return previous;
}


static void setprevious (FuncState *fs, Instruction i) {
  fs->pc--;  /* remove last instruction */
  fs->f->code[fs->pc-1] = i;  /* change previous instruction */
}


static void luaK_minus (FuncState *fs) {
  /* PUSHINT s; MINUS    -> PUSHINT -s   (-k) */
  /* PUSHNUM u; MINUS    -> PUSHNEGNUM u   (-k) */
  Instruction previous = prepare(fs, CREATE_0(OP_MINUS), 0);
  switch(GET_OPCODE(previous)) {
    case OP_PUSHINT: SETARG_S(previous, -GETARG_S(previous)); break;
    case OP_PUSHNUM: SET_OPCODE(previous, OP_PUSHNEGNUM); break;
    default: return;
  }
  setprevious(fs, previous);
}


static void luaK_gettable (FuncState *fs) {
  /* PUSHSTRING u; GETTABLE -> GETDOTTED u   (t.x) */
  Instruction previous = prepare(fs, CREATE_0(OP_GETTABLE), -1);
  switch(GET_OPCODE(previous)) {
    case OP_PUSHSTRING: SET_OPCODE(previous, OP_GETDOTTED); break;
    default: return;
  }
  setprevious(fs, previous);
}


static void luaK_add (FuncState *fs) {
  /* PUSHINT s; ADD -> ADDI s   (a+k) */
  Instruction previous = prepare(fs, CREATE_0(OP_ADD), -1);
  switch(GET_OPCODE(previous)) {
    case OP_PUSHINT: SET_OPCODE(previous, OP_ADDI); break;
    default: return;
  }
  setprevious(fs, previous);
}


static void luaK_sub (FuncState *fs) {
  /* PUSHINT s; SUB -> ADDI -s   (a-k) */
  Instruction previous = prepare(fs, CREATE_0(OP_SUB), -1);
  switch(GET_OPCODE(previous)) {
    case OP_PUSHINT:
      SET_OPCODE(previous, OP_ADDI);
      SETARG_S(previous, -GETARG_S(previous));
      break;
    default: return;
  }
  setprevious(fs, previous);
}


static void luaK_conc (FuncState *fs) {
  /* CONC u; CONC 2 -> CONC u+1   (a..b..c) */
  Instruction previous = prepare(fs, CREATE_U(OP_CONC, 2), -1);
  switch(GET_OPCODE(previous)) {
    case OP_CONC: SETARG_U(previous, GETARG_U(previous)+1); break;
    default: return;
  }
  setprevious(fs, previous);
}


static void luaK_setlocal (FuncState *fs, int l) {
  /* PUSHLOCAL l; ADDI k, SETLOCAL l -> INCLOCAL k, l   ((local)a=a+k) */
  Instruction *code = fs->f->code;
  int pc = fs->pc;
  if (pc-1 > fs->lasttarget &&  /* no jumps in-between instructions? */
      code[pc-2] == CREATE_U(OP_PUSHLOCAL, l) &&
      GET_OPCODE(code[pc-1]) == OP_ADDI &&
      abs(GETARG_S(code[pc-1])) <= MAXARG_sA) {
    int inc = GETARG_S(code[pc-1]);
    fs->pc = pc-1;
    code[pc-2] = CREATE_sAB(OP_INCLOCAL, inc, l);
    luaK_deltastack(fs, -1);
  }
  else
    luaK_U(fs, OP_SETLOCAL, l, -1);
}


static void luaK_eq (FuncState *fs) {
  /* PUSHNIL 1; JMPEQ -> NOT  (a==nil) */
  Instruction previous = prepare(fs, CREATE_S(OP_JMPEQ, NO_JUMP), -2);
  if (previous == CREATE_U(OP_PUSHNIL, 1)) {
    setprevious(fs, CREATE_0(OP_NOT));
    luaK_deltastack(fs, 1);  /* undo delta from `prepare' */
  }
}


static void luaK_neq (FuncState *fs) {
  /* PUSHNIL 1; JMPNEQ -> JMPT   (a~=nil) */
  Instruction previous = prepare(fs, CREATE_S(OP_JMPNEQ, NO_JUMP), -2);
  if (previous == CREATE_U(OP_PUSHNIL, 1)) {
    setprevious(fs, CREATE_S(OP_JMPT, NO_JUMP));
  }
}


int luaK_jump (FuncState *fs) {
  return luaK_S(fs, OP_JMP, NO_JUMP, 0);
}


void luaK_retcode (FuncState *fs, int nlocals, int nexps) {
  Instruction previous = prepare(fs, CREATE_U(OP_RETURN, nlocals), 0);
  if (nexps > 0 && GET_OPCODE(previous) == OP_CALL) {
    LUA_ASSERT(fs->L, GETARG_B(previous) == MULT_RET, "call should be open");
    SET_OPCODE(previous, OP_TAILCALL);
    SETARG_B(previous, nlocals);
    setprevious(fs, previous);
  }
}


static void luaK_pushnil (FuncState *fs, int n) {
  Instruction previous = prepare(fs, CREATE_U(OP_PUSHNIL, n), n);
  switch(GET_OPCODE(previous)) {
    case OP_PUSHNIL: SETARG_U(previous, GETARG_U(previous)+n); break;
    default: return;
  }
  setprevious(fs, previous);
}


static void luaK_fixjump (FuncState *fs, int pc, int dest) {
  Instruction *jmp = &fs->f->code[pc];
  if (dest == NO_JUMP)
    SETARG_S(*jmp, NO_JUMP);  /* point to itself to represent end of list */
  else {  /* jump is relative to position following jump instruction */
    int offset = dest-(pc+1);
    LUA_ASSERT(L, offset != NO_JUMP, "cannot link to itself");
    if (abs(offset) > MAXARG_S)
      luaK_error(fs->ls, "control structure too long");
    SETARG_S(*jmp, offset);
  }
}


static int luaK_getjump (FuncState *fs, int pc) {
  int offset = GETARG_S(fs->f->code[pc]);
  if (offset == NO_JUMP)  /* point to itself represents end of list */
    return NO_JUMP;  /* end of list */
  else
    return (pc+1)+offset;  /* turn offset into absolute position */
}


/*
** returns current `pc' and marks it as a jump target (to avoid wrong
** optimizations with consecutive instructions not in the same basic block).
*/
int luaK_getlabel (FuncState *fs) {
  fs->lasttarget = fs->pc;
  return fs->pc;
}


void luaK_deltastack (FuncState *fs, int delta) {
  fs->stacklevel += delta;
  if (delta > 0 && fs->stacklevel > fs->f->maxstacksize) {
    if (fs->stacklevel > MAXSTACK)
      luaK_error(fs->ls, "function or expression too complex");
    fs->f->maxstacksize = fs->stacklevel;
  }
}


void luaK_kstr (LexState *ls, int c) {
  luaK_U(ls->fs, OP_PUSHSTRING, c, 1);
}


static int real_constant (FuncState *fs, Number r) {
  /* check whether `r' has appeared within the last LOOKBACKNUMS entries */
  Proto *f = fs->f;
  int c = f->nknum;
  int lim = c < LOOKBACKNUMS ? 0 : c-LOOKBACKNUMS;
  while (--c >= lim)
    if (f->knum[c] == r) return c;
  /* not found; create a new entry */
  luaM_growvector(fs->L, f->knum, f->nknum, 1, Number, constantEM, MAXARG_U);
  c = f->nknum++;
  f->knum[c] = r;
  return c;
}


void luaK_number (FuncState *fs, Number f) {
  if (f <= (Number)MAXARG_S && (int)f == f)
    luaK_S(fs, OP_PUSHINT, (int)f, 1);  /* f has a short integer value */
  else
    luaK_U(fs, OP_PUSHNUM, real_constant(fs, f), 1);
}


void luaK_adjuststack (FuncState *fs, int n) {
  if (n > 0)
    luaK_U(fs, OP_POP, n, -n);
  else if (n < 0)
    luaK_pushnil(fs, -n);
}


int luaK_lastisopen (FuncState *fs) {
  /* check whether last instruction is an open function call */
  Instruction i = previous_instruction(fs);
  if (GET_OPCODE(i) == OP_CALL && GETARG_B(i) == MULT_RET)
    return 1;
  else return 0;
}


void luaK_setcallreturns (FuncState *fs, int nresults) {
  if (luaK_lastisopen(fs)) {  /* expression is an open function call? */
    SETARG_B(fs->f->code[fs->pc-1], nresults);  /* set number of results */
    luaK_deltastack(fs, nresults);  /* push results */
  }
}


static void assertglobal (FuncState *fs, int index) {
  luaS_assertglobal(fs->L, fs->f->kstr[index]);
}


static int discharge (FuncState *fs, expdesc *var) {
  switch (var->k) {
    case VLOCAL:
      luaK_U(fs, OP_PUSHLOCAL, var->u.index, 1);
      break;
    case VGLOBAL:
      luaK_U(fs, OP_GETGLOBAL, var->u.index, 1);
      assertglobal(fs, var->u.index);  /* make sure that there is a global */
      break;
    case VINDEXED:
      luaK_gettable(fs);
      break;
    case VEXP:
      return 0;  /* nothing to do */
  }
  var->k = VEXP;
  var->u.l.t = var->u.l.f = NO_JUMP;
  return 1;
}


static void discharge1 (FuncState *fs, expdesc *var) {
  discharge(fs, var);
 /* if it has jumps it is already discharged */
  if (var->u.l.t == NO_JUMP && var->u.l.f  == NO_JUMP)
    luaK_setcallreturns(fs, 1);  /* call must return 1 value */
}
  

void luaK_storevar (LexState *ls, const expdesc *var) {
  FuncState *fs = ls->fs;
  switch (var->k) {
    case VLOCAL:
      luaK_setlocal(fs, var->u.index);
      break;
    case VGLOBAL:
      luaK_U(fs, OP_SETGLOBAL, var->u.index, -1);
      assertglobal(fs, var->u.index);  /* make sure that there is a global */
      break;
    case VINDEXED:
      luaK_0(fs, OP_SETTABLEPOP, -3);
      break;
    default:
      LUA_INTERNALERROR(ls->L, "invalid var kind to store");
  }
}


static OpCode invertjump (OpCode op) {
  switch (op) {
    case OP_JMPNEQ: return OP_JMPEQ;
    case OP_JMPEQ: return OP_JMPNEQ;
    case OP_JMPLT: return OP_JMPGE;
    case OP_JMPLE: return OP_JMPGT;
    case OP_JMPGT: return OP_JMPLE;
    case OP_JMPGE: return OP_JMPLT;
    case OP_JMPT: case OP_JMPONT:  return OP_JMPF;
    case OP_JMPF: case OP_JMPONF:  return OP_JMPT;
    default:
      LUA_INTERNALERROR(NULL, "invalid jump instruction");
      return OP_END;  /* to avoid warnings */
  }
}


static void luaK_condjump (FuncState *fs, OpCode jump) {
  Instruction previous = prepare(fs, CREATE_S(jump, NO_JUMP), -1);
  switch (GET_OPCODE(previous)) {
    case OP_NOT: previous = CREATE_S(invertjump(jump), NO_JUMP); break;
    default: return;
  }
  setprevious(fs, previous);
}


static void luaK_patchlistaux (FuncState *fs, int list, int target,
                               OpCode special, int special_target) {
  Instruction *code = fs->f->code;
  while (list != NO_JUMP) {
    int next = luaK_getjump(fs, list);
    Instruction *i = &code[list];
    OpCode op = GET_OPCODE(*i);
    if (op == special)  /* this `op' already has a value */
      luaK_fixjump(fs, list, special_target);
    else {
      luaK_fixjump(fs, list, target);  /* do the patch */
      if (op == OP_JMPONT)  /* remove eventual values */
        SET_OPCODE(*i, OP_JMPT);
      else if (op == OP_JMPONF)
        SET_OPCODE(*i, OP_JMPF);
    }
    list = next;
  }
}


void luaK_patchlist (FuncState *fs, int list, int target) {
  luaK_patchlistaux(fs, list, target, OP_END, 0);
}


static int need_value (FuncState *fs, int list, OpCode hasvalue) {
  /* check whether list has a jump without a value */
  for (; list != NO_JUMP; list = luaK_getjump(fs, list))
    if (GET_OPCODE(fs->f->code[list]) != hasvalue) return 1;
  return 0;  /* not found */
}


void luaK_concat (FuncState *fs, int *l1, int l2) {
  if (*l1 == NO_JUMP)
    *l1 = l2;
  else {
    int list = *l1;
    for (;;) {  /* traverse `l1' */
      int next = luaK_getjump(fs, list);
      if (next == NO_JUMP) {  /* end of list? */
        luaK_fixjump(fs, list, l2);
        return;
      }
      list = next;
    }
  }
}


static void luaK_testgo (FuncState *fs, expdesc *v, int invert, OpCode jump) {
  Instruction *previous;
  int *golist = &v->u.l.f;
  int *exitlist = &v->u.l.t;
  if (invert) {  /* interchange `golist' and `exitlist' */
    int *temp = golist; golist = exitlist; exitlist = temp;
  }
  discharge1(fs, v);
  previous = &fs->f->code[fs->pc-1];
  LUA_ASSERT(L, GET_OPCODE(*previous) != OP_SETLINE, "bad place to set line");
  if (ISJUMP(GET_OPCODE(*previous))) {
    if (invert)
      SET_OPCODE(*previous, invertjump(GET_OPCODE(*previous)));
  }
  else
    luaK_condjump(fs, jump);
  luaK_concat(fs, exitlist, fs->pc-1);  /* insert last jump in `exitlist' */
  luaK_patchlist(fs, *golist, luaK_getlabel(fs));
  *golist = NO_JUMP;
}


void luaK_goiftrue (FuncState *fs, expdesc *v, int keepvalue) {
  luaK_testgo(fs, v, 1, keepvalue ? OP_JMPONF : OP_JMPF);
}


void luaK_goiffalse (FuncState *fs, expdesc *v, int keepvalue) {
  luaK_testgo(fs, v, 0, keepvalue ? OP_JMPONT : OP_JMPT);
}


void luaK_tostack (LexState *ls, expdesc *v, int onlyone) {
  FuncState *fs = ls->fs;
  if (!discharge(fs, v)) {  /* `v' is an expression? */
    OpCode previous = GET_OPCODE(fs->f->code[fs->pc-1]);
    LUA_ASSERT(L, previous != OP_SETLINE, "bad place to set line");
    if (!ISJUMP(previous) && v->u.l.f == NO_JUMP && v->u.l.t == NO_JUMP) {
      /* it is an expression without jumps */
      if (onlyone)
        luaK_setcallreturns(fs, 1);  /* call must return 1 value */
    }
    else {  /* expression has jumps... */
      int p_nil = 0;  /* position of an eventual PUSHNIL */
      int p_1 = 0;  /* position of an eventual PUSHINT */
      int final;  /* position after whole expression */
      if (ISJUMP(previous)) {
        luaK_concat(fs, &v->u.l.t, fs->pc-1);  /* put `previous' in true list */
        p_nil = luaK_0(fs, OP_PUSHNILJMP, 0);
        p_1 = luaK_S(fs, OP_PUSHINT, 1, 1);
      }
      else {  /* still may need a PUSHNIL or a PUSHINT */
        int need_nil = need_value(fs, v->u.l.f, OP_JMPONF);
        int need_1 = need_value(fs, v->u.l.t, OP_JMPONT);
        if (need_nil && need_1) {
          luaK_S(fs, OP_JMP, 2, 0);  /* skip both pushes */
          p_nil = luaK_0(fs, OP_PUSHNILJMP, 0);
          p_1 = luaK_S(fs, OP_PUSHINT, 1, 0);
        }
        else if (need_nil || need_1) {
          luaK_S(fs, OP_JMP, 1, 0);  /* skip one push */
          if (need_nil)
            p_nil = luaK_U(fs, OP_PUSHNIL, 1, 0);
          else  /* need_1 */
            p_1 = luaK_S(fs, OP_PUSHINT, 1, 0);
        }
      }
      final = luaK_getlabel(fs);
      luaK_patchlistaux(fs, v->u.l.f, p_nil, OP_JMPONF, final);
      luaK_patchlistaux(fs, v->u.l.t, p_1, OP_JMPONT, final);
      v->u.l.f = v->u.l.t = NO_JUMP;
    }
  }
}


void luaK_prefix (LexState *ls, int op, expdesc *v) {
  FuncState *fs = ls->fs;
  if (op == '-') {
    luaK_tostack(ls, v, 1);
    luaK_minus(fs);
  }
  else {  /* op == NOT */
    Instruction *previous;
    discharge1(fs, v);
    previous = &fs->f->code[fs->pc-1];
    if (ISJUMP(GET_OPCODE(*previous)))
      SET_OPCODE(*previous, invertjump(GET_OPCODE(*previous)));
    else
      luaK_0(fs, OP_NOT, 0);
    /* interchange true and false lists */
    { int temp = v->u.l.f; v->u.l.f = v->u.l.t; v->u.l.t = temp; }
  }
}


void luaK_infix (LexState *ls, int op, expdesc *v) {
  FuncState *fs = ls->fs;
  if (op == TK_AND)
    luaK_goiftrue(fs, v, 1);
  else if (op == TK_OR)
    luaK_goiffalse(fs, v, 1);
  else
    luaK_tostack(ls, v, 1);  /* all other binary operators need a value */
} 


void luaK_posfix (LexState *ls, int op, expdesc *v1, expdesc *v2) {
  FuncState *fs = ls->fs;
  if (op == TK_AND) {
    LUA_ASSERT(ls->L, v1->u.l.t == NO_JUMP, "list must be closed");
    discharge1(fs, v2);
    v1->u.l.t = v2->u.l.t;
    luaK_concat(fs, &v1->u.l.f, v2->u.l.f);
  }
  else if (op == TK_OR) {
    LUA_ASSERT(ls->L, v1->u.l.f == NO_JUMP, "list must be closed");
    discharge1(fs, v2);
    v1->u.l.f = v2->u.l.f;
    luaK_concat(fs, &v1->u.l.t, v2->u.l.t);
  }
  else {
    luaK_tostack(ls, v2, 1);  /* `v2' must be a value */
    switch (op) {
      case '+': luaK_add(fs); break;
      case '-': luaK_sub(fs); break;
      case '*': luaK_0(fs, OP_MULT, -1); break;
      case '/': luaK_0(fs, OP_DIV, -1); break;
      case '^': luaK_0(fs, OP_POW, -1); break;
      case TK_CONC: luaK_conc(fs); break;
      case TK_EQ: luaK_eq(fs); break;
      case TK_NE: luaK_neq(fs); break;
      case '>': luaK_S(fs, OP_JMPGT, NO_JUMP, -2); break;
      case '<': luaK_S(fs, OP_JMPLT, NO_JUMP, -2); break;
      case TK_GE: luaK_S(fs, OP_JMPGE, NO_JUMP, -2); break;
      case TK_LE: luaK_S(fs, OP_JMPLE, NO_JUMP, -2); break;
    }
  }
}

