/*
** $Id: lcode.c,v 1.51 2000/09/29 12:42:13 roberto Exp $
** Code generator for Lua
** See Copyright Notice in lua.h
*/


#include "stdlib.h"

#include "lua.h"

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
** returns current `pc' and marks it as a jump target (to avoid wrong
** optimizations with consecutive instructions not in the same basic block).
** discharge list of jumps to last target.
*/
int luaK_getlabel (FuncState *fs) {
  if (fs->pc != fs->lasttarget) {
    int lasttarget = fs->lasttarget;
    fs->lasttarget = fs->pc;
    luaK_patchlist(fs, fs->jlt, lasttarget);  /* discharge old list `jlt' */
    fs->jlt = NO_JUMP;  /* nobody jumps to this new label (yet) */
  }
  return fs->pc;
}


void luaK_deltastack (FuncState *fs, int delta) {
  fs->stacklevel += delta;
  if (fs->stacklevel > fs->f->maxstacksize) {
    if (fs->stacklevel > MAXSTACK)
      luaK_error(fs->ls, "function or expression too complex");
    fs->f->maxstacksize = fs->stacklevel;
  }
}


void luaK_kstr (LexState *ls, int c) {
  luaK_code1(ls->fs, OP_PUSHSTRING, c);
}


static int number_constant (FuncState *fs, Number r) {
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
  if (f <= (Number)MAXARG_S && (Number)(int)f == f)
    luaK_code1(fs, OP_PUSHINT, (int)f);  /* f has a short integer value */
  else
    luaK_code1(fs, OP_PUSHNUM, number_constant(fs, f));
}


void luaK_adjuststack (FuncState *fs, int n) {
  if (n > 0)
    luaK_code1(fs, OP_POP, n);
  else
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
 /* if it has jumps then it is already discharged */
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
      LUA_INTERNALERROR("invalid var kind to store");
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
      LUA_INTERNALERROR("invalid jump instruction");
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
  int prevpos;  /* position of last instruction */
  Instruction *previous;
  int *golist, *exitlist;
  if (!invert) {
    golist = &v->u.l.f;    /* go if false */
    exitlist = &v->u.l.t;  /* exit if true */
  }
  else {
    golist = &v->u.l.t;    /* go if true */
    exitlist = &v->u.l.f;  /* exit if false */
  }
  discharge1(fs, v);
  prevpos = fs->pc-1;
  previous = &fs->f->code[prevpos];
  LUA_ASSERT(*previous==previous_instruction(fs), "no jump allowed here");
  if (!ISJUMP(GET_OPCODE(*previous)))
    prevpos = luaK_code1(fs, jump, NO_JUMP);
  else {  /* last instruction is already a jump */
    if (invert)
      SET_OPCODE(*previous, invertjump(GET_OPCODE(*previous)));
  }
  luaK_concat(fs, exitlist, prevpos);  /* insert last jump in `exitlist' */
  luaK_patchlist(fs, *golist, luaK_getlabel(fs));
  *golist = NO_JUMP;
}


void luaK_goiftrue (FuncState *fs, expdesc *v, int keepvalue) {
  luaK_testgo(fs, v, 1, keepvalue ? OP_JMPONF : OP_JMPF);
}


static void luaK_goiffalse (FuncState *fs, expdesc *v, int keepvalue) {
  luaK_testgo(fs, v, 0, keepvalue ? OP_JMPONT : OP_JMPT);
}


static int code_label (FuncState *fs, OpCode op, int arg) {
  luaK_getlabel(fs);  /* those instructions may be jump targets */
  return luaK_code1(fs, op, arg);
}


