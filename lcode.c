/*
** $Id: lcode.c,v 1.90 2002/03/05 12:42:47 roberto Exp roberto $
** Code generator for Lua
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#include "lua.h"

#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "ltable.h"


#define hasjumps(e)	((e)->t != (e)->f)

#define getcode(fs,e)	((fs)->f->code[(e)->info])



void luaK_error (LexState *ls, const char *msg) {
  luaX_error(ls, msg, ls->t.token);
}


void luaK_nil (FuncState *fs, int from, int n) {
  Instruction *previous;
  if (fs->pc > fs->lasttarget &&  /* no jumps to current position? */
      GET_OPCODE(*(previous = &fs->f->code[fs->pc-1])) == OP_LOADNIL) {
    int pfrom = GETARG_A(*previous);
    int pto = GETARG_B(*previous);
    if (pfrom <= from && from <= pto+1) {  /* can connect both? */
      if (from+n-1 > pto)
        SETARG_B(*previous, from+n-1);
      return;
    }
  }
  luaK_codeABC(fs, OP_LOADNIL, from, from+n-1, 0);  /* else no optimization */
}


int luaK_jump (FuncState *fs) {
  int j = luaK_codeAsBc(fs, OP_JMP, 0, NO_JUMP);
  if (j == fs->lasttarget) {  /* possible jumps to this jump? */
    luaK_concat(fs, &j, fs->jlt);  /* keep them on hold */
    fs->jlt = NO_JUMP;
  }
  return j;
}


static int luaK_condjump (FuncState *fs, OpCode op, int A, int B, int C) {
  luaK_codeABC(fs, op, A, B, C);
  return luaK_codeAsBc(fs, OP_JMP, 0, NO_JUMP);
}


