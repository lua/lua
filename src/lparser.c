/*
** $Id: lparser.c,v 1.116 2000/10/27 11:39:52 roberto Exp $
** LL(1) Parser and code generator for Lua
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#include "lua.h"

#include "lcode.h"
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
** or empty (k = ';' or '}')
*/
typedef struct Constdesc {
  int n;
  int k;
} Constdesc;


typedef struct Breaklabel {
  struct Breaklabel *previous;  /* chain */
  int breaklist;
  int stacklevel;
} Breaklabel;




/*
** prototypes for recursive non-terminal functions
*/
static void body (LexState *ls, int needself, int line);
static void chunk (LexState *ls);
static void constructor (LexState *ls);
static void expr (LexState *ls, expdesc *v);
static void exp1 (LexState *ls);



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
  LUA_ASSERT(ls->lookahead.token == TK_EOS, "two look-aheads");
  ls->lookahead.token = luaX_lex(ls, &ls->lookahead.seminfo);
}


static void error_expected (LexState *ls, int token) {
  char buff[100], t[TOKEN_LEN];
  luaX_token2str(token, t);
  sprintf(buff, "`%.20s' expected", t);
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
      char buff[100];
      char t_what[TOKEN_LEN], t_who[TOKEN_LEN];
      luaX_token2str(what, t_what);
      luaX_token2str(who, t_who);
      sprintf(buff, "`%.20s' expected (to close `%.20s' at line %d)",
              t_what, t_who, where);
      luaK_error(ls, buff);
    }
  }
  next(ls);
}


static int string_constant (FuncState *fs, TString *s) {
  Proto *f = fs->f;
  int c = s->u.s.constindex;
  if (c >= f->nkstr || f->kstr[c] != s) {
    luaM_growvector(fs->L, f->kstr, f->nkstr, 1, TString *,
                    "constant table overflow", MAXARG_U);
    c = f->nkstr++;
    f->kstr[c] = s;
    s->u.s.constindex = c;  /* hint for next time */
  }
  return c;
}


static void code_string (LexState *ls, TString *s) {
  luaK_kstr(ls, string_constant(ls->fs, s));
}


static TString *str_checkname (LexState *ls) {
  TString *ts;
  check_condition(ls, (ls->t.token == TK_NAME), "<name> expected");
  ts = ls->t.seminfo.ts;
  next(ls);
  return ts;
}


static int checkname (LexState *ls) {
  return string_constant(ls->fs, str_checkname(ls));
}


static int luaI_registerlocalvar (LexState *ls, TString *varname) {
  Proto *f = ls->fs->f;
  luaM_growvector(ls->L, f->locvars, f->nlocvars, 1, LocVar, "", MAX_INT);
  f->locvars[f->nlocvars].varname = varname;
  return f->nlocvars++;
}


static void new_localvar (LexState *ls, TString *name, int n) {
  FuncState *fs = ls->fs;
  luaX_checklimit(ls, fs->nactloc+n+1, MAXLOCALS, "local variables");
  fs->actloc[fs->nactloc+n] = luaI_registerlocalvar(ls, name);
}


static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  while (nvars--)
    fs->f->locvars[fs->actloc[fs->nactloc++]].startpc = fs->pc;
}


static void removelocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  while (nvars--)
    fs->f->locvars[fs->actloc[--fs->nactloc]].endpc = fs->pc;
}


static void new_localvarstr (LexState *ls, const char *name, int n) {
  new_localvar(ls, luaS_newfixed(ls->L, name), n);
}


static int search_local (LexState *ls, TString *n, expdesc *var) {
  FuncState *fs;
  int level = 0;
  for (fs=ls->fs; fs; fs=fs->prev) {
    int i;
    for (i=fs->nactloc-1; i >= 0; i--) {
      if (n == fs->f->locvars[fs->actloc[i]].varname) {
        var->k = VLOCAL;
        var->u.index = i;
        return level;
      }
    }
    level++;  /* `var' not found; check outer level */
  }
  var->k = VGLOBAL;  /* not found in any level; must be global */
  return -1;
}


