/*
** $Id: lcode.c,v 1.32 2000/05/24 13:54:49 roberto Exp roberto $
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


void luaK_error (LexState *ls, const char *msg) {
  luaX_error(ls, msg, ls->t.token);
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
  luaM_growvector(fs->L, f->knum, f->nknum, 1, Number,
                  "constant table overflow", MAXARG_U);
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


static int discharge (FuncState *fs, expdesc *var) {
  switch (var->k) {
    case VLOCAL:
      luaK_code1(fs, OP_GETLOCAL, var->u.index);
      break;
    case VGLOBAL:
      luaK_code1(fs, OP_GETGLOBAL, var->u.index);
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
    luaK_code1(fs, jump, NO_JUMP);
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


static int code_label (FuncState *fs, OpCode op, int arg) {
  int j = luaK_getlabel(fs);
  luaK_code1(fs, op, arg);
  return j;
}


void luaK_tostack (LexState *ls, expdesc *v, int onlyone) {
  FuncState *fs = ls->fs;
  if (!discharge(fs, v)) {  /* `v' is an expression? */
    OpCode previous = GET_OPCODE(fs->f->code[fs->pc-1]);
    LUA_ASSERT(L, previous != OP_SETLINE, "bad place to set line");
    if (!ISJUMP(previous) && v->u.l.f == NO_JUMP && v->u.l.t == NO_JUMP) {
      /* expression has no jumps */
      if (onlyone)
        luaK_setcallreturns(fs, 1);  /* call must return 1 value */
    }
    else {  /* expression has jumps */
      int final;  /* position after whole expression */
      int j = NO_JUMP;  /*  eventual  jump over values */
      int p_nil = NO_JUMP;  /* position of an eventual PUSHNIL */
      int p_1 = NO_JUMP;  /* position of an eventual PUSHINT */
      if (ISJUMP(previous) || need_value(fs, v->u.l.f, OP_JMPONF) ||
                              need_value(fs, v->u.l.t, OP_JMPONT)) {
        /* expression needs values */
        if (ISJUMP(previous))
          luaK_concat(fs, &v->u.l.t, fs->pc-1);  /* put `previous' in t. list */
        else {
          j = code_label(fs, OP_JMP, NO_JUMP);  /* to jump over both pushes */
          luaK_deltastack(fs, -1);  /* next PUSHes may be skipped */
        }
        p_nil = code_label(fs, OP_PUSHNILJMP, 0);
        luaK_deltastack(fs, -1);  /* next PUSH is skipped */
        p_1 = code_label(fs, OP_PUSHINT, 1);
        luaK_patchlist(fs, j, luaK_getlabel(fs));
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
      case TK_EQ: luaK_code1(fs, OP_JMPEQ, NO_JUMP); break;
      case TK_NE: luaK_code1(fs, OP_JMPNE, NO_JUMP); break;
      case '>': luaK_code1(fs, OP_JMPGT, NO_JUMP); break;
      case '<': luaK_code1(fs, OP_JMPLT, NO_JUMP); break;
      case TK_GE: luaK_code1(fs, OP_JMPGE, NO_JUMP); break;
      case TK_LE: luaK_code1(fs, OP_JMPLE, NO_JUMP); break;
    }
  }
}


int luaK_code0 (FuncState *fs, OpCode o) {
  return luaK_code2(fs, o, 0, 0);
}


int luaK_code1 (FuncState *fs, OpCode o, int arg1) {
  return luaK_code2(fs, o, arg1, 0);
}


#define VD	100	/* flag for variable delta */