static void luaK_fixjump (FuncState *fs, int pc, int dest) {
  Instruction *jmp = &fs->f->code[pc];
  if (dest == NO_JUMP)
    SETARG_sBc(*jmp, NO_JUMP);  /* point to itself to represent end of list */
  else {  /* jump is relative to position following jump instruction */
    int offset = dest-(pc+1);
    if (abs(offset) > MAXARG_sBc)
      luaK_error(fs->ls, "control structure too long");
    SETARG_sBc(*jmp, offset);
  }
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


static int luaK_getjump (FuncState *fs, int pc) {
  int offset = GETARG_sBc(fs->f->code[pc]);
  if (offset == NO_JUMP)  /* point to itself represents end of list */
    return NO_JUMP;  /* end of list */
  else
    return (pc+1)+offset;  /* turn offset into absolute position */
}


static Instruction *getjumpcontrol (FuncState *fs, int pc) {
  Instruction *pi = &fs->f->code[pc];
  if (pc >= 1 && testOpMode(GET_OPCODE(*(pi-1)), OpModeT))
    return pi-1;
  else
    return pi;
}


static int need_value (FuncState *fs, int list, OpCode op) {
  /* check whether list has any jump different from `op' */
  for (; list != NO_JUMP; list = luaK_getjump(fs, list))
    if (GET_OPCODE(*getjumpcontrol(fs, list)) != op) return 1;
  return 0;  /* not found */
}


static void patchtestreg (Instruction *i, int reg) {
  if (reg == NO_REG) reg = GETARG_B(*i);
  SETARG_A(*i, reg);
}


static void luaK_patchlistaux (FuncState *fs, int list,
          int ttarget, int treg, int ftarget, int freg, int dtarget) {
  while (list != NO_JUMP) {
    int next = luaK_getjump(fs, list);
    Instruction *i = getjumpcontrol(fs, list);
    switch (GET_OPCODE(*i)) {
      case OP_TESTT: {
        patchtestreg(i, treg);
        luaK_fixjump(fs, list, ttarget);
        break;
      }
      case OP_TESTF: {
        patchtestreg(i, freg);
        luaK_fixjump(fs, list, ftarget);
        break;
      }
      default: {
        luaK_fixjump(fs, list, dtarget);  /* jump to default target */
        break;
      }
    }
    list = next;
  }
}


void luaK_patchlist (FuncState *fs, int list, int target) {
  if (target == fs->lasttarget)  /* same target that list `jlt'? */
    luaK_concat(fs, &fs->jlt, list);  /* delay fixing */
  else
    luaK_patchlistaux(fs, list, target, NO_REG, target, NO_REG, target);
}


void luaK_patchtohere (FuncState *fs, int list) {
  luaK_patchlist(fs, list, luaK_getlabel(fs));
}


void luaK_concat (FuncState *fs, int *l1, int l2) {
  if (*l1 == NO_JUMP)
    *l1 = l2;
  else {
    int list = *l1;
    int next;
    while ((next = luaK_getjump(fs, list)) != NO_JUMP)  /* find last element */
      list = next;
    luaK_fixjump(fs, list, l2);
  }
}


void luaK_checkstack (FuncState *fs, int n) {
  int newstack = fs->freereg + n;
  if (newstack > fs->f->maxstacksize) {
    if (newstack >= MAXSTACK)
      luaK_error(fs->ls, "function or expression too complex");
    fs->f->maxstacksize = cast(lu_byte, newstack);
  }
}


void luaK_reserveregs (FuncState *fs, int n) {
  luaK_checkstack(fs, n);
  fs->freereg += n;
}


static void freereg (FuncState *fs, int reg) {
  if (reg >= fs->nactloc && reg < MAXSTACK) {
    fs->freereg--;
    lua_assert(reg == fs->freereg);
  }
}


static void freeexp (FuncState *fs, expdesc *e) {
  if (e->k == VNONRELOC)
    freereg(fs, e->info);
}


static int addk (FuncState *fs, TObject *k, TObject *v) {
  const TObject *index = luaH_get(fs->h, k);
  if (ttype(index) == LUA_TNUMBER) {
    lua_assert(luaO_equalObj(&fs->f->k[cast(int, nvalue(index))], v));
    return cast(int, nvalue(index));
  }
  else {  /* constant not found; create a new entry */
    TObject o;
    Proto *f = fs->f;
    luaM_growvector(fs->L, f->k, fs->nk, f->sizek, TObject,
                    MAXARG_Bc, "constant table overflow");
    setobj(&f->k[fs->nk], v);
    setnvalue(&o, fs->nk);
    luaH_set(fs->L, fs->h, k, &o);
    return fs->nk++;
  }
}


int luaK_stringK (FuncState *fs, TString *s) {
  TObject o;
  setsvalue(&o, s);
  return addk(fs, &o, &o);
}


int luaK_numberK (FuncState *fs, lua_Number r) {
  TObject o;
  setnvalue(&o, r);
  return addk(fs, &o, &o);
}


static int nil_constant (FuncState *fs) {
  TObject k, v;
  setnilvalue(&v);
  sethvalue(&k, fs->h);  /* cannot use nil as key; instead use table itself */
  return addk(fs, &k, &v);
}


void luaK_setcallreturns (FuncState *fs, expdesc *e, int nresults) {
  if (e->k == VCALL) {  /* expression is an open function call? */
    SETARG_C(getcode(fs, e), nresults+1);
    if (nresults == 1) {  /* `regular' expression? */
      e->k = VNONRELOC;
      e->info = GETARG_A(getcode(fs, e));
    }
  }
}


void luaK_dischargevars (FuncState *fs, expdesc *e) {
  switch (e->k) {
    case VLOCAL: {
      e->k = VNONRELOC;
      break;
    }
    case VUPVAL: {
      e->info = luaK_codeABC(fs, OP_GETUPVAL, 0, e->info, 0);
      e->k = VRELOCABLE;
      break;
    }
    case VGLOBAL: {
      e->info = luaK_codeABc(fs, OP_GETGLOBAL, 0, e->info);
      e->k = VRELOCABLE;
      break;
    }
    case VINDEXED: {
      freereg(fs, e->aux);
      freereg(fs, e->info);
      e->info = luaK_codeABC(fs, OP_GETTABLE, 0, e->info, e->aux);
      e->k = VRELOCABLE;
      break;
    }
    case VCALL: {
      luaK_setcallreturns(fs, e, 1);
      break;
    }
    default: break;  /* there is one value available (somewhere) */
  }
}


static int code_label (FuncState *fs, int A, int b, int jump) {
  luaK_getlabel(fs);  /* those instructions may be jump targets */
  return luaK_codeABC(fs, OP_LOADBOOL, A, b, jump);
}


static void dischargejumps (FuncState *fs, expdesc *e, int reg) {
  if (e->k == VJMP || hasjumps(e)) {
    int final;  /* position after whole expression */
    int p_f = NO_JUMP;  /* position of an eventual PUSH false */
    int p_t = NO_JUMP;  /* position of an eventual PUSH true */
    if (e->k == VJMP || need_value(fs, e->f, OP_TESTF) ||
                        need_value(fs, e->t, OP_TESTT)) {
      /* expression needs values */
      if (e->k != VJMP) {
        luaK_getlabel(fs);  /* these instruction may be jump target */
        luaK_codeAsBc(fs, OP_JMP, 0, 2);  /* to jump over both pushes */
      }
      else {  /* last expression is a conditional (test + jump) */
        fs->pc--;  /* remove its jump */
        lua_assert(testOpMode(GET_OPCODE(fs->f->code[fs->pc - 1]), OpModeT));
      }
      p_t = code_label(fs, reg, 1, 1);
      p_f = code_label(fs, reg, 0, 0);
    }
    final = luaK_getlabel(fs);
    luaK_patchlistaux(fs, e->f, p_f, NO_REG, final, reg, p_f);
    luaK_patchlistaux(fs, e->t, final, reg, p_t, NO_REG, p_t);
  }
  e->f = e->t = NO_JUMP;
}


static void discharge2reg (FuncState *fs, expdesc *e, int reg) {
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: {
      luaK_nil(fs, reg, 1);
      break;
    }
    case VFALSE:  case VTRUE: {
      luaK_codeABC(fs, OP_LOADBOOL, reg, e->k == VTRUE, 0);
      break;
    }
    case VK: {
      luaK_codeABc(fs, OP_LOADK, reg, e->info);
      break;
    }
    case VRELOCABLE: {
      Instruction *pc = &getcode(fs, e);
      SETARG_A(*pc, reg);
      break;
    }
    default: return;
  }
  e->info = reg;
  e->k = VNONRELOC;
}