static void singlevar (LexState *ls, TString *n, expdesc *var) {
  int level = search_local(ls, n, var);
  if (level >= 1)  /* neither local (0) nor global (-1)? */
    luaX_syntaxerror(ls, "cannot access a variable in outer scope", n->str);
  else if (level == -1)  /* global? */
    var->u.index = string_constant(ls->fs, n);
}


static int indexupvalue (LexState *ls, expdesc *v) {
  FuncState *fs = ls->fs;
  int i;
  for (i=0; i<fs->nupvalues; i++) {
    if (fs->upvalues[i].k == v->k && fs->upvalues[i].u.index == v->u.index)
      return i;
  }
  /* new one */
  luaX_checklimit(ls, fs->nupvalues+1, MAXUPVALUES, "upvalues");
  fs->upvalues[fs->nupvalues] = *v;
  return fs->nupvalues++;
}


static void pushupvalue (LexState *ls, TString *n) {
  FuncState *fs = ls->fs;
  expdesc v;
  int level = search_local(ls, n, &v);
  if (level == -1) {  /* global? */
    if (fs->prev == NULL)
      luaX_syntaxerror(ls, "cannot access upvalue in main", n->str);
    v.u.index = string_constant(fs->prev, n);
  }
  else if (level != 1)
    luaX_syntaxerror(ls,
         "upvalue must be global or local to immediately outer scope", n->str);
  luaK_code1(fs, OP_PUSHUPVALUE, indexupvalue(ls, &v));
}


static void adjust_mult_assign (LexState *ls, int nvars, int nexps) {
  FuncState *fs = ls->fs;
  int diff = nexps - nvars;
  if (nexps > 0 && luaK_lastisopen(fs)) { /* list ends in a function call */
    diff--;  /* do not count function call itself */
    if (diff <= 0) {  /* more variables than values? */
      luaK_setcallreturns(fs, -diff);  /* function call provide extra values */
      diff = 0;  /* no more difference */
    }
    else  /* more values than variables */
      luaK_setcallreturns(fs, 0);  /* call should provide no value */
  }
  /* push or pop eventual difference between list lengths */
  luaK_adjuststack(fs, diff);
}


static void code_params (LexState *ls, int nparams, int dots) {
  FuncState *fs = ls->fs;
  adjustlocalvars(ls, nparams);
  luaX_checklimit(ls, fs->nactloc, MAXPARAMS, "parameters");
  fs->f->numparams = fs->nactloc;  /* `self' could be there already */
  fs->f->is_vararg = dots;
  if (dots) {
    new_localvarstr(ls, "arg", 0);
    adjustlocalvars(ls, 1);
  }
  luaK_deltastack(fs, fs->nactloc);  /* count parameters in the stack */
}


static void enterbreak (FuncState *fs, Breaklabel *bl) {
  bl->stacklevel = fs->stacklevel;
  bl->breaklist = NO_JUMP;
  bl->previous = fs->bl;
  fs->bl = bl;
}


static void leavebreak (FuncState *fs, Breaklabel *bl) {
  fs->bl = bl->previous;
  LUA_ASSERT(bl->stacklevel == fs->stacklevel, "wrong levels");
  luaK_patchlist(fs, bl->breaklist, luaK_getlabel(fs));
}


static void pushclosure (LexState *ls, FuncState *func) {
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int i;
  for (i=0; i<func->nupvalues; i++)
    luaK_tostack(ls, &func->upvalues[i], 1);
  luaM_growvector(ls->L, f->kproto, f->nkproto, 1, Proto *,
                  "constant table overflow", MAXARG_A);
  f->kproto[f->nkproto++] = func->f;
  luaK_code2(fs, OP_CLOSURE, f->nkproto-1, func->nupvalues);
}


