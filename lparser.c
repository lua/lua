/*
** $Id: lparser.c,v 1.160 2001/10/25 19:14:14 roberto Exp $
** Lua Parser
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#include "lua.h"

#include "lcode.h"
#include "ldebug.h"
#include "lfunc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"


/*
** Constructors descriptor:
** `n' indicates number of elements, and `k' signals whether
** it is a list constructor (k = 0) or a record constructor (k = 1)
** or empty (k = `;' or `}')
*/
typedef struct Constdesc {
  int narray;
  int nhash;
  int k;
} Constdesc;


/*
** nodes for break list (list of active breakable loops)
*/
typedef struct Breaklabel {
  struct Breaklabel *previous;  /* chain */
  int breaklist;  /* list of jumps out of this loop */
  int nactloc;  /* # of active local variables outside the breakable structure */
} Breaklabel;




/*
** prototypes for recursive non-terminal functions
*/
static void body (LexState *ls, expdesc *v, int needself, int line);
static void chunk (LexState *ls);
static void constructor (LexState *ls, expdesc *v);
static void expr (LexState *ls, expdesc *v);



static void next (LexState *ls) {
  ls->lastline = ls->linenumber;
  if (ls->lookahead.token != TK_EOS) {  /* is there a look-ahead token? */
    ls->t = ls->lookahead;  /* use this one */
    ls->lookahead.token = TK_EOS;  /* and discharge it */
  }
  else
    ls->t.token = luaX_lex(ls, &ls->t.seminfo);  /* read next token */
}


static void lookahead (LexState *ls) {
  lua_assert(ls->lookahead.token == TK_EOS);
  ls->lookahead.token = luaX_lex(ls, &ls->lookahead.seminfo);
}


static void error_expected (LexState *ls, int token) {
  char buff[30], t[TOKEN_LEN];
  luaX_token2str(token, t);
  sprintf(buff, "`%.10s' expected", t);
  luaK_error(ls, buff);
}


static void check (LexState *ls, int c) {
  if (ls->t.token != c)
    error_expected(ls, c);
  next(ls);
}


static void check_condition (LexState *ls, int c, const char *msg) {
  if (!c) luaK_error(ls, msg);
}


static int optional (LexState *ls, int c) {
  if (ls->t.token == c) {
    next(ls);
    return 1;
  }
  else return 0;
}


static void check_match (LexState *ls, int what, int who, int where) {
  if (ls->t.token != what) {
    if (where == ls->linenumber)
      error_expected(ls, what);
    else {
      char buff[70];
      char t_what[TOKEN_LEN], t_who[TOKEN_LEN];
      luaX_token2str(what, t_what);
      luaX_token2str(who, t_who);
      sprintf(buff, "`%.10s' expected (to close `%.10s' at line %d)",
              t_what, t_who, where);
      luaK_error(ls, buff);
    }
  }
  next(ls);
}


static TString *str_checkname (LexState *ls) {
  check_condition(ls, (ls->t.token == TK_NAME), "<name> expected");
  return ls->t.seminfo.ts;
}


static void init_exp (expdesc *e, expkind k, int i) {
  e->f = e->t = NO_JUMP;
  e->k = k;
  e->u.i.info = i;
}


static void codestring (LexState *ls, expdesc *e, TString *s) {
  init_exp(e, VK, luaK_stringk(ls->fs, s));
}


static void checkname(LexState *ls, expdesc *e) {
  codestring(ls, e, str_checkname(ls));
  next(ls);
}


static int luaI_registerlocalvar (LexState *ls, TString *varname) {
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  luaM_growvector(ls->L, f->locvars, fs->nlocvars, f->sizelocvars,
                  LocVar, MAX_INT, "");
  f->locvars[fs->nlocvars].varname = varname;
  return fs->nlocvars++;
}


static void new_localvar (LexState *ls, TString *name, int n) {
  FuncState *fs = ls->fs;
  luaX_checklimit(ls, fs->nactloc+n+1, MAXLOCALS, "local variables");
  fs->actloc[fs->nactloc+n] = luaI_registerlocalvar(ls, name);
}


static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  while (nvars--) {
    fs->f->locvars[fs->actloc[fs->nactloc]].startpc = fs->pc;
    resetbit(fs->wasup, fs->nactloc);
    fs->nactloc++;
  }
}


static void closelevel (LexState *ls, int level) {
  FuncState *fs = ls->fs;
  int i;
  for (i=level; i<fs->nactloc; i++)
    if (testbit(fs->wasup, i)) {
      luaK_codeABC(fs, OP_CLOSE, level, 0, 0);
      return;
    }
  return;  /* nothing to close */
}


static void removelocalvars (LexState *ls, int nvars, int toclose) {
  FuncState *fs = ls->fs;
  if (toclose)
    closelevel(ls, fs->nactloc - nvars);
  while (nvars--)
    fs->f->locvars[fs->actloc[--fs->nactloc]].endpc = fs->pc;
}