static void discharge2anyreg (FuncState *fs, expdesc *e) {
  if (e->k != VNONRELOC) {
    luaK_reserveregs(fs, 1);
    discharge2reg(fs, e, fs->freereg-1);
  }
}


static void luaK_exp2reg (FuncState *fs, expdesc *e, int reg) {
  discharge2reg(fs, e, reg);
  switch (e->k) {
    case VVOID: {
      return;  /* nothing to do... */
    }
    case VNONRELOC: {
      if (reg != e->info)
        luaK_codeABC(fs, OP_MOVE, reg, e->info, 0);
      break;
    }
    case VJMP: {
      break;
    }
    default: {
      lua_assert(0);  /* cannot happen */
      break;
    }
  }
  dischargejumps(fs, e, reg);
  e->info = reg;
  e->k = VNONRELOC;
}


void luaK_exp2nextreg (FuncState *fs, expdesc *e) {
  int reg;
  luaK_dischargevars(fs, e);
  freeexp(fs, e);
  reg = fs->freereg;
  luaK_reserveregs(fs, 1);
  luaK_exp2reg(fs, e, reg);
}


int luaK_exp2anyreg (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  if (e->k == VNONRELOC) {
    if (!hasjumps(e)) return e->info;  /* exp is already in a register */ 
    if (e->info >= fs->nactloc) {  /* reg. is not a local? */
      dischargejumps(fs, e, e->info);  /* put value on it */
      return e->info;
    }
  }
  luaK_exp2nextreg(fs, e);  /* default */
  return e->info;
}


void luaK_exp2val (FuncState *fs, expdesc *e) {
  if (hasjumps(e))
    luaK_exp2anyreg(fs, e);
  else
    luaK_dischargevars(fs, e);
}