static void open_func (LexState *ls, FuncState *fs) {
  Proto *f = luaF_newproto(ls->L);
  fs->prev = ls->fs;  /* linked list of funcstates */
  fs->ls = ls;
  fs->L = ls->L;
  ls->fs = fs;
  fs->stacklevel = 0;
  fs->nactloc = 0;
  fs->nupvalues = 0;
  fs->bl = NULL;
  fs->f = f;
  f->source = ls->source;
  fs->pc = 0;
  fs->lasttarget = 0;
  fs->lastline = 0;
  fs->jlt = NO_JUMP;
  f->code = NULL;
  f->maxstacksize = 0;
  f->numparams = 0;  /* default for main chunk */
  f->is_vararg = 0;  /* default for main chunk */
}


static void close_func (LexState *ls) {
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  luaK_code0(fs, OP_END);
  luaK_getlabel(fs);  /* close eventual list of pending jumps */
  luaM_reallocvector(L, f->code, fs->pc, Instruction);
  luaM_reallocvector(L, f->kstr, f->nkstr, TString *);
  luaM_reallocvector(L, f->knum, f->nknum, Number);
  luaM_reallocvector(L, f->kproto, f->nkproto, Proto *);
  removelocalvars(ls, fs->nactloc);
  luaM_reallocvector(L, f->locvars, f->nlocvars, LocVar);
  luaM_reallocvector(L, f->lineinfo, f->nlineinfo+1, int);
  f->lineinfo[f->nlineinfo++] = MAX_INT;  /* end flag */
  luaF_protook(L, f, fs->pc);  /* proto is ok now */
  ls->fs = fs->prev;
  LUA_ASSERT(fs->bl == NULL, "wrong list end");
}


Proto *luaY_parser (lua_State *L, ZIO *z) {
  struct LexState lexstate;
  struct FuncState funcstate;
  luaX_setinput(L, &lexstate, z, luaS_new(L, zname(z)));
  open_func(&lexstate, &funcstate);
  next(&lexstate);  /* read first token */
  chunk(&lexstate);
  check_condition(&lexstate, (lexstate.t.token == TK_EOS), "<eof> expected");
  close_func(&lexstate);
  LUA_ASSERT(funcstate.prev == NULL, "wrong list end");
  LUA_ASSERT(funcstate.nupvalues == 0, "no upvalues in main");
  return funcstate.f;
}



/*============================================================*/
/* GRAMMAR RULES */
/*============================================================*/


static int explist1 (LexState *ls) {
  /* explist1 -> expr { ',' expr } */
  int n = 1;  /* at least one expression */
  expdesc v;
  expr(ls, &v);
  while (ls->t.token == ',') {
    luaK_tostack(ls, &v, 1);  /* gets only 1 value from previous expression */
    next(ls);  /* skip comma */
    expr(ls, &v);
    n++;
  }
  luaK_tostack(ls, &v, 0);  /* keep open number of values of last expression */
  return n;
}


static void funcargs (LexState *ls, int slf) {
  FuncState *fs = ls->fs;
  int slevel = fs->stacklevel - slf - 1;  /* where is func in the stack */
  switch (ls->t.token) {
    case '(': {  /* funcargs -> '(' [ explist1 ] ')' */
      int line = ls->linenumber;
      int nargs = 0;
      next(ls);
      if (ls->t.token != ')')  /* arg list not empty? */
        nargs = explist1(ls);
      check_match(ls, ')', '(', line);
#ifdef LUA_COMPAT_ARGRET
      if (nargs > 0)  /* arg list is not empty? */
        luaK_setcallreturns(fs, 1);  /* last call returns only 1 value */
#else
      UNUSED(nargs);  /* to avoid warnings */
#endif
      break;
    }
    case '{': {  /* funcargs -> constructor */
      constructor(ls);
      break;
    }
    case TK_STRING: {  /* funcargs -> STRING */
      code_string(ls, ls->t.seminfo.ts);  /* must use `seminfo' before `next' */
      next(ls);
      break;
    }
    default: {
      luaK_error(ls, "function arguments expected");
      break;
    }
  }
  fs->stacklevel = slevel;  /* call will remove function and arguments */
  luaK_code2(fs, OP_CALL, slevel, MULT_RET);
}