static void new_localvarstr (LexState *ls, const char *name, int n) {
  new_localvar(ls, luaS_new(ls->L, name), n);
}


static int indexupvalue (FuncState *fs, expdesc *v) {
  int i;
  for (i=0; i<fs->f->nupvalues; i++) {
    if (fs->upvalues[i].k == v->k && fs->upvalues[i].u.i.info == v->u.i.info)
      return i;
  }
  /* new one */
  luaX_checklimit(fs->ls, fs->f->nupvalues+1, MAXUPVALUES, "upvalues");
  fs->upvalues[fs->f->nupvalues] = *v;
  return fs->f->nupvalues++;
}


static void singlevar (FuncState *fs, TString *n, expdesc *var, int baselevel) {
  if (fs == NULL)
    init_exp(var, VGLOBAL, 0);  /* not local in any level; global variable */
  else {  /* look up at current level */
    int i;
    for (i=fs->nactloc-1; i >= 0; i--) {
      if (n == fs->f->locvars[fs->actloc[i]].varname) {
        if (!baselevel)
          setbit(fs->wasup, i);  /* will be upvalue in some other level */
        init_exp(var, VLOCAL, i);
        return;
      }
    }
    /* not found at current level; try upper one */
    singlevar(fs->prev, n, var, 0);
    if (var->k == VGLOBAL) {
      if (baselevel)
        var->u.i.info = luaK_stringk(fs, n);  /* info points to global name */
    }
    else {  /* local variable in some upper level? */
      var->u.i.info = indexupvalue(fs, var);
      var->k = VUPVAL;  /* upvalue in this level */
    }
  }
}



static void adjust_assign (LexState *ls, int nvars, int nexps, expdesc *e) {
  FuncState *fs = ls->fs;
  int extra = nvars - nexps;
  if (e->k == VCALL) {
    extra++;  /* includes call itself */
    if (extra <= 0) extra = 0;
    else luaK_reserveregs(fs, extra-1);
    luaK_setcallreturns(fs, e, extra);  /* call provides the difference */
  }
  else {
    if (e->k != VVOID) luaK_exp2nextreg(fs, e);  /* close last expression */
    if (extra > 0) {
      int reg = fs->freereg; 
      luaK_reserveregs(fs, extra);
      luaK_nil(fs, reg, extra);
    }
  }
}


static void code_params (LexState *ls, int nparams, short dots) {
  FuncState *fs = ls->fs;
  adjustlocalvars(ls, nparams);
  luaX_checklimit(ls, fs->nactloc, MAXPARAMS, "parameters");
  fs->f->numparams = cast(short, fs->nactloc);  /* `self' could be there already */
  fs->f->is_vararg = dots;
  if (dots) {
    new_localvarstr(ls, "arg", 0);
    adjustlocalvars(ls, 1);
  }
  luaK_reserveregs(fs, fs->nactloc);  /* reserve register for parameters */
}


static void enterbreak (FuncState *fs, Breaklabel *bl) {
  bl->breaklist = NO_JUMP;
  bl->nactloc = fs->nactloc;
  bl->previous = fs->bl;
  fs->bl = bl;
}


static void leavebreak (FuncState *fs, Breaklabel *bl) {
  fs->bl = bl->previous;
  luaK_patchlist(fs, bl->breaklist, luaK_getlabel(fs));
  lua_assert(bl->nactloc == fs->nactloc);
}


static void pushclosure (LexState *ls, FuncState *func, expdesc *v) {
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int i;
  luaM_growvector(ls->L, f->p, fs->np, f->sizep, Proto *,
                  MAXARG_Bc, "constant table overflow");
  f->p[fs->np++] = func->f;
  init_exp(v, VRELOCABLE, luaK_codeABc(fs, OP_CLOSURE, 0, fs->np-1));
  for (i=0; i<func->f->nupvalues; i++) {
    luaK_exp2nextreg(fs, &func->upvalues[i]);
    fs->freereg--;  /* CLOSURE will use these values */
  }
}


static void open_func (LexState *ls, FuncState *fs) {
  Proto *f = luaF_newproto(ls->L);
  fs->f = f;
  fs->prev = ls->fs;  /* linked list of funcstates */
  fs->ls = ls;
  fs->L = ls->L;
  ls->fs = fs;
  fs->pc = 0;
  fs->lasttarget = 0;
  fs->jlt = NO_JUMP;
  fs->freereg = 0;
  fs->nk = 0;
  fs->h = luaH_new(ls->L, 0, 0);
  fs->np = 0;
  fs->nlineinfo = 0;
  fs->nlocvars = 0;
  fs->nactloc = 0;
  fs->lastline = 0;
  fs->bl = NULL;
  f->code = NULL;
  f->source = ls->source;
  f->maxstacksize = 1;  /* register 0 is always valid */
  f->numparams = 0;  /* default for main chunk */
  f->is_vararg = 0;  /* default for main chunk */
}