void luaK_tostack (LexState *ls, expdesc *v, int onlyone) {
  FuncState *fs = ls->fs;
  if (!discharge(fs, v)) {  /* `v' is an expression? */
    OpCode previous = GET_OPCODE(fs->f->code[fs->pc-1]);
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
      if (ISJUMP(previous) || need_value(fs, v->u.l.f, OP_JMPONF)
                           || need_value(fs, v->u.l.t, OP_JMPONT)) {
        /* expression needs values */
        if (ISJUMP(previous))
          luaK_concat(fs, &v->u.l.t, fs->pc-1);  /* put `previous' in t. list */
        else {
          j = code_label(fs, OP_JMP, NO_JUMP);  /* to jump over both pushes */
          /* correct stack for compiler and symbolic execution */
          luaK_adjuststack(fs, 1);
        }
        p_nil = code_label(fs, OP_PUSHNILJMP, 0);
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


void luaK_prefix (LexState *ls, UnOpr op, expdesc *v) {
  FuncState *fs = ls->fs;
  if (op == OPR_MINUS) {
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


void luaK_infix (LexState *ls, BinOpr op, expdesc *v) {
  FuncState *fs = ls->fs;
  switch (op) {
    case OPR_AND:
      luaK_goiftrue(fs, v, 1);
      break;
    case OPR_OR:
      luaK_goiffalse(fs, v, 1);
      break;
    default:
      luaK_tostack(ls, v, 1);  /* all other binary operators need a value */
  }
}



static const struct {
  OpCode opcode;  /* opcode for each binary operator */
  int arg;        /* default argument for the opcode */
} codes[] = {  /* ORDER OPR */
      {OP_ADD, 0}, {OP_SUB, 0}, {OP_MULT, 0}, {OP_DIV, 0},
      {OP_POW, 0}, {OP_CONCAT, 2},
      {OP_JMPNE, NO_JUMP}, {OP_JMPEQ, NO_JUMP},
      {OP_JMPLT, NO_JUMP}, {OP_JMPLE, NO_JUMP},
      {OP_JMPGT, NO_JUMP}, {OP_JMPGE, NO_JUMP}
};


void luaK_posfix (LexState *ls, BinOpr op, expdesc *v1, expdesc *v2) {
  FuncState *fs = ls->fs;
  switch (op) {
    case OPR_AND: {
      LUA_ASSERT(v1->u.l.t == NO_JUMP, "list must be closed");
      discharge1(fs, v2);
      v1->u.l.t = v2->u.l.t;
      luaK_concat(fs, &v1->u.l.f, v2->u.l.f);
      break;
    }
    case OPR_OR: {
      LUA_ASSERT(v1->u.l.f == NO_JUMP, "list must be closed");
      discharge1(fs, v2);
      v1->u.l.f = v2->u.l.f;
      luaK_concat(fs, &v1->u.l.t, v2->u.l.t);
      break;
    }
    default: {
      luaK_tostack(ls, v2, 1);  /* `v2' must be a value */
      luaK_code1(fs, codes[op].opcode, codes[op].arg);
    }
  }
}


static void codelineinfo (FuncState *fs) {
  Proto *f = fs->f;
  LexState *ls = fs->ls;
  if (ls->lastline > fs->lastline) {
    luaM_growvector(fs->L, f->lineinfo, f->nlineinfo, 2, int,
                    "line info overflow", MAX_INT);
    if (ls->lastline > fs->lastline+1)
      f->lineinfo[f->nlineinfo++] = -(ls->lastline - (fs->lastline+1));
    f->lineinfo[f->nlineinfo++] = fs->pc;
    fs->lastline = ls->lastline;
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
  int delta = luaK_opproperties[o].push - luaK_opproperties[o].pop;
  int optm = 0;  /* 1 when there is an optimization */
  switch (o) {
    case OP_CLOSURE: {
      delta = -arg2+1;
      break;
    }
    case OP_SETTABLE: {
      delta = -arg2;
      break;
    }
    case OP_SETLIST: {
      if (arg2 == 0) return NO_JUMP;  /* nothing to do */
      delta = -arg2;
      break;
    }
    case OP_SETMAP: {
      if (arg1 == 0) return NO_JUMP;  /* nothing to do */
      delta = -2*arg1;
      break;
    }
    case OP_RETURN: {
      if (GET_OPCODE(i) == OP_CALL && GETARG_B(i) == MULT_RET) {
        SET_OPCODE(i, OP_TAILCALL);
        SETARG_B(i, arg1);
        optm = 1;
      }
      break;
    }
    case OP_PUSHNIL: {
      if (arg1 == 0) return NO_JUMP;  /* nothing to do */
      delta = arg1;
      switch(GET_OPCODE(i)) {
        case OP_PUSHNIL: SETARG_U(i, GETARG_U(i)+arg1); optm = 1; break;
        default: break;
      }
      break;
    }
    case OP_POP: {
      if (arg1 == 0) return NO_JUMP;  /* nothing to do */
      delta = -arg1;
      switch(GET_OPCODE(i)) {
        case OP_SETTABLE: SETARG_B(i, GETARG_B(i)+arg1); optm = 1; break;
        default: break;
      }
      break;
    }
    case OP_GETTABLE: {
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
    }
    case OP_ADD: {
      switch(GET_OPCODE(i)) {
        case OP_PUSHINT: SET_OPCODE(i, OP_ADDI); optm = 1; break;  /* `a+k' */
        default: break;
      }
      break;
    }
    case OP_SUB: {
      switch(GET_OPCODE(i)) {
        case OP_PUSHINT:  /* `a-k' */
          i = CREATE_S(OP_ADDI, -GETARG_S(i));
          optm = 1;
          break;
        default: break;
      }
      break;
    }
    case OP_CONCAT: {
      delta = -arg1+1;
      switch(GET_OPCODE(i)) {
        case OP_CONCAT:  /* `a..b..c' */
          SETARG_U(i, GETARG_U(i)+1);
          optm = 1;
          break;
        default: break;
      }
      break;
    }
    case OP_MINUS: {
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
    }
    case OP_JMPNE: {
      if (i == CREATE_U(OP_PUSHNIL, 1)) {  /* `a~=nil' */
        i = CREATE_S(OP_JMPT, NO_JUMP);
        optm = 1;
      }
      break;
    }
    case OP_JMPEQ: {
      if (i == CREATE_U(OP_PUSHNIL, 1)) {  /* `a==nil' */
        i = CREATE_0(OP_NOT);
        delta = -1;  /* just undo effect of previous PUSHNIL */
        optm = 1;
      }
      break;
    }
    case OP_JMPT:
    case OP_JMPONT: {
      switch (GET_OPCODE(i)) {
        case OP_NOT: {
          i = CREATE_S(OP_JMPF, NO_JUMP);
          optm = 1;
          break;
        }
        case OP_PUSHINT: {
          if (o == OP_JMPT) {  /* JMPONT must keep original integer value */
            i = CREATE_S(OP_JMP, NO_JUMP);
            optm = 1;
          }
          break;
        }
        case OP_PUSHNIL: {
          if (GETARG_U(i) == 1) {
            fs->pc--;  /* erase previous instruction */
            luaK_deltastack(fs, -1);  /* correct stack */
            return NO_JUMP; 
          }
          break;
        }
        default: break;
      }
      break;
    }
    case OP_JMPF:
    case OP_JMPONF: {
      switch (GET_OPCODE(i)) {
        case OP_NOT: {
          i = CREATE_S(OP_JMPT, NO_JUMP);
          optm = 1;
          break;
        }
        case OP_PUSHINT: {  /* `while 1 do ...' */
          fs->pc--;  /* erase previous instruction */
          luaK_deltastack(fs, -1);  /* correct stack */
          return NO_JUMP; 
        }
        case OP_PUSHNIL: {  /* `repeat ... until nil' */
          if (GETARG_U(i) == 1) {
            i = CREATE_S(OP_JMP, NO_JUMP);
            optm = 1;
          }
          break;
        }
        default: break;
      }
      break;
    }
    case OP_GETDOTTED:
    case OP_GETINDEXED:
    case OP_TAILCALL:
    case OP_ADDI: {
      LUA_INTERNALERROR("instruction used only for optimizations");
      break;
    }
    default: {
      LUA_ASSERT(delta != VD, "invalid delta");
      break;
    }
  }
  luaK_deltastack(fs, delta);
  if (optm) {  /* optimize: put instruction in place of last one */
      fs->f->code[fs->pc-1] = i;  /* change previous instruction */
      return fs->pc-1;  /* do not generate new instruction */
  }
  /* else build new instruction */
  switch ((enum Mode)luaK_opproperties[o].mode) {
    case iO: i = CREATE_0(o); break;
    case iU: i = CREATE_U(o, arg1); break;
    case iS: i = CREATE_S(o, arg1); break;
    case iAB: i = CREATE_AB(o, arg1, arg2); break;
  }
  codelineinfo(fs);
  /* put new instruction in code array */
  luaM_growvector(fs->L, fs->f->code, fs->pc, 1, Instruction,
                  "code size overflow", MAX_INT);
  fs->f->code[fs->pc] = i;
  return fs->pc++;
}


const struct OpProperties luaK_opproperties[NUM_OPCODES] = {
  {iO, 0, 0},	/* OP_END */
  {iU, 0, 0},	/* OP_RETURN */
  {iAB, 0, 0},	/* OP_CALL */
  {iAB, 0, 0},	/* OP_TAILCALL */
  {iU, VD, 0},	/* OP_PUSHNIL */
  {iU, VD, 0},	/* OP_POP */
  {iS, 1, 0},	/* OP_PUSHINT */
  {iU, 1, 0},	/* OP_PUSHSTRING */
  {iU, 1, 0},	/* OP_PUSHNUM */
  {iU, 1, 0},	/* OP_PUSHNEGNUM */
  {iU, 1, 0},	/* OP_PUSHUPVALUE */
  {iU, 1, 0},	/* OP_GETLOCAL */
  {iU, 1, 0},	/* OP_GETGLOBAL */
  {iO, 1, 2},	/* OP_GETTABLE */
  {iU, 1, 1},	/* OP_GETDOTTED */
  {iU, 1, 1},	/* OP_GETINDEXED */
  {iU, 2, 1},	/* OP_PUSHSELF */
  {iU, 1, 0},	/* OP_CREATETABLE */
  {iU, 0, 1},	/* OP_SETLOCAL */
  {iU, 0, 1},	/* OP_SETGLOBAL */
  {iAB, VD, 0},	/* OP_SETTABLE */
  {iAB, VD, 0},	/* OP_SETLIST */
  {iU, VD, 0},	/* OP_SETMAP */
  {iO, 1, 2},	/* OP_ADD */
  {iS, 1, 1},	/* OP_ADDI */
  {iO, 1, 2},	/* OP_SUB */
  {iO, 1, 2},	/* OP_MULT */
  {iO, 1, 2},	/* OP_DIV */
  {iO, 1, 2},	/* OP_POW */
  {iU, VD, 0},	/* OP_CONCAT */
  {iO, 1, 1},	/* OP_MINUS */
  {iO, 1, 1},	/* OP_NOT */
  {iS, 0, 2},	/* OP_JMPNE */
  {iS, 0, 2},	/* OP_JMPEQ */
  {iS, 0, 2},	/* OP_JMPLT */
  {iS, 0, 2},	/* OP_JMPLE */
  {iS, 0, 2},	/* OP_JMPGT */
  {iS, 0, 2},	/* OP_JMPGE */
  {iS, 0, 1},	/* OP_JMPT */
  {iS, 0, 1},	/* OP_JMPF */
  {iS, 0, 1},	/* OP_JMPONT */
  {iS, 0, 1},	/* OP_JMPONF */
  {iS, 0, 0},	/* OP_JMP */
  {iO, 0, 0},	/* OP_PUSHNILJMP */
  {iS, 0, 0},	/* OP_FORPREP */
  {iS, 0, 3},	/* OP_FORLOOP */
  {iS, 2, 0},	/* OP_LFORPREP */
  {iS, 0, 3},	/* OP_LFORLOOP */
  {iAB, VD, 0}	/* OP_CLOSURE */
};