static void var_or_func_tail (LexState *ls, expdesc *v) {
  for (;;) {
    switch (ls->t.token) {
      case '.': {  /* var_or_func_tail -> '.' NAME */
        next(ls);
        luaK_tostack(ls, v, 1);  /* `v' must be on stack */
        luaK_kstr(ls, checkname(ls));
        v->k = VINDEXED;
        break;
      }
      case '[': {  /* var_or_func_tail -> '[' exp1 ']' */
        next(ls);
        luaK_tostack(ls, v, 1);  /* `v' must be on stack */
        v->k = VINDEXED;
        exp1(ls);
        check(ls, ']');
        break;
      }
      case ':': {  /* var_or_func_tail -> ':' NAME funcargs */
        int name;
        next(ls);
        name = checkname(ls);
        luaK_tostack(ls, v, 1);  /* `v' must be on stack */
        luaK_code1(ls->fs, OP_PUSHSELF, name);
        funcargs(ls, 1);
        v->k = VEXP;
        v->u.l.t = v->u.l.f = NO_JUMP;
        break;
      }
      case '(': case TK_STRING: case '{': {  /* var_or_func_tail -> funcargs */
        luaK_tostack(ls, v, 1);  /* `v' must be on stack */
        funcargs(ls, 0);
        v->k = VEXP;
        v->u.l.t = v->u.l.f = NO_JUMP;
        break;
      }
      default: return;  /* should be follow... */
    }
  }
}


static void var_or_func (LexState *ls, expdesc *v) {
  /* var_or_func -> ['%'] NAME var_or_func_tail */
  if (optional(ls, '%')) {  /* upvalue? */
    pushupvalue(ls, str_checkname(ls));
    v->k = VEXP;
    v->u.l.t = v->u.l.f = NO_JUMP;
  }
  else  /* variable name */
    singlevar(ls, str_checkname(ls), v);
  var_or_func_tail(ls, v);
}



/*
** {======================================================================
** Rules for Constructors
** =======================================================================
*/


static void recfield (LexState *ls) {
  /* recfield -> (NAME | '['exp1']') = exp1 */
  switch (ls->t.token) {
    case TK_NAME: {
      luaK_kstr(ls, checkname(ls));
      break;
    }
    case '[': {
      next(ls);
      exp1(ls);
      check(ls, ']');
      break;
    }
    default: luaK_error(ls, "<name> or `[' expected");
  }
  check(ls, '=');
  exp1(ls);
}


static int recfields (LexState *ls) {
  /* recfields -> recfield { ',' recfield } [','] */
  FuncState *fs = ls->fs;
  int n = 1;  /* at least one element */
  recfield(ls);
  while (ls->t.token == ',') {
    next(ls);
    if (ls->t.token == ';' || ls->t.token == '}')
      break;
    recfield(ls);
    n++;
    if (n%RFIELDS_PER_FLUSH == 0)
      luaK_code1(fs, OP_SETMAP, RFIELDS_PER_FLUSH);
  }
  luaK_code1(fs, OP_SETMAP, n%RFIELDS_PER_FLUSH);
  return n;
}


static int listfields (LexState *ls) {
  /* listfields -> exp1 { ',' exp1 } [','] */
  FuncState *fs = ls->fs;
  int n = 1;  /* at least one element */
  exp1(ls);
  while (ls->t.token == ',') {
    next(ls);
    if (ls->t.token == ';' || ls->t.token == '}')
      break;
    exp1(ls);
    n++;
    luaX_checklimit(ls, n/LFIELDS_PER_FLUSH, MAXARG_A,
               "`item groups' in a list initializer");
    if (n%LFIELDS_PER_FLUSH == 0)
      luaK_code2(fs, OP_SETLIST, n/LFIELDS_PER_FLUSH - 1, LFIELDS_PER_FLUSH);
  }
  luaK_code2(fs, OP_SETLIST, n/LFIELDS_PER_FLUSH, n%LFIELDS_PER_FLUSH);
  return n;
}