static void close_func (LexState *ls) {
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  removelocalvars(ls, fs->nactloc, 0);
  luaK_codeABC(fs, OP_RETURN, 0, 0, 0);  /* final return */
  luaK_getlabel(fs);  /* close eventual list of pending jumps */
  lua_assert(G(L)->roottable == fs->h);
  G(L)->roottable = fs->h->next;
  luaH_free(L, fs->h);
  luaM_reallocvector(L, f->code, f->sizecode, fs->pc, Instruction);
  f->sizecode = fs->pc;
  luaM_reallocvector(L, f->k, f->sizek, fs->nk, TObject);
  f->sizek = fs->nk;
  luaM_reallocvector(L, f->p, f->sizep, fs->np, Proto *);
  f->sizep = fs->np;
  luaM_reallocvector(L, f->locvars, f->sizelocvars, fs->nlocvars, LocVar);
  f->sizelocvars = fs->nlocvars;
  luaM_reallocvector(L, f->lineinfo, f->sizelineinfo, fs->nlineinfo+1, int);
  f->lineinfo[fs->nlineinfo++] = MAX_INT;  /* end flag */
  f->sizelineinfo = fs->nlineinfo;
  lua_assert(luaG_checkcode(f));
  lua_assert(fs->bl == NULL);
  ls->fs = fs->prev;
}


Proto *luaY_parser (lua_State *L, ZIO *z) {
  struct LexState lexstate;
  struct FuncState funcstate;
  luaX_setinput(L, &lexstate, z, luaS_new(L, zname(z)));
  open_func(&lexstate, &funcstate);
  next(&lexstate);  /* read first token */
  chunk(&lexstate);
  check_condition(&lexstate, (lexstate.t.token == TK_EOS),
                             "<eof> expected");
  close_func(&lexstate);
  lua_assert(funcstate.prev == NULL);
  lua_assert(funcstate.f->nupvalues == 0);
  return funcstate.f;
}



/*============================================================*/
/* GRAMMAR RULES */
/*============================================================*/


static void luaY_field (LexState *ls, expdesc *v) {
  /* field -> ['.' | ':'] NAME */
  FuncState *fs = ls->fs;
  expdesc key;
  luaK_exp2anyreg(fs, v);
  next(ls);  /* skip the dot or colon */
  checkname(ls, &key);
  luaK_indexed(fs, v, &key);
}


static void luaY_index (LexState *ls, expdesc *v) {
  /* index -> '[' expr ']' */
  next(ls);  /* skip the '[' */
  expr(ls, v);
  luaK_exp2val(ls->fs, v);
  check(ls, ']');
}


static int explist1 (LexState *ls, expdesc *v) {
  /* explist1 -> expr { `,' expr } */
  int n = 1;  /* at least one expression */
  expr(ls, v);
  while (ls->t.token == ',') {
    next(ls);  /* skip comma */
    luaK_exp2nextreg(ls->fs, v);
    expr(ls, v);
    n++;
  }
  return n;
}


static void funcargs (LexState *ls, expdesc *f) {
  FuncState *fs = ls->fs;
  expdesc args;
  int base, nparams;
  switch (ls->t.token) {
    case '(': {  /* funcargs -> `(' [ explist1 ] `)' */
      int line = ls->linenumber;
      if (line != ls->lastline)
        luaK_error(ls, "ambiguous syntax (function call x new statement)");
      next(ls);
      if (ls->t.token == ')')  /* arg list is empty? */
        args.k = VVOID;
      else {
        explist1(ls, &args);
        luaK_setcallreturns(fs, &args, LUA_MULTRET);
      }
      check_match(ls, ')', '(', line);
      break;
    }
    case '{': {  /* funcargs -> constructor */
      constructor(ls, &args);
      break;
    }
    case TK_STRING: {  /* funcargs -> STRING */
      codestring(ls, &args, ls->t.seminfo.ts);
      next(ls);  /* must use `seminfo' before `next' */
      break;
    }
    default: {
      luaK_error(ls, "function arguments expected");
      break;
    }
  }
  lua_assert(f->k == VNONRELOC);
  base = f->u.i.info;  /* base register for call */
  if (args.k == VCALL)
    nparams = NO_REG;  /* open call */
  else {
    if (args.k != VVOID)
      luaK_exp2nextreg(fs, &args);  /* close last argument */
    nparams = fs->freereg - (base+1);
  }
  init_exp(f, VCALL, luaK_codeABC(fs, OP_CALL, base, nparams, 1));
  fs->freereg = base+1;  /* call remove function and arguments and leaves
                            (unless changed) one result */
}



/*
** {======================================================================
** Rules for Constructors
** =======================================================================
*/