int luaK_exp2RK (FuncState *fs, expdesc *e) {
  luaK_exp2val(fs, e);
  switch (e->k) {
    case VNIL: {
      if (fs->nk + MAXSTACK <= MAXARG_C) {  /* constant fit in argC? */
        e->info = nil_constant(fs);
        e->k = VK;
        return e->info + MAXSTACK;
      }
      else break;
    }
    case VK: {
      if (e->info + MAXSTACK <= MAXARG_C)  /* constant fit in argC? */
        return e->info + MAXSTACK;
      else break;
    }
    default: break;
  }
  /* not a constant in the right range: put in a register */
  return luaK_exp2anyreg(fs, e);
}


void luaK_storevar (FuncState *fs, expdesc *var, expdesc *exp) {
  switch (var->k) {
    case VLOCAL: {
      freeexp(fs, exp);
      luaK_exp2reg(fs, exp, var->info);
      break;
    }
    case VUPVAL: {
      int e = luaK_exp2anyreg(fs, exp);
      freereg(fs, e);
      luaK_codeABC(fs, OP_SETUPVAL, e, var->info, 0);
      break;
    }
    case VGLOBAL: {
      int e = luaK_exp2anyreg(fs, exp);
      freereg(fs, e);
      luaK_codeABc(fs, OP_SETGLOBAL, e, var->info);
      break;
    }
    case VINDEXED: {
      int e = luaK_exp2anyreg(fs, exp);
      freereg(fs, e);
      luaK_codeABC(fs, OP_SETTABLE, e, var->info, var->aux);
      break;
    }
    default: {
      lua_assert(0);  /* invalid var kind to store */
      break;
    }
  }
}


void luaK_self (FuncState *fs, expdesc *e, expdesc *key) {
  int func;
  luaK_exp2anyreg(fs, e);
  freeexp(fs, e);
  func = fs->freereg;
  luaK_reserveregs(fs, 2);
  luaK_codeABC(fs, OP_SELF, func, e->info, luaK_exp2RK(fs, key));
  freeexp(fs, key);
  e->info = func;
  e->k = VNONRELOC;
}


static OpCode invertoperator (OpCode op) {
  switch (op) {
    case OP_TESTNE: return OP_TESTEQ;
    case OP_TESTEQ: return OP_TESTNE;
    case OP_TESTLT: return OP_TESTGE;
    case OP_TESTLE: return OP_TESTGT;
    case OP_TESTGT: return OP_TESTLE;
    case OP_TESTGE: return OP_TESTLT;
    case OP_TESTT: return OP_TESTF;
    case OP_TESTF: return OP_TESTT;
    default: lua_assert(0); return op;  /* invalid jump instruction */
  }
}


static void invertjump (FuncState *fs, expdesc *e) {
  Instruction *pc = getjumpcontrol(fs, e->info);
  *pc = SET_OPCODE(*pc, invertoperator(GET_OPCODE(*pc)));
}


static int jumponcond (FuncState *fs, expdesc *e, OpCode op) {
  if (e->k == VRELOCABLE) {
    Instruction ie = getcode(fs, e);
    if (GET_OPCODE(ie) == OP_NOT) {
      op = invertoperator(op);
      fs->pc--;  /* remove previous OP_NOT */
      return luaK_condjump(fs, op, NO_REG, GETARG_B(ie), 0);
    }
    /* else go through */
  }
  discharge2anyreg(fs, e);
  freeexp(fs, e);
  return luaK_condjump(fs, op, NO_REG, e->info, 0);
}


void luaK_goiftrue (FuncState *fs, expdesc *e) {
  int pc;  /* pc of last jump */
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VK: case VTRUE: {
      pc = NO_JUMP;  /* always true; do nothing */
      break;
    }
    case VFALSE: {
      pc = luaK_codeAsBc(fs, OP_JMP, 0, NO_JUMP);  /* always jump */
      break;
    }
    case VJMP: {
      invertjump(fs, e);
      pc = e->info;
      break;
    }
    default: {
      pc = jumponcond(fs, e, OP_TESTF);
      break;
    }
  }
  luaK_concat(fs, &e->f, pc);  /* insert last jump in `f' list */
  luaK_patchtohere(fs, e->t);
  e->t = NO_JUMP;
}