static void constructor_part (LexState *ls, Constdesc *cd) {
  switch (ls->t.token) {
    case ';': case '}': {  /* constructor_part -> empty */
      cd->n = 0;
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
      cd->n = recfields(ls);
      cd->k = 1;  /* record */
      break;
    }
    default: {  /* constructor_part -> listfields */
    case_default:
      cd->n = listfields(ls);
      cd->k = 0;  /* list */
      break;
    }
  }
}


static void constructor (LexState *ls) {
  /* constructor -> '{' constructor_part [';' constructor_part] '}' */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_code1(fs, OP_CREATETABLE, 0);
  int nelems;
  Constdesc cd;
  check(ls, '{');
  constructor_part(ls, &cd);
  nelems = cd.n;
  if (optional(ls, ';')) {
    Constdesc other_cd;
    constructor_part(ls, &other_cd);
    check_condition(ls, (cd.k != other_cd.k), "invalid constructor syntax");
    nelems += other_cd.n;
  }
  check_match(ls, '}', '{', line);
  luaX_checklimit(ls, nelems, MAXARG_U, "elements in a table constructor");
  SETARG_U(fs->f->code[pc], nelems);  /* set initial table size */
}

/* }====================================================================== */




/*
** {======================================================================
** Expression parsing
** =======================================================================
*/


static void simpleexp (LexState *ls, expdesc *v) {
  FuncState *fs = ls->fs;
  switch (ls->t.token) {
    case TK_NUMBER: {  /* simpleexp -> NUMBER */
      Number r = ls->t.seminfo.r;
      next(ls);
      luaK_number(fs, r);
      break;
    }
    case TK_STRING: {  /* simpleexp -> STRING */
      code_string(ls, ls->t.seminfo.ts);  /* must use `seminfo' before `next' */
      next(ls);
      break;
    }
    case TK_NIL: {  /* simpleexp -> NIL */
      luaK_adjuststack(fs, -1);
      next(ls);
      break;
    }
    case '{': {  /* simpleexp -> constructor */
      constructor(ls);
      break;
    }
    case TK_FUNCTION: {  /* simpleexp -> FUNCTION body */
      next(ls);
      body(ls, 0, ls->linenumber);
      break;
    }
    case '(': {  /* simpleexp -> '(' expr ')' */
      next(ls);
      expr(ls, v);
      check(ls, ')');
      return;
    }
    case TK_NAME: case '%': {
      var_or_func(ls, v);
      return;
    }
    default: {
      luaK_error(ls, "<expression> expected");
      return;
    }
  }
  v->k = VEXP;
  v->u.l.t = v->u.l.f = NO_JUMP;
}


static void exp1 (LexState *ls) {
  expdesc v;
  expr(ls, &v);
  luaK_tostack(ls, &v, 1);
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
  char left;  /* left priority for each binary operator */
  char right; /* right priority */
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
    luaK_prefix(ls, uop, v);
  }
  else simpleexp(ls, v);
  /* expand while operators have priorities higher than `limit' */
  op = getbinopr(ls->t.token);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    expdesc v2;
    BinOpr nextop;
    next(ls);
    luaK_infix(ls, op, v);
    /* read sub-expression with higher priority */
    nextop = subexpr(ls, &v2, priority[op].right);
    luaK_posfix(ls, op, v, &v2);
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
  luaK_adjuststack(fs, fs->nactloc - nactloc);  /* remove local variables */
  removelocalvars(ls, fs->nactloc - nactloc);
}