static void recfield (LexState *ls, expdesc *t) {
  /* recfield -> (NAME | `['exp1`]') = exp1 */
  FuncState *fs = ls->fs;
  int reg = ls->fs->freereg;
  expdesc key, val;
  switch (ls->t.token) {
    case TK_NAME: {
      checkname(ls, &key);
      break;
    }
    case '[': {
      luaY_index(ls, &key);
      break;
    }
    default: luaK_error(ls, "<name> or `[' expected");
  }
  check(ls, '=');
  luaK_exp2RK(fs, &key);
  expr(ls, &val);
  luaK_exp2anyreg(fs, &val);
  luaK_codeABC(fs, OP_SETTABLE, val.u.i.info, t->u.i.info,
               luaK_exp2RK(fs, &key));
  fs->freereg = reg;  /* free registers */
}


static int anotherfield (LexState *ls) {
  if (ls->t.token != ',') return 0;
  next(ls);  /* skip the comma */
  return (ls->t.token != ';' && ls->t.token != '}');
}


static int recfields (LexState *ls, expdesc *t) {
  /* recfields -> recfield { `,' recfield } [`,'] */
  int n = 0;
  do {  /* at least one element */
    recfield(ls, t);
    luaX_checklimit(ls, n, MAX_INT, "items in a constructor");
    n++;
  } while (anotherfield(ls));
  return n;
}


static int listfields (LexState *ls, expdesc *t) {
  /* listfields -> exp1 { `,' exp1 } [`,'] */
  expdesc v;
  FuncState *fs = ls->fs;
  int n = 1;  /* at least one element */
  int reg;
  reg = fs->freereg;
  expr(ls, &v);
  while (anotherfield(ls)) {
    luaK_exp2nextreg(fs, &v);
    luaX_checklimit(ls, n, MAXARG_Bc, "items in a constructor");
    if (n%LFIELDS_PER_FLUSH == 0) {
      luaK_codeABc(fs, OP_SETLIST, t->u.i.info, n-1);  /* flush */
      fs->freereg = reg;  /* free registers */
    }
    expr(ls, &v);
    n++;
  }
  if (v.k == VCALL) {
    luaK_setcallreturns(fs, &v, LUA_MULTRET);
    luaK_codeABc(fs, OP_SETLISTO, t->u.i.info, n-1);
  }
  else {
    luaK_exp2nextreg(fs, &v);
    luaK_codeABc(fs, OP_SETLIST, t->u.i.info, n-1);
  }
  fs->freereg = reg;  /* free registers */
  return n;
}


static void constructor_part (LexState *ls, expdesc *t, Constdesc *cd) {
  switch (ls->t.token) {
    case ';': case '}': {  /* constructor_part -> empty */
      cd->narray = cd->nhash = 0;
      cd->k = ls->t.token;
      break;
    }
    case TK_NAME: {  /* may be listfields or recfields */
      lookahead(ls);
      if (ls->lookahead.token != '=')  /* expression? */
        goto case_default;
      /* else go through to recfields */
    }
    case '[': {  /* constructor_part -> recfields */
      cd->nhash = recfields(ls, t);
      cd->narray = 0;
      cd->k = 1;  /* record */
      break;
    }
    default: {  /* constructor_part -> listfields */
    case_default:
      cd->narray = listfields(ls, t);
      cd->nhash = 0;
      cd->k = 0;  /* list */
      break;
    }
  }
}


static void constructor (LexState *ls, expdesc *t) {
  /* constructor -> `{' constructor_part [`;' constructor_part] `}' */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int na, nh;
  int pc;
  Constdesc cd;
  pc = luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  init_exp(t, VRELOCABLE, pc);
  luaK_exp2nextreg(ls->fs, t);  /* fix it at stack top (for gc) */
  check(ls, '{');
  constructor_part(ls, t, &cd);
  na = cd.narray;
  nh = cd.nhash;
  if (optional(ls, ';')) {
    Constdesc other_cd;
    constructor_part(ls, t, &other_cd);
    check_condition(ls,(cd.k != other_cd.k), "invalid constructor syntax");
    na += other_cd.narray;
    nh += other_cd.nhash;
  }
  check_match(ls, '}', '{', line);
  if (na > 0)
    SETARG_B(fs->f->code[pc], luaO_log2(na-1)+2);  /* set initial table size */
  SETARG_C(fs->f->code[pc], luaO_log2(nh)+1);  /* set initial table size */
}

/* }====================================================================== */




/*
** {======================================================================
** Expression parsing
** =======================================================================
*/


static void prefixexp (LexState *ls, expdesc *v) {
  /* prefixexp -> NAME | '(' expr ')' */
  switch (ls->t.token) {
    case '(': {
      next(ls);
      expr(ls, v);
      check(ls, ')');
      luaK_dischargevars(ls->fs, v);
      return;
    }
    case TK_NAME: {
      singlevar(ls->fs, str_checkname(ls), v, 1);
      next(ls);
      return;
    }
    case '%': {  /* for compatibility only */
      next(ls);  /* skip `%' */
      singlevar(ls->fs, str_checkname(ls), v, 1);
      check_condition(ls, v->k == VUPVAL, "global upvalues are obsolete");
      next(ls);
      return;
    }
    default: {
      luaK_error(ls, "unexpected symbol");
      return;
    }
  }
}