static void luaK_goiffalse (FuncState *fs, expdesc *e) {
  int pc;  /* pc of last jump */
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: case VFALSE: {
      pc = NO_JUMP;  /* always false; do nothing */
      break;
    }
    case VTRUE: {
      pc = luaK_codeAsBc(fs, OP_JMP, 0, NO_JUMP);  /* always jump */
      break;
    }
    case VJMP: {
      pc = e->info;
      break;
    }
    default: {
      pc = jumponcond(fs, e, OP_TESTT);
      break;
    }
  }
  luaK_concat(fs, &e->t, pc);  /* insert last jump in `t' list */
  luaK_patchtohere(fs, e->f);
  e->f = NO_JUMP;
}


static void codenot (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: case VFALSE: {
      e->k = VTRUE;
      break;
    }
    case VK: case VTRUE: {
      e->k = VFALSE;
      break;
    }
    case VJMP: {
      invertjump(fs, e);
      break;
    }
    case VRELOCABLE:
    case VNONRELOC: {
      discharge2anyreg(fs, e);
      freeexp(fs, e);
      e->info = luaK_codeABC(fs, OP_NOT, 0, e->info, 0);
      e->k = VRELOCABLE;
      break;
    }
    default: {
      lua_assert(0);  /* cannot happen */
      break;
    }
  }
  /* interchange true and false lists */
  { int temp = e->f; e->f = e->t; e->t = temp; }
}


void luaK_indexed (FuncState *fs, expdesc *t, expdesc *k) {
  t->aux = luaK_exp2RK(fs, k);
  t->k = VINDEXED;
}


void luaK_prefix (FuncState *fs, UnOpr op, expdesc *e) {
  if (op == OPR_MINUS) {
    luaK_exp2val(fs, e);
    if (e->k == VK && ttype(&fs->f->k[e->info]) == LUA_TNUMBER)
      e->info = luaK_numberK(fs, -nvalue(&fs->f->k[e->info]));
    else {
      luaK_exp2anyreg(fs, e);
      freeexp(fs, e);
      e->info = luaK_codeABC(fs, OP_UNM, 0, e->info, 0);
      e->k = VRELOCABLE;
    }
  }
  else  /* op == NOT */
    codenot(fs, e);
}


void luaK_infix (FuncState *fs, BinOpr op, expdesc *v) {
  switch (op) {
    case OPR_AND: {
      luaK_goiftrue(fs, v);
      break;
    }
    case OPR_OR: {
      luaK_goiffalse(fs, v);
      break;
    }
    case OPR_CONCAT: {
      luaK_exp2nextreg(fs, v);  /* operand must be on the `stack' */
      break;
    }
    case OPR_SUB: case OPR_DIV: case OPR_POW: {
      /* non-comutative operators */
      luaK_exp2anyreg(fs, v);  /* first operand must be a register */
      break;
    }
    default: {
      luaK_exp2RK(fs, v);
      break;
    }
  }
}



/* opcode for each binary operator */
static const OpCode codes[] = {  /* ORDER OPR */
  OP_ADD, OP_SUB, OP_MUL, OP_DIV,
  OP_POW, OP_CONCAT,
  OP_TESTNE, OP_TESTEQ,
  OP_TESTLT, OP_TESTLE, OP_TESTGT, OP_TESTGE
};


/* `inverted' opcode for each binary operator */
/* ( -1 means operator has no inverse) */
static const OpCode invcodes[] = {  /* ORDER OPR */
  OP_ADD, (OpCode)-1, OP_MUL, (OpCode)-1,
  (OpCode)-1, (OpCode)-1,
  OP_TESTNE, OP_TESTEQ,
  OP_TESTGT, OP_TESTGE, OP_TESTLT, OP_TESTLE
};


