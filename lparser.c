/*
** $Id: lparser.c,v 1.68 2000/03/10 18:37:44 roberto Exp roberto $
** LL(1) Parser and code generator for Lua
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#define LUA_REENTRANT

#include "lcode.h"
#include "ldo.h"
#include "lfunc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"


/*
** check whether arbitrary limits fit into respective opcode types
*/
#if MAXLOCALS > MAXARG_U || MAXUPVALUES > MAXARG_B || MAXVARSLH > MAXARG_B || \
    MAXPARAMS > MAXLOCALS || MAXSTACK > MAXARG_A || \
    LFIELDS_PER_FLUSH > MAXARG_B || MULT_RET > MAXARG_B
#error invalid limits
#endif



/*
** Constructors descriptor:
** `n' indicates number of elements, and `k' signals whether
** it is a list constructor (k = 0) or a record constructor (k = 1)
** or empty (k = ';' or '}')
*/
typedef struct constdesc {
  int n;
  int k;
} constdesc;




/*
** prototypes for recursive non-terminal functions
*/
static void body (LexState *ls, int needself, int line);
static void chunk (LexState *ls);
static void constructor (LexState *ls);
static void expr (LexState *ls, expdesc *v);
static void exp1 (LexState *ls);


static void next (LexState *ls) {
  ls->token = luaX_lex(ls);
}


static void error_expected (LexState *ls, int token) {
  char buff[100], t[TOKEN_LEN];
  luaX_token2str(token, t);
  sprintf(buff, "`%.20s' expected", t);
  luaK_error(ls, buff);
}


static void error_unexpected (LexState *ls) {
  luaK_error(ls, "unexpected token");
}