static void primaryexp (LexState *ls, expdesc *v) {
  /* primaryexp ->
        prefixexp { `.' NAME | `[' exp `]' | `:' NAME funcargs | funcargs } */
  FuncState *fs = ls->fs;
  prefixexp(ls, v);
  for (;;) {
    switch (ls->t.token) {
      case '.': {  /* field */
        luaY_field(ls, v);
        break;
      }
      case '[': {  /* `[' exp1 `]' */
        expdesc key;
        luaK_exp2anyreg(fs, v);
        luaY_index(ls, &key);
        luaK_indexed(fs, v, &key);
        break;
      }
      case ':': {  /* `:' NAME funcargs */
        expdesc key;
        next(ls);
        checkname(ls, &key);
        luaK_self(fs, v, &key);
        funcargs(ls, v);
        break;
      }
      case '(': case TK_STRING: case '{': {  /* funcargs */
        luaK_exp2nextreg(fs, v);
        funcargs(ls, v);
        break;
      }
      default: return;
    }
  }
}


static void simpleexp (LexState *ls, expdesc *v) {
  /* simpleexp -> NUMBER | STRING | NIL | constructor | FUNCTION body
               | primaryexp */
  switch (ls->t.token) {
    case TK_NUMBER: {
      init_exp(v, VNUMBER, 0);
      v->u.n = ls->t.seminfo.r;
      next(ls);  /* must use `seminfo' before `next' */
      break;
    }
    case TK_STRING: {
      codestring(ls, v, ls->t.seminfo.ts);
      next(ls);  /* must use `seminfo' before `next' */
      break;
    }
    case TK_NIL: {
      init_exp(v, VNIL, 0);
      next(ls);
      break;
    }
    case '{': {  /* constructor */
      constructor(ls, v);
      break;
    }
    case TK_FUNCTION: {
      next(ls);
      body(ls, v, 0, ls->linenumber);
      break;
    }
    default: {
      primaryexp(ls, v);
      break;
    }
  }
}




static UnOpr getunopr (int op) {
  switch (op) {
    case TK_NOT: return OPR_NOT;
    case '-': return OPR_MINUS;
    default: return OPR_NOUNOPR;
  }
}


static BinOpr getbinopr (int op) {
  switch (op) {
    case '+': return OPR_ADD;
    case '-': return OPR_SUB;
    case '*': return OPR_MULT;
    case '/': return OPR_DIV;
    case '^': return OPR_POW;
    case TK_CONCAT: return OPR_CONCAT;
    case TK_NE: return OPR_NE;
    case TK_EQ: return OPR_EQ;
    case '<': return OPR_LT;
    case TK_LE: return OPR_LE;
    case '>': return OPR_GT;
    case TK_GE: return OPR_GE;
    case TK_AND: return OPR_AND;
    case TK_OR: return OPR_OR;
    default: return OPR_NOBINOPR;
  }
}


static const struct {
  lu_byte left;  /* left priority for each binary operator */
  lu_byte right; /* right priority */
} priority[] = {  /* ORDER OPR */
   {5, 5}, {5, 5}, {6, 6}, {6, 6},  /* arithmetic */
   {9, 8}, {4, 3},                  /* power and concat (right associative) */
   {2, 2}, {2, 2},                  /* equality */
   {2, 2}, {2, 2}, {2, 2}, {2, 2},  /* order */
   {1, 1}, {1, 1}                   /* logical */
};

#define UNARY_PRIORITY	7  /* priority for unary operators */


/*
** subexpr -> (simplexep | unop subexpr) { binop subexpr }
** where `binop' is any binary operator with a priority higher than `limit'
*/
static BinOpr subexpr (LexState *ls, expdesc *v, int limit) {
  BinOpr op;
  UnOpr uop = getunopr(ls->t.token);
  if (uop != OPR_NOUNOPR) {
    next(ls);
    subexpr(ls, v, UNARY_PRIORITY);
    luaK_prefix(ls->fs, uop, v);
  }
  else simpleexp(ls, v);
  /* expand while operators have priorities higher than `limit' */
  op = getbinopr(ls->t.token);
  while (op != OPR_NOBINOPR && cast(int, priority[op].left) > limit) {
    expdesc v2;
    BinOpr nextop;
    next(ls);
    luaK_infix(ls->fs, op, v);
    /* read sub-expression with higher priority */
    nextop = subexpr(ls, &v2, cast(int, priority[op].right));
    luaK_posfix(ls->fs, op, v, &v2);
    op = nextop;
  }
  return op;  /* return first untreated operator */
}


static void expr (LexState *ls, expdesc *v) {
  subexpr(ls, v, -1);
}

/* }==================================================================== */


