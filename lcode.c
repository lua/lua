/*
** $Id: lcode.c,v 1.75 2001/06/12 14:36:48 roberto Exp roberto $
** Code generator for Lua
** See Copyright Notice in lua.h
*/


#include <stdlib.h>

#define LUA_PRIVATE
#include "lua.h"

#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


#define hasjumps(e)	((e)->t != (e)->f)

#define getcode(fs,e)	((fs)->f->code[(e)->u.i.info])



void luaK_error (LexState *ls, const l_char *msg) {
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
    return (Instruction)(-1);/* no optimizations after an invalid instruction */
}


void luaK_nil (FuncState *fs, int from, int n) {
  Instruction previous = previous_instruction(fs);
  if (GET_OPCODE(previous) == OP_LOADNIL) {
    int pfrom = GETARG_A(previous);
    int pto = GETARG_B(previous);
    if (pfrom <= from && from <= pto+1) {  /* can connect both? */
      if (from+n-1 > pto)
        SETARG_B(fs->f->code[fs->pc-1], from+n-1);
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
  return luaK_codeAsBc(fs, OP_CJMP, 0, NO_JUMP);
}


static void luaK_fixjump (FuncState *fs, int pc, int dest) {
  Instruction *jmp = &fs->f->code[pc];
  if (dest == NO_JUMP)
    SETARG_sBc(*jmp, NO_JUMP);  /* point to itself to represent end of list */
  else {  /* jump is relative to position following jump instruction */
    int offset = dest-(pc+1);
    if (abs(offset) > MAXARG_sBc)
      luaK_error(fs->ls, l_s("control structure too long"));
    SETARG_sBc(*jmp, offset);
  }
}


/*
** prep-for instructions (OP_FORPREP & OP_TFORPREP) have a negated jump,
** as they simulate the real jump...
*/
void luaK_fixfor (FuncState *fs, int pc, int dest) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest-(pc+1);
  SETARG_sBc(*jmp, -offset);
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
  OpCode op = GET_OPCODE(*pi);
  if (op == OP_CJMP)
    return pi-1;
  else {
    lua_assert(op == OP_JMP || op == OP_FORLOOP || op == OP_TFORLOOP);
    return pi;
  }
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


void luaK_reserveregs (FuncState *fs, int n) {
  fs->freereg += n;
  if (fs->freereg > fs->f->maxstacksize) {
    if (fs->freereg >= MAXSTACK)
      luaK_error(fs->ls, l_s("function or expression too complex"));
    fs->f->maxstacksize = (short)fs->freereg;
  }
}


static void freereg (FuncState *fs, int reg) {
  if (reg >= fs->nactloc && reg < MAXSTACK) {
    fs->freereg--;
    lua_assert(reg == fs->freereg);
  }
}


static void freeexp (FuncState *fs, expdesc *e) {
  if (e->k == VNONRELOC)
    freereg(fs, e->u.i.info);
}


static int addk (FuncState *fs, TObject *k) {
  Proto *f = fs->f;
  luaM_growvector(fs->L, f->k, fs->nk, f->sizek, TObject,
                  MAXARG_Bc, l_s("constant table overflow"));
  setobj(&f->k[fs->nk], k);
  return fs->nk++;
}


int luaK_stringk (FuncState *fs, TString *s) {
  Proto *f = fs->f;
  int c = s->tsv.constindex;
  if (c >= fs->nk || ttype(&f->k[c]) != LUA_TSTRING || tsvalue(&f->k[c]) != s) {
    TObject o;
    setsvalue(&o, s);
    c = addk(fs, &o);
    s->tsv.constindex = (unsigned short)c;  /* hint for next time */
  }
  return c;
}


static int number_constant (FuncState *fs, lua_Number r) {
  /* check whether `r' has appeared within the last LOOKBACKNUMS entries */
  TObject o;
  Proto *f = fs->f;
  int c = fs->nk;
  int lim = c < LOOKBACKNUMS ? 0 : c-LOOKBACKNUMS;
  while (--c >= lim) {
    if (ttype(&f->k[c]) == LUA_TNUMBER && nvalue(&f->k[c]) == r)
      return c;
  }
  /* not found; create a new entry */
  setnvalue(&o, r);
  return addk(fs, &o);
}


void luaK_setcallreturns (FuncState *fs, expdesc *e, int nresults) {
  if (e->k == VCALL) {  /* expression is an open function call? */
    if (nresults == LUA_MULTRET) nresults = NO_REG;
    SETARG_C(getcode(fs, e), nresults);
    if (nresults == 1) {  /* `regular' expression? */
      e->k = VNONRELOC;
      e->u.i.info = GETARG_A(getcode(fs, e));
    }
  }
}


void luaK_dischargevars (FuncState *fs, expdesc *e) {
  switch (e->k) {
    case VLOCAL: {
      e->k = VNONRELOC;
      break;
    }
    case VGLOBAL: {
      e->u.i.info = luaK_codeABc(fs, OP_GETGLOBAL, 0, e->u.i.info);
      e->k = VRELOCABLE;
      break;
    }
    case VINDEXED: {
      freereg(fs, e->u.i.aux);
      freereg(fs, e->u.i.info);
      e->u.i.info = luaK_codeABC(fs, OP_GETTABLE, 0, e->u.i.info, e->u.i.aux);
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


static int code_label (FuncState *fs, OpCode op, int A, int sBc) {
  luaK_getlabel(fs);  /* those instructions may be jump targets */
  return luaK_codeAsBc(fs, op, A, sBc);
}


static void dischargejumps (FuncState *fs, expdesc *e, int reg) {
  if (hasjumps(e)) {
    int final;  /* position after whole expression */
    int p_nil = NO_JUMP;  /* position of an eventual PUSHNIL */
    int p_1 = NO_JUMP;  /* position of an eventual PUSHINT */
    if (need_value(fs, e->f, OP_TESTF) || need_value(fs, e->t, OP_TESTT)) {
      /* expression needs values */
      if (e->k != VJMP)
        code_label(fs, OP_JMP, 0, 2);  /* to jump over both pushes */
      p_nil = code_label(fs, OP_NILJMP, reg, 0);
      p_1 = code_label(fs, OP_LOADINT, reg, 1);
    }
    final = luaK_getlabel(fs);
    luaK_patchlistaux(fs, e->f, p_nil, NO_REG, final,    reg, p_nil);
    luaK_patchlistaux(fs, e->t, final,    reg,   p_1, NO_REG,   p_1);
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
    case VNUMBER: {
      lua_Number f = e->u.n;
      int i = (int)f;
      if ((lua_Number)i == f && -MAXARG_sBc <= i && i <= MAXARG_sBc)
        luaK_codeAsBc(fs, OP_LOADINT, reg, i);  /* f has a small int value */
      else
        luaK_codeABc(fs, OP_LOADK, reg, number_constant(fs, f));
      break;
    }
    case VK: {
      luaK_codeABc(fs, OP_LOADK, reg, e->u.i.info);
      break;
    }
    case VRELOCABLE: {
      Instruction *pc = &getcode(fs, e);
      SETARG_A(*pc, reg);
      break;
    }
    default: return;
  }
  e->u.i.info = reg;
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
      if (reg != e->u.i.info)
        luaK_codeABC(fs, OP_MOVE, reg, e->u.i.info, 0);
      break;
    }
    case VJMP: {
      luaK_concat(fs, &e->t, e->u.i.info);  /* put this jump in `t' list */
      break;
    }
    default: {
      lua_assert(0);  /* cannot happen */
      break;
    }
  }
  dischargejumps(fs, e, reg);
  e->u.i.info = reg;
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
    if (!hasjumps(e)) return e->u.i.info;  /* exp is already in a register */ 
    if (e->u.i.info >= fs->nactloc) {  /* reg. is not a local? */
      dischargejumps(fs, e, e->u.i.info);  /* put value on it */
      return e->u.i.info;
    }
  }
  luaK_exp2nextreg(fs, e);  /* default */
  return e->u.i.info;
}


void luaK_exp2val (FuncState *fs, expdesc *e) {
  if (hasjumps(e))
    luaK_exp2anyreg(fs, e);
  else
    luaK_dischargevars(fs, e);
}


int luaK_exp2RK (FuncState *fs, expdesc *e) {
  luaK_exp2val(fs, e);
  if (e->k == VNUMBER && fs->nk + MAXSTACK <= MAXARG_C) {
      e->u.i.info = number_constant(fs, e->u.n);
      e->k = VK;
  }
  else if (!(e->k == VK && e->u.i.info + MAXSTACK <= MAXARG_C))
    luaK_exp2anyreg(fs, e);  /* not a constant in the right range */
  return (e->k == VK) ? e->u.i.info+MAXSTACK : e->u.i.info;
}


void luaK_storevar (FuncState *fs, expdesc *var, expdesc *exp) {
  switch (var->k) {
    case VLOCAL: {
      freeexp(fs, exp);
      luaK_exp2reg(fs, exp, var->u.i.info);
      break;
    }
    case VGLOBAL: {
      int e = luaK_exp2anyreg(fs, exp);
      freereg(fs, e);
      luaK_codeABc(fs, OP_SETGLOBAL, e, var->u.i.info);
      break;
    }
    case VINDEXED: {
      int e = luaK_exp2anyreg(fs, exp);
      freereg(fs, e);
      luaK_codeABC(fs, OP_SETTABLE, e, var->u.i.info, var->u.i.aux);
      break;
    }
    default: {
      lua_assert(0);  /* invalid var kind to store */
      break;
    }
  }
}


void luaK_self (FuncState *fs, expdesc *e, expdesc *key) {
  luaK_exp2anyreg(fs, e);
  freeexp(fs, e);
  luaK_reserveregs(fs, 2);
  luaK_codeABC(fs, OP_SELF, fs->freereg-2, e->u.i.info, luaK_exp2RK(fs, key));
  e->u.i.info = fs->freereg-2;
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
  Instruction *pc = getjumpcontrol(fs, e->u.i.info);
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
  return luaK_condjump(fs, op, NO_REG, e->u.i.info, 0);
}


void luaK_goiftrue (FuncState *fs, expdesc *e) {
  int pc;  /* pc of last jump */
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VK: case VNUMBER: {
      pc = NO_JUMP;  /* always true; do nothing */
      break;
    }
    case VNIL: {
      pc = luaK_codeAsBc(fs, OP_JMP, 0, NO_JUMP);  /* always jump */
      break;
    }
    case VJMP: {
      invertjump(fs, e);
      pc = e->u.i.info;
      break;
    }
    case VRELOCABLE:
    case VNONRELOC: {
      pc = jumponcond(fs, e, OP_TESTF);
      break;
    }
    default: {
      pc = 0;  /* to avoid warnings */
      lua_assert(0);  /* cannot happen */
      break;
    }
  }
  luaK_concat(fs, &e->f, pc);  /* insert last jump in `f' list */
  luaK_patchlist(fs, e->t, luaK_getlabel(fs));
  e->t = NO_JUMP;
}


static void luaK_goiffalse (FuncState *fs, expdesc *e) {
  int pc;  /* pc of last jump */
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: {
      pc = NO_JUMP;  /* always false; do nothing */
      break;
    }
    case VJMP: {
      pc = e->u.i.info;
      break;
    }
    case VK: case VNUMBER:  /* cannot optimize it (`or' must keep value) */
    case VRELOCABLE:
    case VNONRELOC: {
      pc = jumponcond(fs, e, OP_TESTT);
      break;
    }
    default: {
      pc = 0;  /* to avoid warnings */
      lua_assert(0);  /* cannot happen */
      break;
    }
  }
  luaK_concat(fs, &e->t, pc);  /* insert last jump in `t' list */
  luaK_patchlist(fs, e->f, luaK_getlabel(fs));
  e->f = NO_JUMP;
}


