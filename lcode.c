/*
** $Id: lcode.c,v 1.22 2000/04/07 13:13:11 roberto Exp roberto $
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


int luaK_jump (FuncState *fs) {
  int j = luaK_code1(fs, OP_JMP, NO_JUMP);
  if (j == fs->lasttarget) {  /* possible jumps to this jump? */
    luaK_concat(fs, &j, fs->jlt);  /* keep them on hold */
    fs->jlt = NO_JUMP;
  }
  return j;
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
** discharge list of jumps to last target.
** returns current `pc' and marks it as a jump target (to avoid wrong
** optimizations with consecutive instructions not in the same basic block).
*/
int luaK_getlabel (FuncState *fs) {
  if (fs->pc != fs->lasttarget) {
    int lasttarget = fs->lasttarget;
    fs->lasttarget = fs->pc;
    luaK_patchlist(fs, fs->jlt, lasttarget);  /* discharge old list `jlt' */
    fs->jlt = NO_JUMP;  /* nobody jumps to this new label (till now) */
  }
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
  luaK_code1(ls->fs, OP_PUSHSTRING, c);
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
    luaK_code1(fs, OP_PUSHINT, (int)f);  /* f has a short integer value */
  else
    luaK_code1(fs, OP_PUSHNUM, real_constant(fs, f));
}


void luaK_adjuststack (FuncState *fs, int n) {
  if (n > 0)
    luaK_code1(fs, OP_POP, n);
  else if (n < 0)
    luaK_code1(fs, OP_PUSHNIL, -n);
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
      luaK_code1(fs, OP_GETLOCAL, var->u.index);
      break;
    case VGLOBAL:
      luaK_code1(fs, OP_GETGLOBAL, var->u.index);
      assertglobal(fs, var->u.index);  /* make sure that there is a global */
      break;
    case VINDEXED:
      luaK_code0(fs, OP_GETTABLE);
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
      luaK_code1(fs, OP_SETLOCAL, var->u.index);
      break;
    case VGLOBAL:
      luaK_code1(fs, OP_SETGLOBAL, var->u.index);
      assertglobal(fs, var->u.index);  /* make sure that there is a global */
      break;
    case VINDEXED:  /* table is at top-3; pop 3 elements after operation */
      luaK_code2(fs, OP_SETTABLE, 3, 3);
      break;
    default:
      LUA_INTERNALERROR(ls->L, "invalid var kind to store");
  }
}


static OpCode invertjump (OpCode op) {
  switch (op) {
    case OP_JMPNE: return OP_JMPEQ;
    case OP_JMPEQ: return OP_JMPNE;
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
  if (target == fs->lasttarget)  /* same target that list `jlt'? */
    luaK_concat(fs, &fs->jlt, list);  /* delay fixing */
  else
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
    luaK_code0(fs, jump);
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
        p_nil = luaK_code0(fs, OP_PUSHNILJMP);
        p_1 = luaK_code1(fs, OP_PUSHINT, 1);
      }
      else {  /* still may need a PUSHNIL or a PUSHINT */
        int need_nil = need_value(fs, v->u.l.f, OP_JMPONF);
        int need_1 = need_value(fs, v->u.l.t, OP_JMPONT);
        if (need_nil && need_1) {
          luaK_code1(fs, OP_JMP, 2);  /* skip both pushes */
          p_nil = luaK_code0(fs, OP_PUSHNILJMP);
          p_1 = luaK_code1(fs, OP_PUSHINT, 1);
          luaK_deltastack(fs, -1);  /* previous PUSHINT may be skipped */
        }
        else if (need_nil || need_1) {
          luaK_code1(fs, OP_JMP, 1);  /* skip one push */
          if (need_nil)
            p_nil = luaK_code1(fs, OP_PUSHNIL, 1);
          else  /* need_1 */
            p_1 = luaK_code1(fs, OP_PUSHINT, 1);
          luaK_deltastack(fs, -1);  /* previous PUSHs may be skipped */
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
    luaK_code0(fs, OP_MINUS);
  }
  else {  /* op == NOT */
    Instruction *previous;
    discharge1(fs, v);
    previous = &fs->f->code[fs->pc-1];
    if (ISJUMP(GET_OPCODE(*previous)))
      SET_OPCODE(*previous, invertjump(GET_OPCODE(*previous)));
    else
      luaK_code0(fs, OP_NOT);
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
      case '+': luaK_code0(fs, OP_ADD); break;
      case '-': luaK_code0(fs, OP_SUB); break;
      case '*': luaK_code0(fs, OP_MULT); break;
      case '/': luaK_code0(fs, OP_DIV); break;
      case '^': luaK_code0(fs, OP_POW); break;
      case TK_CONCAT: luaK_code1(fs, OP_CONCAT, 2); break;
      case TK_EQ: luaK_code0(fs, OP_JMPEQ); break;
      case TK_NE: luaK_code0(fs, OP_JMPNE); break;
      case '>': luaK_code0(fs, OP_JMPGT); break;
      case '<': luaK_code0(fs, OP_JMPLT); break;
      case TK_GE: luaK_code0(fs, OP_JMPGE); break;
      case TK_LE: luaK_code0(fs, OP_JMPLE); break;
    }
  }
}