/*
** {======================================================================
** Rules for Statements
** =======================================================================
*/


static int block_follow (int token) {
  switch (token) {
    case TK_ELSE: case TK_ELSEIF: case TK_END:
    case TK_UNTIL: case TK_EOS:
      return 1;
    default: return 0;
  }
}


static void block (LexState *ls) {
  /* block -> chunk */
  FuncState *fs = ls->fs;
  int nactloc = fs->nactloc;
  chunk(ls);
  removelocalvars(ls, fs->nactloc - nactloc, 1);
  fs->freereg = nactloc;  /* free registers used by locals */
}


/*
** structure to chain all variables in the left-hand side of an
** assignment
*/
struct LHS_assign {
  struct LHS_assign *prev;
  expdesc v;  /* variable (global, local, upvalue, or indexed) */
};


/*
** check whether, in an assignment to a local variable, the local variable
** is needed in a previous assignment (to a table). If so, save original
** local value in a safe place and use this safe copy in the previous
** assignment.
*/
static void check_conflict (LexState *ls, struct LHS_assign *lh, expdesc *v) {
  FuncState *fs = ls->fs;
  int extra = fs->freereg;  /* eventual position to save local variable */
  int conflict = 0;
  for (; lh; lh = lh->prev) {
    if (lh->v.k == VINDEXED) {
      if (lh->v.u.i.info == v->u.i.info) {  /* conflict? */
        conflict = 1;
        lh->v.u.i.info = extra;  /* previous assignment will use safe copy */
      }
      if (lh->v.u.i.aux == v->u.i.info) {  /* conflict? */
        conflict = 1;
        lh->v.u.i.aux = extra;  /* previous assignment will use safe copy */
      }
    }
  }
  if (conflict) {
    luaK_codeABC(fs, OP_MOVE, fs->freereg, v->u.i.info, 0);  /* make copy */
    luaK_reserveregs(fs, 1);
  }
}


static void assignment (LexState *ls, struct LHS_assign *lh, int nvars) {
  expdesc e;
  check_condition(ls, VLOCAL <= lh->v.k && lh->v.k <= VINDEXED,
                      "syntax error!");
  if (ls->t.token == ',') {  /* assignment -> `,' primaryexp assignment */
    struct LHS_assign nv;
    nv.prev = lh;
    next(ls);
    primaryexp(ls, &nv.v);
    if (nv.v.k == VLOCAL)
      check_conflict(ls, lh, &nv.v);
    assignment(ls, &nv, nvars+1);
  }
  else {  /* assignment -> `=' explist1 */
    int nexps;
    check(ls, '=');
    nexps = explist1(ls, &e);
    if (nexps != nvars) {
      adjust_assign(ls, nvars, nexps, &e);
      if (nexps > nvars)
        ls->fs->freereg -= nexps - nvars;  /* remove extra values */
    }
    else {
      luaK_storevar(ls->fs, &lh->v, &e);
      return;  /* avoid default */
    }
  }
  init_exp(&e, VNONRELOC, ls->fs->freereg-1);  /* default assignment */
  luaK_storevar(ls->fs, &lh->v, &e);
}


static void cond (LexState *ls, expdesc *v) {
  /* cond -> exp */
  expr(ls, v);  /* read condition */
  luaK_goiftrue(ls->fs, v);
}


static void whilestat (LexState *ls, int line) {
  /* whilestat -> WHILE cond DO block END */
  FuncState *fs = ls->fs;
  int while_init = luaK_getlabel(fs);
  expdesc v;
  Breaklabel bl;
  enterbreak(fs, &bl);
  next(ls);
  cond(ls, &v);
  check(ls, TK_DO);
  block(ls);
  luaK_patchlist(fs, luaK_jump(fs), while_init);
  luaK_patchlist(fs, v.f, luaK_getlabel(fs));
  check_match(ls, TK_END, TK_WHILE, line);
  leavebreak(fs, &bl);
}


static void repeatstat (LexState *ls, int line) {
  /* repeatstat -> REPEAT block UNTIL cond */
  FuncState *fs = ls->fs;
  int repeat_init = luaK_getlabel(fs);
  expdesc v;
  Breaklabel bl;
  enterbreak(fs, &bl);
  next(ls);
  block(ls);
  check_match(ls, TK_UNTIL, TK_REPEAT, line);
  cond(ls, &v);
  luaK_patchlist(fs, v.f, repeat_init);
  leavebreak(fs, &bl);
}


static void exp1 (LexState *ls) {
  expdesc e;
  expr(ls, &e);
  luaK_exp2nextreg(ls->fs, &e);
}