int luaK_code2 (FuncState *fs, OpCode o, int arg1, int arg2) {
  Instruction i = previous_instruction(fs);
  int delta = luaK_opproperties[o].delta;
  int optm = 0;  /* 1 when there is an optimization */
  switch (o) {

    case OP_CLOSURE:
      delta = -arg2+1;
      break;

    case OP_SETTABLE:
    case OP_SETLIST:
      delta = -arg2;
      break;

    case OP_SETMAP:
      delta = -2*(arg1+1);
      break;

    case OP_RETURN:
      if (GET_OPCODE(i) == OP_CALL && GETARG_B(i) == MULT_RET) {
        SET_OPCODE(i, OP_TAILCALL);
        SETARG_B(i, arg1);
        optm = 1;
      }
      break;

    case OP_PUSHNIL:
      delta = arg1;
      switch(GET_OPCODE(i)) {
        case OP_PUSHNIL: SETARG_U(i, GETARG_U(i)+arg1); optm = 1; break;
        default: break;
      }
      break;

    case OP_POP:
      delta = -arg1;
      switch(GET_OPCODE(i)) {
        case OP_SETTABLE: SETARG_B(i, GETARG_B(i)+arg1); optm = 1; break;
        default: break;
      }
      break;

    case OP_GETTABLE:
      switch(GET_OPCODE(i)) {
        case OP_PUSHSTRING:  /* `t.x' */
          SET_OPCODE(i, OP_GETDOTTED);
          optm = 1;
          break;
        case OP_GETLOCAL:  /* `t[i]' */
          SET_OPCODE(i, OP_GETINDEXED);
          optm = 1;
          break;
        default: break;
      }
      break;

    case OP_ADD:
      switch(GET_OPCODE(i)) {
        case OP_PUSHINT: SET_OPCODE(i, OP_ADDI); optm = 1; break;  /* `a+k' */
        default: break;
      }
      break;

    case OP_SUB:
      switch(GET_OPCODE(i)) {
        case OP_PUSHINT:  /* `a-k' */
          i = CREATE_S(OP_ADDI, -GETARG_S(i));
          optm = 1;
          break;
        default: break;
      }
      break;

    case OP_CONCAT:
      delta = -arg1+1;
      switch(GET_OPCODE(i)) {
        case OP_CONCAT:  /* `a..b..c' */
          SETARG_U(i, GETARG_U(i)+1);
          optm = 1;
          break;
        default: break;
      }
      break;

    case OP_MINUS:
      switch(GET_OPCODE(i)) {
        case OP_PUSHINT:  /* `-k' */
          SETARG_S(i, -GETARG_S(i));
          optm = 1;
          break;
        case OP_PUSHNUM:  /* `-k' */
          SET_OPCODE(i, OP_PUSHNEGNUM);
          optm = 1;
          break;
        default: break;
      }
      break;

    case OP_JMPNE:
      if (i == CREATE_U(OP_PUSHNIL, 1)) {  /* `a~=nil' */
        i = CREATE_S(OP_JMPT, NO_JUMP);
        optm = 1;
      }
      break;

    case OP_JMPEQ:
      if (i == CREATE_U(OP_PUSHNIL, 1)) {  /* `a==nil' */
        i = CREATE_0(OP_NOT);
        delta = -1;  /* just undo effect of previous PUSHNIL */
        optm = 1;
      }
      break;

    case OP_JMPT:
    case OP_JMPF:
    case OP_JMPONT:
    case OP_JMPONF:
      switch (GET_OPCODE(i)) {
        case OP_NOT: i = CREATE_S(invertjump(o), NO_JUMP); optm = 1; break;
        default: break;
      }
      break;

    case OP_GETDOTTED:
    case OP_GETINDEXED:
    case OP_TAILCALL:
    case OP_ADDI:
      LUA_INTERNALERROR(L, "instruction used only for optimizations");
      break;

    default:
      LUA_ASSERT(L, delta != VD, "invalid delta");
      break;

  }
  luaK_deltastack(fs, delta);
  if (optm) {  /* optimize: put instruction in place of last one */
      fs->f->code[fs->pc-1] = i;  /* change previous instruction */
      return fs->pc-1;  /* do not generate new instruction */
  }
  switch ((enum Mode)luaK_opproperties[o].mode) {
    case iO: i = CREATE_0(o); break;
    case iU: i = CREATE_U(o, arg1); break;
    case iS: i = CREATE_S(o, arg1); break;
    case iAB: i = CREATE_AB(o, arg1, arg2); break;
  }
  /* actually create the new instruction */
  luaM_growvector(fs->L, fs->f->code, fs->pc, 1, Instruction,
                  "code size overflow", MAX_INT);
  fs->f->code[fs->pc] = i;
  return fs->pc++;
}


const struct OpProperties luaK_opproperties[OP_SETLINE+1] = {
  {iO, 0},	/* OP_END */
  {iU, 0},	/* OP_RETURN */
  {iAB, 0},	/* OP_CALL */
  {iAB, 0},	/* OP_TAILCALL */
  {iU, VD},	/* OP_PUSHNIL */
  {iU, VD},	/* OP_POP */
  {iS, 1},	/* OP_PUSHINT */
  {iU, 1},	/* OP_PUSHSTRING */
  {iU, 1},	/* OP_PUSHNUM */
  {iU, 1},	/* OP_PUSHNEGNUM */
  {iU, 1},	/* OP_PUSHUPVALUE */
  {iU, 1},	/* OP_GETLOCAL */
  {iU, 1},	/* OP_GETGLOBAL */
  {iO, -1},	/* OP_GETTABLE */
  {iU, 0},	/* OP_GETDOTTED */
  {iU, 0},	/* OP_GETINDEXED */
  {iU, 1},	/* OP_PUSHSELF */
  {iU, 1},	/* OP_CREATETABLE */
  {iU, -1},	/* OP_SETLOCAL */
  {iU, -1},	/* OP_SETGLOBAL */
  {iAB, VD},	/* OP_SETTABLE */
  {iAB, VD},	/* OP_SETLIST */
  {iU, VD},	/* OP_SETMAP */
  {iO, -1},	/* OP_ADD */
  {iS, 0},	/* OP_ADDI */
  {iO, -1},	/* OP_SUB */
  {iO, -1},	/* OP_MULT */
  {iO, -1},	/* OP_DIV */
  {iO, -1},	/* OP_POW */
  {iU, VD},	/* OP_CONCAT */
  {iO, 0},	/* OP_MINUS */
  {iO, 0},	/* OP_NOT */
  {iS, -2},	/* OP_JMPNE */
  {iS, -2},	/* OP_JMPEQ */
  {iS, -2},	/* OP_JMPLT */
  {iS, -2},	/* OP_JMPLE */
  {iS, -2},	/* OP_JMPGT */
  {iS, -2},	/* OP_JMPGE */
  {iS, -1},	/* OP_JMPT */
  {iS, -1},	/* OP_JMPF */
  {iS, -1},	/* OP_JMPONT */
  {iS, -1},	/* OP_JMPONF */
  {iS, 0},	/* OP_JMP */
  {iO, 1},	/* OP_PUSHNILJMP */
  {iS, 0},	/* OP_FORPREP */
  {iS, -3},	/* OP_FORLOOP */
  {iS, 3},	/* OP_LFORPREP */
  {iS, -4},	/* OP_LFORLOOP */
  {iAB, VD},	/* OP_CLOSURE */
  {iU, 0}	/* OP_SETLINE */
};