static int assignment (LexState *ls, expdesc *v, int nvars) {
  int left = 0;  /* number of values left in the stack after assignment */
  luaX_checklimit(ls, nvars, MAXVARSLH, "variables in a multiple assignment");
  if (ls->t.token == ',') {  /* assignment -> ',' NAME assignment */
    expdesc nv;
    next(ls);
    var_or_func(ls, &nv);
    check_condition(ls, (nv.k != VEXP), "syntax error");
    left = assignment(ls, &nv, nvars+1);
  }
  else {  /* assignment -> '=' explist1 */
    int nexps;
    check(ls, '=');
    nexps = explist1(ls);
    adjust_mult_assign(ls, nvars, nexps);
  }
  if (v->k != VINDEXED)
    luaK_storevar(ls, v);
  else {  /* there may be garbage between table-index and value */
    luaK_code2(ls->fs, OP_SETTABLE, left+nvars+2, 1);
    left += 2;
  }
  return left;
}


static void cond (LexState *ls, expdesc *v) {
  /* cond -> exp */
  expr(ls, v);  /* read condition */
  luaK_goiftrue(ls->fs, v, 0);
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
  luaK_patchlist(fs, v.u.l.f, luaK_getlabel(fs));
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
  luaK_patchlist(fs, v.u.l.f, repeat_init);
  leavebreak(fs, &bl);
}


static void forbody (LexState *ls, int nvar, OpCode prepfor, OpCode loopfor) {
  /* forbody -> DO block END */
  FuncState *fs = ls->fs;
  int prep = luaK_code1(fs, prepfor, NO_JUMP);
  int blockinit = luaK_getlabel(fs);
  check(ls, TK_DO);
  adjustlocalvars(ls, nvar);  /* scope for control variables */
  block(ls);
  luaK_patchlist(fs, luaK_code1(fs, loopfor, NO_JUMP), blockinit);
  luaK_patchlist(fs, prep, luaK_getlabel(fs));
  removelocalvars(ls, nvar);
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
  else
    luaK_code1(fs, OP_PUSHINT, 1);  /* default step */
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
  /* next test is dirty, but avoids `in' being a reserved word */
  check_condition(ls,
       (ls->t.token == TK_NAME && ls->t.seminfo.ts == luaS_new(ls->L, "in")),
       "`in' expected");
  next(ls);  /* skip `in' */
  exp1(ls);  /* table */
  new_localvarstr(ls, "(table)", 0);
  new_localvar(ls, indexname, 1);
  new_localvar(ls, valname, 2);
  forbody(ls, 3, OP_LFORPREP, OP_LFORLOOP);
}


static void forstat (LexState *ls, int line) {
  /* forstat -> fornum | forlist */
  FuncState *fs = ls->fs;
  TString *varname;
  Breaklabel bl;
  enterbreak(fs, &bl);
  next(ls);  /* skip `for' */
  varname = str_checkname(ls);  /* first variable name */
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
    luaK_patchlist(fs, v.u.l.f, luaK_getlabel(fs));
    test_then_block(ls, &v);  /* ELSEIF cond THEN block */
  }
  if (ls->t.token == TK_ELSE) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));
    luaK_patchlist(fs, v.u.l.f, luaK_getlabel(fs));
    next(ls);  /* skip ELSE */
    block(ls);  /* `else' part */
  }
  else
    luaK_concat(fs, &escapelist, v.u.l.f);
  luaK_patchlist(fs, escapelist, luaK_getlabel(fs));
  check_match(ls, TK_END, TK_IF, line);
}


static void localstat (LexState *ls) {
  /* stat -> LOCAL NAME {',' NAME} ['=' explist1] */
  int nvars = 0;
  int nexps;
  do {
    next(ls);  /* skip LOCAL or ',' */
    new_localvar(ls, str_checkname(ls), nvars++);
  } while (ls->t.token == ',');
  if (optional(ls, '='))
    nexps = explist1(ls);
  else
    nexps = 0;
  adjust_mult_assign(ls, nvars, nexps);
  adjustlocalvars(ls, nvars);
}