static void error_unmatched (LexState *ls, int what, int who, int where) {
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

static void check (LexState *ls, int c) {
  if (ls->token != c)
    error_expected(ls, c);
  next(ls);
}


static int optional (LexState *ls, int c) {
  if (ls->token == c) {
    next(ls);
    return 1;
  }
  else return 0;
}


static void checklimit (LexState *ls, int val, int limit, const char *msg) {
  if (val > limit) {
    char buff[100];
    sprintf(buff, "too many %.50s (limit=%d)", msg, limit);
    luaK_error(ls, buff);
  }
}


static void check_debugline (LexState *ls) {
  FuncState *fs = ls->fs;
  if (ls->L->debug && ls->linenumber != fs->lastsetline) {
    luaK_U(fs, OP_SETLINE, ls->linenumber, 0);
    fs->lastsetline = ls->linenumber;
  }
}


static void check_match (LexState *ls, int what, int who, int where) {
  if (ls->token != what)
    error_unmatched(ls, what, who, where);
  check_debugline(ls);  /* to `mark' the `what' */
  next(ls);
}


static int string_constant (FuncState *fs, TString *s) {
  Proto *f = fs->f;
  int c = s->constindex;
  if (c >= f->nkstr || f->kstr[c] != s) {
    luaM_growvector(fs->L, f->kstr, f->nkstr, 1, TString *,
                    constantEM, MAXARG_U);
    c = f->nkstr++;
    f->kstr[c] = s;
    s->constindex = c;  /* hint for next time */
  }
  return c;
}


static void code_string (LexState *ls, TString *s) {
  luaK_kstr(ls, string_constant(ls->fs, s));
}


static int checkname (LexState *ls) {
  int sc;
  if (ls->token != TK_NAME)
    luaK_error(ls, "<name> expected");
  sc = string_constant(ls->fs, ls->seminfo.ts);
  next(ls);
  return sc;
}


static TString *str_checkname (LexState *ls) {
  int i = checkname(ls);  /* this call may realloc `f->consts' */
  return ls->fs->f->kstr[i];
}


static void luaI_registerlocalvar (LexState *ls, TString *varname,
                                   int line) {
  FuncState *fs = ls->fs;
  if (fs->nvars != -1) {  /* debug information? */
    Proto *f = fs->f;
    luaM_growvector(ls->L, f->locvars, fs->nvars, 1, LocVar, "", MAX_INT);
    f->locvars[fs->nvars].varname = varname;
    f->locvars[fs->nvars].line = line;
    fs->nvars++;
  }
}


static void luaI_unregisterlocalvar (LexState *ls, int line) {
  luaI_registerlocalvar(ls, NULL, line);
}


static void store_localvar (LexState *ls, TString *name, int n) {
  FuncState *fs = ls->fs;
  checklimit(ls, fs->nlocalvar+n+1, MAXLOCALS, "local variables");
  fs->localvar[fs->nlocalvar+n] = name;
}


static void adjustlocalvars (LexState *ls, int nvars, int line) {
  FuncState *fs = ls->fs;
  int i;
  fs->nlocalvar += nvars;
  for (i=fs->nlocalvar-nvars; i<fs->nlocalvar; i++)
    luaI_registerlocalvar(ls, fs->localvar[i], line);
}


static void add_localvar (LexState *ls, TString *name) {
  store_localvar(ls, name, 0);
  adjustlocalvars(ls, 1, 0);
}


static int aux_localname (FuncState *fs, TString *n) {
  int i;
  for (i=fs->nlocalvar-1; i >= 0; i--)
    if (n == fs->localvar[i]) return i;  /* local var index */
  return -1;  /* not found */
}


static void singlevar (LexState *ls, TString *n, expdesc *var, int prev) {
  FuncState *fs = prev ? ls->fs->prev : ls->fs;
  int i = aux_localname(fs, n);
  if (i >= 0) {  /* local value? */
    var->k = VLOCAL;
    var->u.index = i;
  }
  else {
    FuncState *level = fs;
    while ((level = level->prev) != NULL)  /* check shadowing */
      if (aux_localname(level, n) >= 0)
        luaX_syntaxerror(ls, "cannot access a variable in outer scope", n->str);
    var->k = VGLOBAL;
    var->u.index = string_constant(fs, n);
  }
}


static int indexupvalue (LexState *ls, TString *n) {
  FuncState *fs = ls->fs;
  expdesc v;
  int i;
  singlevar(ls, n, &v, 1);
  for (i=0; i<fs->nupvalues; i++) {
    if (fs->upvalues[i].k == v.k && fs->upvalues[i].u.index == v.u.index)
      return i;
  }
  /* new one */
  ++(fs->nupvalues);
  checklimit(ls, fs->nupvalues, MAXUPVALUES, "upvalues");
  fs->upvalues[i] = v;  /* i = fs->nupvalues - 1 */
  return i;
}


static void pushupvalue (LexState *ls, TString *n) {
  FuncState *fs = ls->fs;
  if (fs->prev == NULL)
    luaX_syntaxerror(ls, "cannot access upvalue in main", n->str);
  if (aux_localname(ls->fs, n) >= 0)
    luaX_syntaxerror(ls, "cannot access an upvalue in current scope", n->str);
  luaK_U(fs, OP_PUSHUPVALUE, indexupvalue(ls, n), 1);
}


static void adjust_mult_assign (LexState *ls, int nvars, int nexps) {
  FuncState *fs = ls->fs;
  int diff = nexps - nvars;
  if (nexps == 0 || !luaK_lastisopen(fs)) {  /* list is empty or closed */
    /* push or pop eventual difference between list lengths */
    luaK_adjuststack(fs, diff);
  }
  else {  /* list ends in a function call; must correct it */
    diff--;  /* do not count function call itself */
    if (diff <= 0) {  /* more variables than values? */
      /* function call must provide extra values */
      luaK_setcallreturns(fs, -diff);
    }
    else {  /* more values than variables */
      luaK_setcallreturns(fs, 0);  /* call should provide no value */
      luaK_adjuststack(fs, diff);  /* pop eventual extra values */
    }
  }
}


static void code_args (LexState *ls, int nparams, int dots) {
  FuncState *fs = ls->fs;
  adjustlocalvars(ls, nparams, 0);
  checklimit(ls, fs->nlocalvar, MAXPARAMS, "parameters");
  nparams = fs->nlocalvar;  /* `self' could be there already */
  fs->f->numparams = nparams;
  fs->f->is_vararg = dots;
  if (!dots)
    luaK_deltastack(fs, nparams);
  else {
    luaK_deltastack(fs, nparams+1);
    add_localvar(ls, luaS_newfixed(ls->L, "arg"));
  }
}


static int getvarname (LexState *ls, expdesc *var) {
  switch (var->k) {
    case VGLOBAL:
      return var->u.index;
    case VLOCAL:
      return string_constant(ls->fs, ls->fs->localvar[var->u.index]);
      break;
    default:
      error_unexpected(ls);  /* there is no `var name' */
      return 0;  /* to avoid warnings */
  }
}


static void func_onstack (LexState *ls, FuncState *func) {
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int i;
  for (i=0; i<func->nupvalues; i++)
    luaK_tostack(ls, &func->upvalues[i], 1);
  luaM_growvector(ls->L, f->kproto, f->nkproto, 1, Proto *,
                  constantEM, MAXARG_A);
  f->kproto[f->nkproto++] = func->f;
  luaK_deltastack(fs, 1);  /* CLOSURE puts one extra element before popping */
  luaK_AB(fs, OP_CLOSURE, f->nkproto-1, func->nupvalues, -func->nupvalues);
}


static void init_state (LexState *ls, FuncState *fs, TString *source) {
  lua_State *L = ls->L;
  Proto *f = luaF_newproto(ls->L);
  fs->prev = ls->fs;  /* linked list of funcstates */
  fs->ls = ls;
  fs->L = ls->L;
  ls->fs = fs;
  fs->stacksize = 0;
  fs->nlocalvar = 0;
  fs->nupvalues = 0;
  fs->lastsetline = 0;
  fs->f = f;
  f->source = source;
  fs->pc = 0;
  fs->lasttarget = 0;
  f->code = NULL;
  f->maxstacksize = 0;
  f->numparams = 0;  /* default for main chunk */
  f->is_vararg = 0;  /* default for main chunk */
  fs->nvars = (L->debug) ? 0 : -1;  /* flag no debug information? */
  /* push function (to avoid GC) */
  tfvalue(L->top) = f;
  ttype(L->top) = TAG_LPROTO;
  incr_top;
}


static void close_func (LexState *ls) {
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  luaK_0(fs, OP_END, 0);
  luaM_reallocvector(L, f->code, fs->pc, Instruction);
  luaM_reallocvector(L, f->kstr, f->nkstr, TString *);
  luaM_reallocvector(L, f->knum, f->nknum, Number);
  luaM_reallocvector(L, f->kproto, f->nkproto, Proto *);
  if (fs->nvars != -1) {  /* debug information? */
    luaI_registerlocalvar(ls, NULL, -1);  /* flag end of vector */
    luaM_reallocvector(L, f->locvars, fs->nvars, LocVar);
  }
  ls->fs = fs->prev;
  L->top--;  /* pop function */
}


Proto *luaY_parser (lua_State *L, ZIO *z) {
  struct LexState lexstate;
  struct FuncState funcstate;
  luaX_setinput(L, &lexstate, z);
  init_state(&lexstate, &funcstate, luaS_new(L, zname(z)));
  next(&lexstate);  /* read first token */
  chunk(&lexstate);
  if (lexstate.token != TK_EOS)
    luaK_error(&lexstate, "<eof> expected");
  close_func(&lexstate);
  return funcstate.f;
}



/*============================================================*/
/* GRAMAR RULES */
/*============================================================*/


static int explist1 (LexState *ls) {
  /* explist1 -> expr { ',' expr } */
  int n = 1;  /* at least one expression */
  expdesc v;
  expr(ls, &v);
  while (ls->token == ',') {
    luaK_tostack(ls, &v, 1);  /* gets only 1 value from previous expression */
    next(ls);  /* skip comma */
    expr(ls, &v);
    n++;
  }
  luaK_tostack(ls, &v, 0);
  return n;
}


static int explist (LexState *ls) {
  /* explist -> [ explist1 ] */
  switch (ls->token) {
    case TK_ELSE: case TK_ELSEIF: case TK_END: case TK_UNTIL:
    case TK_EOS: case ';': case ')':
      return 0;  /* empty list */

    default:
      return explist1(ls);
  }
}


static void funcargs (LexState *ls, int slf) {
  FuncState *fs = ls->fs;
  int slevel = fs->stacksize - slf - 1;  /* where is func in the stack */
  switch (ls->token) {
    case '(': {  /* funcargs -> '(' explist ')' */
      int line = ls->linenumber;
      int nargs;
      next(ls);
      nargs = explist(ls);
      check_match(ls, ')', '(', line);
#ifdef LUA_COMPAT_ARGRET
      if (nargs > 0)  /* arg list is not empty? */
        luaK_setcallreturns(fs, 1);  /* last call returns only 1 value */
#endif
      break;
    }

    case '{':  /* funcargs -> constructor */
      constructor(ls);
      break;

    case TK_STRING:  /* funcargs -> STRING */
      code_string(ls, ls->seminfo.ts);  /* must use `seminfo' before `next' */
      next(ls);
      break;

    default:
      luaK_error(ls, "function arguments expected");
      break;
  }
  fs->stacksize = slevel;  /* call will remove function and arguments */
  luaK_AB(fs, OP_CALL, slevel, MULT_RET, 0);
}


static void var_or_func_tail (LexState *ls, expdesc *v) {
  for (;;) {
    switch (ls->token) {
      case '.':  /* var_or_func_tail -> '.' NAME */
        next(ls);
        luaK_tostack(ls, v, 1);  /* `v' must be on stack */
        luaK_kstr(ls, checkname(ls));
        v->k = VINDEXED;
        break;

      case '[':  /* var_or_func_tail -> '[' exp1 ']' */
        next(ls);
        luaK_tostack(ls, v, 1);  /* `v' must be on stack */
        v->k = VINDEXED;
        exp1(ls);
        check(ls, ']');
        break;

      case ':': {  /* var_or_func_tail -> ':' NAME funcargs */
        int name;
        next(ls);
        name = checkname(ls);
        luaK_tostack(ls, v, 1);  /* `v' must be on stack */
        luaK_U(ls->fs, OP_PUSHSELF, name, 1);
        funcargs(ls, 1);
        v->k = VEXP;
        v->u.l.t = v->u.l.f = NO_JUMP;
        break;
      }

      case '(': case TK_STRING: case '{':  /* var_or_func_tail -> funcargs */
        luaK_tostack(ls, v, 1);  /* `v' must be on stack */
        funcargs(ls, 0);
        v->k = VEXP;
        v->u.l.t = v->u.l.f = NO_JUMP;
        break;

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
    singlevar(ls, str_checkname(ls), v, 0);
  var_or_func_tail(ls, v);
}



/*
** {======================================================================
** Rules for Constructors
** =======================================================================
*/


static void recfield (LexState *ls) {
  /* recfield -> (NAME | '['exp1']') = exp1 */
  switch (ls->token) {
    case TK_NAME:
      luaK_kstr(ls, checkname(ls));
      break;

    case '[':
      next(ls);
      exp1(ls);
      check(ls, ']');
      break;

    default: luaK_error(ls, "<name> or `[' expected");
  }
  check(ls, '=');
  exp1(ls);
}


static int recfields (LexState *ls) {
  /* recfields -> { ',' recfield } [','] */
  FuncState *fs = ls->fs;
  int n = 1;  /* one has been read before */
  int mod_n = 1;  /* mod_n == n%RFIELDS_PER_FLUSH */
  while (ls->token == ',') {
    next(ls);
    if (ls->token == ';' || ls->token == '}')
      break;
    recfield(ls);
    n++;
    if (++mod_n == RFIELDS_PER_FLUSH) {
      luaK_U(fs, OP_SETMAP, RFIELDS_PER_FLUSH-1, -2*RFIELDS_PER_FLUSH);
      mod_n = 0;
    }
  }
  if (mod_n)
    luaK_U(fs, OP_SETMAP, mod_n-1, -2*mod_n);
  return n;
}


static int listfields (LexState *ls) {
  /* listfields -> { ',' exp1 } [','] */
  FuncState *fs = ls->fs;
  int n = 1;  /* one has been read before */
  int mod_n = 1;  /* mod_n == n%LFIELDS_PER_FLUSH */
  while (ls->token == ',') {
    next(ls);
    if (ls->token == ';' || ls->token == '}')
      break;
    exp1(ls);
    n++;
    checklimit(ls, n, MAXARG_A*LFIELDS_PER_FLUSH,
               "items in a list initializer");
    if (++mod_n == LFIELDS_PER_FLUSH) {
      luaK_AB(fs, OP_SETLIST, n/LFIELDS_PER_FLUSH - 1, LFIELDS_PER_FLUSH-1,
              -LFIELDS_PER_FLUSH);
      mod_n = 0;
    }
  }
  if (mod_n > 0)
    luaK_AB(fs, OP_SETLIST, n/LFIELDS_PER_FLUSH, mod_n-1, -mod_n);
  return n;
}



static void constructor_part (LexState *ls, constdesc *cd) {
  switch (ls->token) {
    case ';': case '}':  /* constructor_part -> empty */
      cd->n = 0;
      cd->k = ls->token;
      return;

    case TK_NAME: {
      expdesc v;
      expr(ls, &v);
      if (ls->token == '=') {
      luaK_kstr(ls, getvarname(ls, &v));
      next(ls);  /* skip '=' */
      exp1(ls);
        cd->n = recfields(ls);
        cd->k = 1;  /* record */
      }
      else {
        luaK_tostack(ls, &v, 0);
        cd->n = listfields(ls);
        cd->k = 0;  /* list */
      }
      break;
    }

    case '[':  /* constructor_part -> recfield recfields */
      recfield(ls);
      cd->n = recfields(ls);
      cd->k = 1;  /* record */
      break;

    default:  /* constructor_part -> exp1 listfields */
      exp1(ls);
      cd->n = listfields(ls);
      cd->k = 0;  /* list */
      break;
  }
}


static void constructor (LexState *ls) {
  /* constructor -> '{' constructor_part [';' constructor_part] '}' */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_U(fs, OP_CREATETABLE, 0, 1);
  int nelems;
  constdesc cd;
  check(ls, '{');
  constructor_part(ls, &cd);
  nelems = cd.n;
  if (ls->token == ';') {
    constdesc other_cd;
    next(ls);
    constructor_part(ls, &other_cd);
    if (cd.k == other_cd.k)  /* repeated parts? */
      luaK_error(ls, "invalid constructor syntax");
    nelems += other_cd.n;
  }
  check_match(ls, '}', '{', line);
  /* set initial table size */
  SETARG_U(fs->f->code[pc], nelems);
}

/* }====================================================================== */




/*
** {======================================================================
** Expression parsing
** =======================================================================
*/


static void simpleexp (LexState *ls, expdesc *v) {
  FuncState *fs = ls->fs;
  check_debugline(ls);
  switch (ls->token) {
    case TK_NUMBER: {  /* simpleexp -> NUMBER */
      Number r = ls->seminfo.r;
      next(ls);
      luaK_number(fs, r);
      break;
    }

    case TK_STRING:  /* simpleexp -> STRING */
      code_string(ls, ls->seminfo.ts);  /* must use `seminfo' before `next' */
      next(ls);
      break;

    case TK_NIL:  /* simpleexp -> NIL */
      luaK_adjuststack(fs, -1);
      next(ls);
      break;

    case '{':  /* simpleexp -> constructor */
      constructor(ls);
      break;

    case TK_FUNCTION:  /* simpleexp -> FUNCTION body */
      next(ls);
      body(ls, 0, ls->linenumber);
      break;

    case '(':  /* simpleexp -> '(' expr ')' */
      next(ls);
      expr(ls, v);
      check(ls, ')');
      return;

    case TK_NAME: case '%':
      var_or_func(ls, v);
      return;

    default:
      luaK_error(ls, "<expression> expected");
      return;
  }
  v->k = VEXP;
  v->u.l.t = v->u.l.f = NO_JUMP;
}


static void exp1 (LexState *ls) {
  expdesc v;
  expr(ls, &v);
  luaK_tostack(ls, &v, 1);
}


/*
** gets priorities of an operator. Returns the priority to the left, and
** sets `rp' to the priority to the right.
*/
static int get_priority (int op, int *rp) {
  switch (op) {

    case  '^': *rp = 8; return 9;  /* right associative */

#define UNARY_PRIORITY	7

    case  '*': case  '/': *rp = 6; return 6;

    case  '+': case  '-': *rp = 5; return 5;

    case  TK_CONC: *rp = 3; return 4;  /* right associative (?) */

    case TK_EQ: case  TK_NE: case  '>': case  '<': case  TK_LE: case  TK_GE:
      *rp = 2; return 2;

    case TK_AND: case TK_OR: *rp = 1; return 1;

    default: *rp = -1; return -1;
  }
}


/*
** subexpr -> (simplexep | (NOT | '-') subexpr) { binop subexpr }
** where `binop' is any binary operator with a priority higher than `limit'
*/
static void subexpr (LexState *ls, expdesc *v, int limit) {
  int rp;
  if (ls->token == '-' || ls->token == TK_NOT) {
    int op = ls->token;  /* operator */
    next(ls);
    subexpr(ls, v, UNARY_PRIORITY);
    luaK_prefix(ls, op, v);
  }
  else simpleexp(ls, v);
  /* expand while operators have priorities higher than `limit' */
  while (get_priority(ls->token, &rp) > limit) {
    expdesc v2;
    int op = ls->token;  /* current operator (with priority == `rp') */
    next(ls);
    luaK_infix(ls, op, v);
    subexpr(ls, &v2, rp);  /* read sub-expression with priority > `rp' */
    luaK_posfix(ls, op, v, &v2);
  }
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


static void block (LexState *ls) {
  /* block -> chunk */
  FuncState *fs = ls->fs;
  int nlocalvar = fs->nlocalvar;
  chunk(ls);
  luaK_adjuststack(fs, fs->nlocalvar - nlocalvar);  /* remove local variables */
  for (; fs->nlocalvar > nlocalvar; fs->nlocalvar--)
    luaI_unregisterlocalvar(ls, fs->lastsetline);
}


static int assignment (LexState *ls, expdesc *v, int nvars) {
  int left = 0;
  checklimit(ls, nvars, MAXVARSLH, "variables in a multiple assignment");
  if (ls->token == ',') {  /* assignment -> ',' NAME assignment */
    expdesc nv;
    next(ls);
    var_or_func(ls, &nv);
    if (nv.k == VEXP)
      luaK_error(ls, "syntax error");
    left = assignment(ls, &nv, nvars+1);
  }
  else {  /* assignment -> '=' explist1 */
    int nexps;;
    if (ls->token != '=')
      error_unexpected(ls);
    next(ls);
    nexps = explist1(ls);
    adjust_mult_assign(ls, nvars, nexps);
  }
  if (v->k != VINDEXED || left+(nvars-1) == 0) {
    /* global/local var or indexed var without values in between */
    luaK_storevar(ls, v);
  }
  else {  /* indexed var with values in between*/
    luaK_U(ls->fs, OP_SETTABLE, left+(nvars-1), -1);
    left += 2;  /* table&index are not popped, because they aren't on top */
  }
  return left;
}


/* maximum size of a while condition */
#ifndef MAX_WHILE_EXP
#define MAX_WHILE_EXP 200	/* arbitrary limit */
#endif

static void whilestat (LexState *ls, int line) {
  /* whilestat -> WHILE exp1 DO block END */
  Instruction buffer[MAX_WHILE_EXP];
  FuncState *fs = ls->fs;
  int while_init = luaK_getlabel(fs);
  int loopentry;  /* point to jump to repeat the loop */
  int cond_init;  /* init of condition, after the move */
  int cond_size;
  expdesc v;
  int i;
  next(ls);  /* skip WHILE */
  expr(ls, &v);  /* read condition */
  luaK_goiffalse(fs, &v, 0);
  cond_size = fs->pc - while_init;
  /* save condition (to move it to after body) */
  if (cond_size > MAX_WHILE_EXP)
    luaK_error(ls, "while condition too complex");
  for (i=0; i<cond_size; i++) buffer[i] = fs->f->code[while_init+i];
  /* go back to state prior condition */
  fs->pc = while_init;
  luaK_S(fs, OP_JMP, 0, 0);  /* initial jump to condition */
  check(ls, TK_DO);
  loopentry = luaK_getlabel(fs);
  block(ls);
  check_match(ls, TK_END, TK_WHILE, line);
  cond_init = luaK_getlabel(fs);
  luaK_fixjump(fs, while_init, cond_init);
  /* correct `v' and  copy condition to new position */
  if (v.u.l.t != NO_JUMP) v.u.l.t += cond_init-while_init;
  for (i=0; i<cond_size; i++) luaK_primitivecode(fs, buffer[i]);
  luaK_patchlist(fs, v.u.l.t, loopentry);
  luaK_getlabel(fs);  /* mark possible jump to this point */
}


static void repeatstat (LexState *ls, int line) {
  /* repeatstat -> REPEAT block UNTIL exp1 */
  FuncState *fs = ls->fs;
  int repeat_init = luaK_getlabel(fs);
  expdesc v;
  next(ls);
  block(ls);
  check_match(ls, TK_UNTIL, TK_REPEAT, line);
  expr(ls, &v);
  luaK_goiftrue(fs, &v, 0);
  luaK_patchlist(fs, v.u.l.f, repeat_init);
}


static int localnamelist (LexState *ls) {
  /* localnamelist -> NAME {',' NAME} */
  int i = 1;
  store_localvar(ls, str_checkname(ls), 0);
  while (ls->token == ',') {
    next(ls);
    store_localvar(ls, str_checkname(ls), i++);
  }
  return i;
}


static int decinit (LexState *ls) {
  /* decinit -> ['=' explist1] */
  if (ls->token == '=') {
    next(ls);
    return explist1(ls);
  }
  else
    return 0;  /* no initializations */
}


static void localstat (LexState *ls) {
  /* stat -> LOCAL localnamelist decinit */
  FuncState *fs = ls->fs;
  int nvars;
  int nexps;
  check_debugline(ls);
  next(ls);
  nvars = localnamelist(ls);
  nexps = decinit(ls);
  adjustlocalvars(ls, nvars, fs->lastsetline);
  adjust_mult_assign(ls, nvars, nexps);
}


static int funcname (LexState *ls, expdesc *v) {
  /* funcname -> NAME [':' NAME | '.' NAME] */
  int needself = 0;
  singlevar(ls, str_checkname(ls), v, 0);
  if (ls->token == ':' || ls->token == '.') {
    needself = (ls->token == ':');
    next(ls);
    luaK_tostack(ls, v, 1);
    luaK_kstr(ls, checkname(ls));
    v->k = VINDEXED;
  }
  return needself;
}


static int funcstat (LexState *ls, int line) {
  /* funcstat -> FUNCTION funcname body */
  int needself;
  expdesc v;
  if (ls->fs->prev)  /* inside other function? */
    return 0;
  check_debugline(ls);
  next(ls);
  needself = funcname(ls, &v);
  body(ls, needself, line);
  luaK_storevar(ls, &v);
  return 1;
}


static void namestat (LexState *ls) {
  /* stat -> func | ['%'] NAME assignment */
  FuncState *fs = ls->fs;
  expdesc v;
  check_debugline(ls);
  var_or_func(ls, &v);
  if (v.k == VEXP) {  /* stat -> func */
    if (!luaK_lastisopen(fs))  /* is just an upvalue? */
      luaK_error(ls, "syntax error");
    luaK_setcallreturns(fs, 0);  /* call statement uses no results */
  }
  else {  /* stat -> ['%'] NAME assignment */
    int left = assignment(ls, &v, 1);
    luaK_adjuststack(fs, left);  /* remove eventual garbage left on stack */
  }
}


static void ifpart (LexState *ls, int line) {
  /* ifpart -> cond THEN block (ELSEIF ifpart | [ELSE block] END) */
  FuncState *fs = ls->fs;
  expdesc v;
  int elseinit;
  next(ls);  /* skip IF or ELSEIF */
  expr(ls, &v);  /* cond */
  luaK_goiftrue(fs, &v, 0);
  check(ls, TK_THEN);
  block(ls);  /* `then' part */
  luaK_S(fs, OP_JMP, 0, 0);  /* 2nd jump: over `else' part */
  elseinit = luaK_getlabel(fs);  /* address of 2nd jump == elseinit-1 */
  if (ls->token == TK_ELSEIF)
    ifpart(ls, line);
  else {
    if (optional(ls, TK_ELSE))
      block(ls);  /* `else' part */
    check_match(ls, TK_END, TK_IF, line);
  }
  if (fs->pc > elseinit) {  /* is there an `else' part? */
    luaK_fixjump(fs, elseinit-1, luaK_getlabel(fs));  /* fix 2nd jump */
  }
  else {  /* no else part */
    fs->pc--;  /* remove 2nd jump */
    elseinit = luaK_getlabel(fs);  /* `elseinit' points to end */
  }
  luaK_patchlist(fs, v.u.l.f, elseinit);  /* fix 1st jump to `else' part */
}


static int stat (LexState *ls) {
  int line = ls->linenumber;  /* may be needed for error messages */
  switch (ls->token) {
    case TK_IF:  /* stat -> IF ifpart END */
      ifpart(ls, line);
      return 1;

    case TK_WHILE:  /* stat -> whilestat */
      whilestat(ls, line);
      return 1;

    case TK_DO: {  /* stat -> DO block END */
      next(ls);
      block(ls);
      check_match(ls, TK_END, TK_DO, line);
      return 1;
    }

    case TK_REPEAT:  /* stat -> repeatstat */
      repeatstat(ls, line);
      return 1;

    case TK_FUNCTION:  /* stat -> funcstat */
      return funcstat(ls, line);

    case TK_LOCAL:  /* stat -> localstat */
      localstat(ls);
      return 1;

    case TK_NAME: case '%':  /* stat -> namestat */
      namestat(ls);
      return 1;

    case TK_RETURN: case ';': case TK_ELSE: case TK_ELSEIF:
    case TK_END: case TK_UNTIL: case TK_EOS:  /* `stat' follow */
      return 0;

    default:
      error_unexpected(ls);
      return 0;  /* to avoid warnings */
  }
}


static void parlist (LexState *ls) {
  int nparams = 0;
  int dots = 0;
  switch (ls->token) {
    case TK_DOTS:  /* parlist -> DOTS */
      next(ls);
      dots = 1;
      break;

    case TK_NAME:  /* parlist, tailparlist -> NAME [',' tailparlist] */
      init:
      store_localvar(ls, str_checkname(ls), nparams++);
      if (ls->token == ',') {
        next(ls);
        switch (ls->token) {
          case TK_DOTS:  /* tailparlist -> DOTS */
            next(ls);
            dots = 1;
            break;

          case TK_NAME:  /* tailparlist -> NAME [',' tailparlist] */
            goto init;

          default: luaK_error(ls, "<name> or `...' expected");
        }
      }
      break;

    case ')': break;  /* parlist -> empty */

    default: luaK_error(ls, "<name> or `...' expected");
  }
  code_args(ls, nparams, dots);
}


static void body (LexState *ls, int needself, int line) {
  /* body ->  '(' parlist ')' chunk END */
  FuncState new_fs;
  init_state(ls, &new_fs, ls->fs->f->source);
  new_fs.f->lineDefined = line;
  check(ls, '(');
  if (needself)
    add_localvar(ls, luaS_newfixed(ls->L, "self"));
  parlist(ls);
  check(ls, ')');
  chunk(ls);
  check_match(ls, TK_END, TK_FUNCTION, line);
  close_func(ls);
  func_onstack(ls, &new_fs);
}


static void ret (LexState *ls) {
  /* ret -> [RETURN explist sc] */
  if (ls->token == TK_RETURN) {
    FuncState *fs = ls->fs;
    int nexps;  /* number of expressions returned */
    check_debugline(ls);
    next(ls);
    nexps = explist(ls);
    luaK_retcode(fs, ls->fs->nlocalvar, nexps);
    fs->stacksize = fs->nlocalvar;  /* removes all temp values */
    optional(ls, ';');
  }
}

/* }====================================================================== */


static void chunk (LexState *ls) {
  /* chunk -> { stat [;] } ret */
  while (stat(ls)) {
    LUA_ASSERT(ls->L, ls->fs->stacksize == ls->fs->nlocalvar,
               "stack size != # local vars");
    optional(ls, ';');
  }
  ret(ls);  /* optional return */
}