int luaK_code0 (FuncState *fs, OpCode o) {
  return luaK_code2(fs, o, 0, 0);
}


int luaK_code1 (FuncState *fs, OpCode o, int arg1) {
  return luaK_code2(fs, o, arg1, 0);
}



int luaK_code2 (FuncState *fs, OpCode o, int arg1, int arg2) {
  Instruction i = previous_instruction(fs);
  int delta = 0;
  enum {iO, iU, iS, iAB, iP} mode;  /* instruction format (or iP to optimize) */
  mode = iP;
  switch (o) {

    case OP_JMP: delta = 0; mode = iS; break;
    case OP_CLOSURE: delta = -arg2+1; mode = iAB; break;
    case OP_SETLINE: mode = iU; break;
    case OP_CALL: mode = iAB; break;
    case OP_PUSHINT: delta = 1; mode = iS; break;
    case OP_SETGLOBAL: delta = -1; mode = iU; break;
    case OP_SETTABLE: delta = -arg2; mode = iAB; break;
    case OP_SETLIST: delta = -(arg2+1); mode = iAB; break;
    case OP_SETMAP: delta = -2*(arg1+1); mode = iU; break;

    case OP_END: case OP_PUSHNILJMP: case OP_NOT:
      mode = iO; break;

    case OP_PUSHSTRING: case OP_PUSHNUM:
    case OP_PUSHNEGNUM: case OP_PUSHUPVALUE:
    case OP_GETGLOBAL: case OP_PUSHSELF: case OP_CREATETABLE:
      delta = 1; mode = iU; break;

    case OP_JMPLT: case OP_JMPLE: case OP_JMPGT: case OP_JMPGE:
      delta = -2; arg1 = NO_JUMP; mode = iS; break;

    case OP_MULT: case OP_DIV: case OP_POW:
      delta = -1; mode = iO; break;

    case OP_RETURN:
      if (GET_OPCODE(i) == OP_CALL && GETARG_B(i) == MULT_RET) {
        SET_OPCODE(i, OP_TAILCALL);
        SETARG_B(i, arg1);
      }
      else mode = iU;
      break;

    case OP_PUSHNIL:
      delta = arg1;
      switch(GET_OPCODE(i)) {
        case OP_PUSHNIL: SETARG_U(i, GETARG_U(i)+arg1); break;
        default: mode = iU; break;
      }
      break;

    case OP_POP:
      delta = -arg1;
      switch(GET_OPCODE(i)) {
        case OP_SETTABLE: SETARG_B(i, GETARG_B(i)+arg1); break;
        case OP_SETLOCAL: SETARG_B(i, GETARG_B(i)+arg1); break;
        default: mode = iU; break;
      }
      break;

    case OP_GETLOCAL:
      delta = 1;
      if (i == CREATE_AB(OP_SETLOCAL, arg1, 1))
        SETARG_B(i, 0);
      else mode = iU;
      break;

    case OP_GETTABLE:
      delta = -1;
      switch(GET_OPCODE(i)) {
        case OP_PUSHSTRING: SET_OPCODE(i, OP_GETDOTTED); break;  /* `t.x' */
        case OP_GETLOCAL: SET_OPCODE(i, OP_GETINDEXED); break;  /* `t[i]' */
        default: mode = iO; break;
      }
      break;

    case OP_SETLOCAL: {
      int pc = fs->pc;
      Instruction *code = fs->f->code;
      delta = -1;
      if (pc-1 > fs->lasttarget &&  /* no jumps in-between instructions? */
          code[pc-2] == CREATE_U(OP_GETLOCAL, arg1) &&
          GET_OPCODE(i) == OP_ADDI && abs(GETARG_S(i)) <= MAXARG_sA) {
        /* `local=local+k' */
        fs->pc = pc-1;
        code[pc-2] = CREATE_sAB(OP_INCLOCAL, GETARG_S(i), arg1);
        luaK_deltastack(fs, delta);
        return pc-1;
      }
      else {
        arg2 = 1;  /* `setlocal' default pops one value */
        mode = iAB;
      }
      break;
    }

    case OP_ADD:
      delta = -1;
      switch(GET_OPCODE(i)) {
        case OP_PUSHINT: SET_OPCODE(i, OP_ADDI); break;  /* `a+k' */
        default: mode = iO; break;
      }
      break;

    case OP_SUB:
      delta = -1;
      switch(GET_OPCODE(i)) {
        case OP_PUSHINT: i = CREATE_S(OP_ADDI, -GETARG_S(i)); break; /* `a-k' */
        default: mode = iO; break;
      }
      break;

    case OP_CONCAT:
      delta = -arg1+1;
      switch(GET_OPCODE(i)) {
        case OP_CONCAT: SETARG_U(i, GETARG_U(i)+1); break;  /* `a..b..c' */
        default: mode = iU; break;
      }
      break;

    case OP_MINUS:
      switch(GET_OPCODE(i)) {
        case OP_PUSHINT: SETARG_S(i, -GETARG_S(i)); break;  /* `-k' */
        case OP_PUSHNUM: SET_OPCODE(i, OP_PUSHNEGNUM); break;  /* `-k' */
        default: mode = iO; break;
      }
      break;

    case OP_JMPNE:
      delta = -2;
      if (i == CREATE_U(OP_PUSHNIL, 1))  /* `a~=nil' */
        i = CREATE_S(OP_JMPT, NO_JUMP);
      else {
        arg1 = NO_JUMP;
        mode = iS;
      }
      break;

    case OP_JMPEQ:
      delta = -2;
      if (i == CREATE_U(OP_PUSHNIL, 1)) {  /* `a==nil' */
        i = CREATE_0(OP_NOT);
        delta = -1;  /* just undo effect of previous PUSHNIL */
      }
      else {
        arg1 = NO_JUMP;
        mode = iS;
      }
      break;

    case OP_JMPT: case OP_JMPF: case OP_JMPONT: case OP_JMPONF:
      delta = -1;
      arg1 = NO_JUMP;
      switch (GET_OPCODE(i)) {
        case OP_NOT: i = CREATE_S(invertjump(o), NO_JUMP); break;
        default: mode = iS; break;
      }
      break;

    case OP_GETDOTTED: case OP_GETINDEXED:
    case OP_TAILCALL: case OP_INCLOCAL:
    case OP_ADDI:
      LUA_INTERNALERROR(L, "instruction used only for optimizations");
      return 0;  /* to avoid warnings */

  }
  luaK_deltastack(fs, delta);
  switch (mode) {  /* handle instruction formats */
    case iO: i = CREATE_0(o); break;
    case iU: i = CREATE_U(o, arg1); break;
    case iS: i = CREATE_S(o, arg1); break;
    case iAB: i = CREATE_AB(o, arg1, arg2); break;
    case iP: {  /* optimize: put instruction in place of last one */
      fs->f->code[fs->pc-1] = i;  /* change previous instruction */
      return fs->pc-1;
    }
  }
  /* actually create the new instruction */
  luaM_growvector(fs->L, fs->f->code, fs->pc, 1, Instruction, codeEM, MAX_INT);
  fs->f->code[fs->pc] = i;
  return fs->pc++;
}