static int funcname (LexState *ls, expdesc *v) {
  /* funcname -> NAME [':' NAME | '.' NAME] */
  int needself = 0;
  singlevar(ls, str_checkname(ls), v);
  if (ls->t.token == ':' || ls->t.token == '.') {
    needself = (ls->t.token == ':');
    next(ls);
    luaK_tostack(ls, v, 1);
    luaK_kstr(ls, checkname(ls));
    v->k = VINDEXED;
  }
  return needself;
}


static void funcstat (LexState *ls, int line) {
  /* funcstat -> FUNCTION funcname body */
  int needself;
  expdesc v;
  next(ls);  /* skip FUNCTION */
  needself = funcname(ls, &v);
  body(ls, needself, line);
  luaK_storevar(ls, &v);
}


static void namestat (LexState *ls) {
  /* stat -> func | ['%'] NAME assignment */
  FuncState *fs = ls->fs;
  expdesc v;
  var_or_func(ls, &v);
  if (v.k == VEXP) {  /* stat -> func */
    check_condition(ls, luaK_lastisopen(fs), "syntax error");  /* an upvalue? */
    luaK_setcallreturns(fs, 0);  /* call statement uses no results */
  }
  else {  /* stat -> ['%'] NAME assignment */
    int left = assignment(ls, &v, 1);
    luaK_adjuststack(fs, left);  /* remove eventual garbage left on stack */
  }
}


static void retstat (LexState *ls) {
  /* stat -> RETURN explist */
  FuncState *fs = ls->fs;
  next(ls);  /* skip RETURN */
  if (!block_follow(ls->t.token))
    explist1(ls);  /* optional return values */
  luaK_code1(fs, OP_RETURN, ls->fs->nactloc);
  fs->stacklevel = fs->nactloc;  /* removes all temp values */
}


static void breakstat (LexState *ls) {
  /* stat -> BREAK [NAME] */
  FuncState *fs = ls->fs;
  int currentlevel = fs->stacklevel;
  Breaklabel *bl = fs->bl;
  if (!bl)
    luaK_error(ls, "no loop to break");
  next(ls);  /* skip BREAK */
  luaK_adjuststack(fs, currentlevel - bl->stacklevel);
  luaK_concat(fs, &bl->breaklist, luaK_jump(fs));
  /* correct stack for compiler and symbolic execution */
  luaK_adjuststack(fs, bl->stacklevel - currentlevel);
}


static int stat (LexState *ls) {
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
    case TK_FUNCTION: {  /* stat -> funcstat */
      funcstat(ls, line);
      return 0;
    }
    case TK_LOCAL: {  /* stat -> localstat */
      localstat(ls);
      return 0;
    }
    case TK_NAME: case '%': {  /* stat -> namestat */
      namestat(ls);
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
      luaK_error(ls, "<statement> expected");
      return 0;  /* to avoid warnings */
    }
  }
}


static void parlist (LexState *ls) {
  /* parlist -> [ param { ',' param } ] */
  int nparams = 0;
  int dots = 0;
  if (ls->t.token != ')') {  /* is `parlist' not empty? */
    do {
      switch (ls->t.token) {
        case TK_DOTS: next(ls); dots = 1; break;
        case TK_NAME: new_localvar(ls, str_checkname(ls), nparams++); break;
        default: luaK_error(ls, "<name> or `...' expected");
      }
    } while (!dots && optional(ls, ','));
  }
  code_params(ls, nparams, dots);
}


static void body (LexState *ls, int needself, int line) {
  /* body ->  '(' parlist ')' chunk END */
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
  pushclosure(ls, &new_fs);
}


/* }====================================================================== */


static void chunk (LexState *ls) {
  /* chunk -> { stat [';'] } */
  int islast = 0;
  while (!islast && !block_follow(ls->t.token)) {
    islast = stat(ls);
    optional(ls, ';');
    LUA_ASSERT(ls->fs->stacklevel == ls->fs->nactloc,
               "stack size != # local vars");
  }
}