void luaK_posfix (FuncState *fs, BinOpr op, expdesc *e1, expdesc *e2) {
  switch (op) {
    case OPR_AND: {
      lua_assert(e1->t == NO_JUMP);  /* list must be closed */
      luaK_dischargevars(fs, e2);
      luaK_concat(fs, &e1->f, e2->f);
      e1->k = e2->k; e1->info = e2->info; e1->aux = e2->aux; e1->t = e2->t;
      break;
    }
    case OPR_OR: {
      lua_assert(e1->f == NO_JUMP);  /* list must be closed */
      luaK_dischargevars(fs, e2);
      luaK_concat(fs, &e1->t, e2->t);
      e1->k = e2->k; e1->info = e2->info; e1->aux = e2->aux; e1->f = e2->f;
      break;
    }
    case OPR_CONCAT: {
      luaK_exp2val(fs, e2);
      if (e2->k == VRELOCABLE && GET_OPCODE(getcode(fs, e2)) == OP_CONCAT) {
        lua_assert(e1->info == GETARG_B(getcode(fs, e2))-1);
        freeexp(fs, e1);
        SETARG_B(getcode(fs, e2), e1->info);
        e1->k = e2->k; e1->info = e2->info;
      }
      else {
        luaK_exp2nextreg(fs, e2);
        freeexp(fs, e2);
        freeexp(fs, e1);
        e1->info = luaK_codeABC(fs, codes[op], 0, e1->info, e2->info);
        e1->k = VRELOCABLE;
      }
      break;
    }
    default: {
      int o1, o2;
      OpCode opc;
      if (e1->k != VK) {  /* not a constant operator? */
        o1 = e1->info;
        o2 = luaK_exp2RK(fs, e2);  /* maybe other operator is constant... */
        opc = codes[op];
      }
      else {  /* invert operands */
        o2 = luaK_exp2RK(fs, e1);  /* constant must be 2nd operand */
        o1 = luaK_exp2anyreg(fs, e2);  /* other operator must be in register */
        opc = invcodes[op];  /* use inverted operator */
      }
      freeexp(fs, e2);
      freeexp(fs, e1);
      if (op < OPR_NE) {  /* ORDER OPR */
        e1->info = luaK_codeABC(fs, opc, 0, o1, o2);
        e1->k = VRELOCABLE;
      }
      else {  /* jump */
        e1->info = luaK_condjump(fs, opc, o1, 0, o2);
        e1->k = VJMP;
      }
    }
  }
}


static void codelineinfo (FuncState *fs) {
  Proto *f = fs->f;
  LexState *ls = fs->ls;
  if (ls->lastline > fs->lastline) {
    if (ls->lastline > fs->lastline+1) {
      luaM_growvector(fs->L, f->lineinfo, fs->nlineinfo, f->sizelineinfo, int,
                      MAX_INT, "line info overflow");
      f->lineinfo[fs->nlineinfo++] = -(ls->lastline - (fs->lastline+1));
    }
    luaM_growvector(fs->L, f->lineinfo, fs->nlineinfo, f->sizelineinfo, int,
                    MAX_INT, "line info overflow");
    f->lineinfo[fs->nlineinfo++] = fs->pc;
    fs->lastline = ls->lastline;
  }
}


static int luaK_code (FuncState *fs, Instruction i) {
  Proto *f;
  codelineinfo(fs);
  f = fs->f;
  /* put new instruction in code array */
  luaM_growvector(fs->L, f->code, fs->pc, f->sizecode, Instruction,
                  MAX_INT, "code size overflow");
  f->code[fs->pc] = i;
  return fs->pc++;
}


int luaK_codeABC (FuncState *fs, OpCode o, int a, int b, int c) {
  lua_assert(getOpMode(o) == iABC);
  return luaK_code(fs, CREATE_ABC(o, a, b, c));
}


int luaK_codeABc (FuncState *fs, OpCode o, int a, unsigned int bc) {
  lua_assert(getOpMode(o) == iABc || getOpMode(o) == iAsBc);
  return luaK_code(fs, CREATE_ABc(o, a, bc));
}