static void forbody (LexState *ls, int nvar, OpCode prepfor, OpCode loopfor) {
  /* forbody -> DO block END */
  FuncState *fs = ls->fs;
  int basereg = fs->freereg - nvar;
  int prep = luaK_codeAsBc(fs, prepfor, basereg, NO_JUMP);
  int blockinit = luaK_getlabel(fs);
  check(ls, TK_DO);
  adjustlocalvars(ls, nvar);  /* scope for control variables */
  block(ls);
  luaK_patchlist(fs, luaK_codeAsBc(fs, loopfor, basereg, NO_JUMP), blockinit);
  luaK_fixfor(fs, prep, luaK_getlabel(fs));
  removelocalvars(ls, nvar, 1);
}


static void fornum (LexState *ls, TString *varname) {
  /* fornum -> NAME = exp1,exp1[,exp1] forbody */
  FuncState *fs = ls->fs;
  check(ls, '=');
  exp1(ls);  /* initial value */
  check(ls, ',');
  exp1(ls);  /* limit */
  if (optional(ls, ','))
    exp1(ls);  /* optional step */
  else {
    luaK_codeAsBc(fs, OP_LOADINT, fs->freereg, 1);  /* default step */
    luaK_reserveregs(fs, 1);
  }
  new_localvar(ls, varname, 0);
  new_localvarstr(ls, "(limit)", 1);
  new_localvarstr(ls, "(step)", 2);
  forbody(ls, 3, OP_FORPREP, OP_FORLOOP);
}


static void forlist (LexState *ls, TString *indexname) {
  /* forlist -> NAME,NAME IN exp1 forbody */
  TString *valname;
  check(ls, ',');
  valname = str_checkname(ls);
  next(ls);  /* skip var name */
  check(ls, TK_IN);
  exp1(ls);  /* table */
  new_localvarstr(ls, "(table)", 0);
  new_localvarstr(ls, "(index)", 1);
  new_localvar(ls, indexname, 2);
  new_localvar(ls, valname, 3);
  luaK_reserveregs(ls->fs, 3);  /* registers for control, index and val */
  forbody(ls, 4, OP_TFORPREP, OP_TFORLOOP);
}


static void forstat (LexState *ls, int line) {
  /* forstat -> fornum | forlist */
  FuncState *fs = ls->fs;
  TString *varname;
  Breaklabel bl;
  enterbreak(fs, &bl);
  next(ls);  /* skip `for' */
  varname = str_checkname(ls);  /* first variable name */
  next(ls);  /* skip var name */
  switch (ls->t.token) {
    case '=': fornum(ls, varname); break;
    case ',': forlist(ls, varname); break;
    default: luaK_error(ls, "`=' or `,' expected");
  }
  check_match(ls, TK_END, TK_FOR, line);
  leavebreak(fs, &bl);
}


static void test_then_block (LexState *ls, expdesc *v) {
  /* test_then_block -> [IF | ELSEIF] cond THEN block */
  next(ls);  /* skip IF or ELSEIF */
  cond(ls, v);
  check(ls, TK_THEN);
  block(ls);  /* `then' part */
}


static void ifstat (LexState *ls, int line) {
  /* ifstat -> IF cond THEN block {ELSEIF cond THEN block} [ELSE block] END */
  FuncState *fs = ls->fs;
  expdesc v;
  int escapelist = NO_JUMP;
  test_then_block(ls, &v);  /* IF cond THEN block */
  while (ls->t.token == TK_ELSEIF) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));
    luaK_patchlist(fs, v.f, luaK_getlabel(fs));
    test_then_block(ls, &v);  /* ELSEIF cond THEN block */
  }
  if (ls->t.token == TK_ELSE) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));
    luaK_patchlist(fs, v.f, luaK_getlabel(fs));
    next(ls);  /* skip ELSE */
    block(ls);  /* `else' part */
  }
  else
    luaK_concat(fs, &escapelist, v.f);
  luaK_patchlist(fs, escapelist, luaK_getlabel(fs));
  check_match(ls, TK_END, TK_IF, line);
}


static void localstat (LexState *ls) {
  /* stat -> LOCAL NAME {`,' NAME} [`=' explist1] */
  int nvars = 0;
  int nexps;
  expdesc e;
  do {
    next(ls);  /* skip LOCAL or `,' */
    new_localvar(ls, str_checkname(ls), nvars++);
    next(ls);  /* skip var name */
  } while (ls->t.token == ',');
  if (optional(ls, '='))
    nexps = explist1(ls, &e);
  else {
    e.k = VVOID;
    nexps = 0;
  }
  adjust_assign(ls, nvars, nexps, &e);
  adjustlocalvars(ls, nvars);
}


static int funcname (LexState *ls, expdesc *v) {
  /* funcname -> NAME {field} [`:' NAME] */
  int needself = 0;
  singlevar(ls->fs, str_checkname(ls), v, 1);
  next(ls);  /* skip var name */
  while (ls->t.token == '.') {
    luaY_field(ls, v);
  }
  if (ls->t.token == ':') {
    needself = 1;
    luaY_field(ls, v);
  }
  return needself;
}