static void codenot (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: {
      e->u.n = 1;
      e->k = VNUMBER;
      break;
    }
    case VK:  case VNUMBER: {
      e->k = VNIL;
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
      e->u.i.info = luaK_codeABC(fs, OP_NOT, 0, e->u.i.info, 0);
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
  t->u.i.aux = luaK_exp2RK(fs, k);
  t->k = VINDEXED;
}


void luaK_prefix (FuncState *fs, UnOpr op, expdesc *e) {
  if (op == OPR_MINUS) {
    luaK_exp2val(fs, e);
    if (e->k == VNUMBER)
      e->u.n = -e->u.n;
    else {
      luaK_exp2anyreg(fs, e);
      freeexp(fs, e);
      e->u.i.info = luaK_codeABC(fs, OP_UNM, 0, e->u.i.info, 0);
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
      e1->k = e2->k; e1->u = e2->u; e1->t = e2->t;
      break;
    }
    case OPR_OR: {
      lua_assert(e1->f == NO_JUMP);  /* list must be closed */
      luaK_dischargevars(fs, e2);
      luaK_concat(fs, &e1->t, e2->t);
      e1->k = e2->k; e1->u = e2->u; e1->f = e2->f;
      break;
    }
    case OPR_CONCAT: {
      luaK_exp2val(fs, e2);
      if (e2->k == VRELOCABLE && GET_OPCODE(getcode(fs, e2)) == OP_CONCAT) {
        lua_assert(e1->u.i.info == GETARG_B(getcode(fs, e2))-1);
        freeexp(fs, e1);
        SETARG_B(getcode(fs, e2), e1->u.i.info);
        e1->k = e2->k; e1->u.i.info = e2->u.i.info;
      }
      else {
        luaK_exp2nextreg(fs, e2);
        freeexp(fs, e2);
        freeexp(fs, e1);
        e1->u.i.info = luaK_codeABC(fs, codes[op], 0, e1->u.i.info,
                                                      e2->u.i.info);
        e1->k = VRELOCABLE;
      }
      break;
    }
    case OPR_EQ: case OPR_NE: {
      luaK_exp2val(fs, e2);
      if (e2->k == VNIL) {  /* exp x= nil ? */
        if (e1->k == VK) {  /* constant x= nil ? */
          if (op == OPR_EQ)  /* constant == nil ? */
            e1->k = VNIL;  /* always false */
          /* else always true (leave the constant itself) */
        }
        else {
          OpCode opc = (op == OPR_EQ) ? OP_TESTF : OP_TESTT;
          e1->u.i.info = jumponcond(fs, e1, opc);
          e1->k = VJMP;
        }
        break;
      }
      /* else go through */
    }
    default: {
      int o1, o2;
      OpCode opc;
      if (e1->k != VK) {  /* not a constant operator? */
        o1 = e1->u.i.info;
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
        e1->u.i.info = luaK_codeABC(fs, opc, 0, o1, o2);
        e1->k = VRELOCABLE;
      }
      else {  /* jump */
        e1->u.i.info = luaK_condjump(fs, opc, o1, 0, o2);
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
                      MAX_INT, l_s("line info overflow"));
      f->lineinfo[fs->nlineinfo++] = -(ls->lastline - (fs->lastline+1));
    }
    luaM_growvector(fs->L, f->lineinfo, fs->nlineinfo, f->sizelineinfo, int,
                    MAX_INT, l_s("line info overflow"));
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
                  MAX_INT, l_s("code size overflow"));
  f->code[fs->pc] = i;
/*printf("free: %d  ", fs->freereg); printopcode(f, fs->pc);*/
  return fs->pc++;
}


int luaK_codeABC (FuncState *fs, OpCode o, int a, int b, int c) {
  lua_assert(getOpMode(o) == iABC);
  return luaK_code(fs, CREATE_ABC(o, a, b, c));
}


int luaK_codeABc (FuncState *fs, OpCode o, int a, int bc) {
  lua_assert(getOpMode(o) == iABc || getOpMode(o) == iAsBc);
  return luaK_code(fs, CREATE_ABc(o, a, bc));
}