static void funcstat (LexState *ls, int line) {
  /* funcstat -> FUNCTION funcname body */
  int needself;
  expdesc v, b;
  next(ls);  /* skip FUNCTION */
  needself = funcname(ls, &v);
  body(ls, &b, needself, line);
  luaK_storevar(ls->fs, &v, &b);
}


static void exprstat (LexState *ls) {
  /* stat -> func | assignment */
  FuncState *fs = ls->fs;
  struct LHS_assign v;
  primaryexp(ls, &v.v);
  if (v.v.k == VCALL) {  /* stat -> func */
    luaK_setcallreturns(fs, &v.v, 0);  /* call statement uses no results */
  }
  else {  /* stat -> assignment */
    v.prev = NULL;
    assignment(ls, &v, 1);
  }
}


static void retstat (LexState *ls) {
  /* stat -> RETURN explist */
  FuncState *fs = ls->fs;
  expdesc e;
  int first, nret;  /* registers with returned values */
  next(ls);  /* skip RETURN */
  if (block_follow(ls->t.token) || ls->t.token == ';')
    first = nret = 0;  /* return no values */
  else {
    explist1(ls, &e);  /* optional return values */
    if (e.k == VCALL) {
      luaK_setcallreturns(fs, &e, LUA_MULTRET);
      first = fs->nactloc;
      nret = NO_REG;  /* return all values */
    }
    else {
      luaK_exp2nextreg(fs, &e);  /* values must go to the `stack' */
      first = fs->nactloc;
      nret = fs->freereg - first;  /* return all `active' values */
    }
  }
  luaK_codeABC(fs, OP_RETURN, first, nret, 0);
  fs->freereg = fs->nactloc;  /* removes all temp values */
}


static void breakstat (LexState *ls) {
  /* stat -> BREAK [NAME] */
  FuncState *fs = ls->fs;
  Breaklabel *bl = fs->bl;
  if (!bl)
    luaK_error(ls, "no loop to break");
  next(ls);  /* skip BREAK */
  closelevel(ls, bl->nactloc);
  luaK_concat(fs, &bl->breaklist, luaK_jump(fs));
}


static int statement (LexState *ls) {
  int line = ls->linenumber;  /* may be needed for error messages */
  switch (ls->t.token) {
    case TK_IF: {  /* stat -> ifstat */
      ifstat(ls, line);
      return 0;
    }
    case TK_WHILE: {  /* stat -> whilestat */
      whilestat(ls, line);
      return 0;
    }
    case TK_DO: {  /* stat -> DO block END */
      next(ls);  /* skip DO */
      block(ls);
      check_match(ls, TK_END, TK_DO, line);
      return 0;
    }
    case TK_FOR: {  /* stat -> forstat */
      forstat(ls, line);
      return 0;
    }
    case TK_REPEAT: {  /* stat -> repeatstat */
      repeatstat(ls, line);
      return 0;
    }
    case TK_FUNCTION: {
      funcstat(ls, line);  /* stat -> funcstat */
      return 0;
    }
    case TK_LOCAL: {  /* stat -> localstat */
      localstat(ls);
      return 0;
    }
    case TK_RETURN: {  /* stat -> retstat */
      retstat(ls);
      return 1;  /* must be last statement */
    }
    case TK_BREAK: {  /* stat -> breakstat */
      breakstat(ls);
      return 1;  /* must be last statement */
    }
    default: {
      exprstat(ls);
      return 0;  /* to avoid warnings */
    }
  }
}


static void parlist (LexState *ls) {
  /* parlist -> [ param { `,' param } ] */
  int nparams = 0;
  short dots = 0;
  if (ls->t.token != ')') {  /* is `parlist' not empty? */
    do {
      switch (ls->t.token) {
        case TK_DOTS: dots = 1; break;
        case TK_NAME: new_localvar(ls, str_checkname(ls), nparams++); break;
        default: luaK_error(ls, "<name> or `...' expected");
      }
      next(ls);
    } while (!dots && optional(ls, ','));
  }
  code_params(ls, nparams, dots);
}


static void body (LexState *ls, expdesc *e, int needself, int line) {
  /* body ->  `(' parlist `)' chunk END */
  FuncState new_fs;
  open_func(ls, &new_fs);
  new_fs.f->lineDefined = line;
  check(ls, '(');
  if (needself) {
    new_localvarstr(ls, "self", 0);
    adjustlocalvars(ls, 1);
  }
  parlist(ls);
  check(ls, ')');
  chunk(ls);
  check_match(ls, TK_END, TK_FUNCTION, line);
  close_func(ls);
  pushclosure(ls, &new_fs, e);
}


/* }====================================================================== */


static void chunk (LexState *ls) {
  /* chunk -> { stat [`;'] } */
  int islast = 0;
  while (!islast && !block_follow(ls->t.token)) {
    islast = statement(ls);
    optional(ls, ';');
    lua_assert(ls->fs->freereg >= ls->fs->nactloc);
    ls->fs->freereg = ls->fs->nactloc;  /* free registers */
  }
}

