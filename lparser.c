/*
** $Id: lparser.c,v 1.64 2000/03/03 20:29:25 roberto Exp roberto $
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
  if (ls->L->debug && ls->linenumber != ls->fs->lastsetline) {
    luaK_U(ls, SETLINE, ls->linenumber, 0);
    ls->fs->lastsetline = ls->linenumber;
  }
}


static void check_match (LexState *ls, int what, int who, int where) {
  if (ls->token != what)
    error_unmatched(ls, what, who, where);
  check_debugline(ls);  /* to `mark' the `what' */
  next(ls);
}


static int string_constant (LexState *ls, FuncState *fs, TaggedString *s) {
  TProtoFunc *f = fs->f;
  int c = s->constindex;
  if (c >= f->nkstr || f->kstr[c] != s) {
    luaM_growvector(ls->L, f->kstr, f->nkstr, 1, TaggedString *,
                    constantEM, MAXARG_U);
    c = f->nkstr++;
    f->kstr[c] = s;
    s->constindex = c;  /* hint for next time */
  }
  return c;
}


static void code_string (LexState *ls, TaggedString *s) {
  luaK_kstr(ls, string_constant(ls, ls->fs, s));
}


static int checkname (LexState *ls) {
  int sc;
  if (ls->token != NAME)
    luaK_error(ls, "<name> expected");
  sc = string_constant(ls, ls->fs, ls->seminfo.ts);
  next(ls);
  return sc;
}


static TaggedString *str_checkname (LexState *ls) {
  int i = checkname(ls);  /* this call may realloc `f->consts' */
  return ls->fs->f->kstr[i];
}


static void luaI_registerlocalvar (LexState *ls, TaggedString *varname,
                                   int line) {
  FuncState *fs = ls->fs;
  if (fs->nvars != -1) {  /* debug information? */
    TProtoFunc *f = fs->f;
    luaM_growvector(ls->L, f->locvars, fs->nvars, 1, LocVar, "", MAX_INT);
    f->locvars[fs->nvars].varname = varname;
    f->locvars[fs->nvars].line = line;
    fs->nvars++;
  }
}


static void luaI_unregisterlocalvar (LexState *ls, int line) {
  luaI_registerlocalvar(ls, NULL, line);
}


static void store_localvar (LexState *ls, TaggedString *name, int n) {
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


static void add_localvar (LexState *ls, TaggedString *name) {
  store_localvar(ls, name, 0);
  adjustlocalvars(ls, 1, 0);
}


static int aux_localname (FuncState *fs, TaggedString *n) {
  int i;
  for (i=fs->nlocalvar-1; i >= 0; i--)
    if (n == fs->localvar[i]) return i;  /* local var index */
  return -1;  /* not found */
}


static void singlevar (LexState *ls, TaggedString *n, expdesc *var, int prev) {
  FuncState *fs = prev ? ls->fs->prev : ls->fs;
  int i = aux_localname(fs, n);
  if (i >= 0) {  /* local value? */
    var->k = VLOCAL;
    var->info = i;
  }
  else {
    FuncState *level = fs;
    while ((level = level->prev) != NULL)  /* check shadowing */
      if (aux_localname(level, n) >= 0)
        luaX_syntaxerror(ls, "cannot access a variable in outer scope", n->str);
    var->k = VGLOBAL;
    var->info = string_constant(ls, fs, n);
  }
}


static int indexupvalue (LexState *ls, TaggedString *n) {
  FuncState *fs = ls->fs;
  expdesc v;
  int i;
  singlevar(ls, n, &v, 1);
  for (i=0; i<fs->nupvalues; i++) {
    if (fs->upvalues[i].k == v.k && fs->upvalues[i].info == v.info)
      return i;
  }
  /* new one */
  ++(fs->nupvalues);
  checklimit(ls, fs->nupvalues, MAXUPVALUES, "upvalues");
  fs->upvalues[i] = v;  /* i = fs->nupvalues - 1 */
  return i;
}


static void pushupvalue (LexState *ls, TaggedString *n) {
  if (ls->fs->prev == NULL)
    luaX_syntaxerror(ls, "cannot access upvalue in main", n->str);
  if (aux_localname(ls->fs, n) >= 0)
    luaX_syntaxerror(ls, "cannot access an upvalue in current scope", n->str);
  luaK_U(ls, PUSHUPVALUE, indexupvalue(ls, n), 1);
}


static void adjust_mult_assign (LexState *ls, int nvars, int nexps) {
  int diff = nexps - nvars;
  if (nexps == 0 || !luaK_lastisopen(ls)) {  /* list is empty or closed */
    /* push or pop eventual difference between list lengths */
    luaK_adjuststack(ls, diff);
  }
  else {  /* list ends in a function call; must correct it */
    diff--;  /* do not count function call itself */
    if (diff <= 0) {  /* more variables than values? */
      /* function call must provide extra values */
      luaK_setcallreturns(ls, -diff);
    }
    else {  /* more values than variables */
      luaK_setcallreturns(ls, 0);  /* call should provide no value */
      luaK_adjuststack(ls, diff);  /* pop eventual extra values */
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
    luaK_deltastack(ls, nparams);
  else {
    luaK_deltastack(ls, nparams+1);
    add_localvar(ls, luaS_newfixed(ls->L, "arg"));
  }
}


static int getvarname (LexState *ls, expdesc *var) {
  switch (var->k) {
    case VGLOBAL:
      return var->info;
    case VLOCAL:
      return string_constant(ls, ls->fs, ls->fs->localvar[var->info]);
      break;
    default:
      error_unexpected(ls);  /* there is no `var name' */
      return 0;  /* to avoid warnings */
  }
}


static void func_onstack (LexState *ls, FuncState *func) {
  TProtoFunc *f = ls->fs->f;
  int i;
  for (i=0; i<func->nupvalues; i++)
    luaK_1tostack(ls, &func->upvalues[i]);
  luaM_growvector(ls->L, f->kproto, f->nkproto, 1, TProtoFunc *,
                  constantEM, MAXARG_A);
  f->kproto[f->nkproto++] = func->f;
  luaK_deltastack(ls, 1);  /* CLOSURE puts one extra element before popping */
  luaK_AB(ls, CLOSURE, f->nkproto-1, func->nupvalues, -func->nupvalues);
}


static void init_state (LexState *ls, FuncState *fs, TaggedString *source) {
  lua_State *L = ls->L;
  TProtoFunc *f = luaF_newproto(ls->L);
  fs->prev = ls->fs;  /* linked list of funcstates */
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
  ttype(L->top) = LUA_T_LPROTO;
  incr_top;
}


static void close_func (LexState *ls) {
  FuncState *fs = ls->fs;
  TProtoFunc *f = fs->f;
  luaK_0(ls, ENDCODE, 0);
  luaM_reallocvector(ls->L, f->code, fs->pc, Instruction);
  luaM_reallocvector(ls->L, f->kstr, f->nkstr, TaggedString *);
  luaM_reallocvector(ls->L, f->knum, f->nknum, real);
  luaM_reallocvector(ls->L, f->kproto, f->nkproto, TProtoFunc *);
  if (fs->nvars != -1) {  /* debug information? */
    luaI_registerlocalvar(ls, NULL, -1);  /* flag end of vector */
    luaM_reallocvector(ls->L, f->locvars, fs->nvars, LocVar);
  }
  ls->fs = fs->prev;
  ls->L->top--;  /* pop function */
}


TProtoFunc *luaY_parser (lua_State *L, ZIO *z) {
  struct LexState lexstate;
  struct FuncState funcstate;
  luaX_setinput(L, &lexstate, z);
  init_state(&lexstate, &funcstate, luaS_new(L, zname(z)));
  next(&lexstate);  /* read first token */
  chunk(&lexstate);
  if (lexstate.token != EOS)
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
    luaK_1tostack(ls, &v);  /* gets only 1 value from previous expression */
    next(ls);  /* skip comma */
    expr(ls, &v);
    n++;
  }
  luaK_tostack(ls, &v);
  return n;
}


static int explist (LexState *ls) {
  /* explist -> [ explist1 ] */
  switch (ls->token) {
    case ELSE: case ELSEIF: case END: case UNTIL:
    case EOS: case ';': case ')':
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
        luaK_setcallreturns(ls, 1);  /* last call returns only 1 value */
#endif
      break;
    }

    case '{':  /* funcargs -> constructor */
      constructor(ls);
      break;

    case STRING:  /* funcargs -> STRING */
      code_string(ls, ls->seminfo.ts);  /* must use `seminfo' before `next' */
      next(ls);
      break;

    default:
      luaK_error(ls, "function arguments expected");
      break;
  }
  fs->stacksize = slevel;  /* call will remove function and arguments */
  luaK_AB(ls, CALL, slevel, MULT_RET, 0);
}


static void var_or_func_tail (LexState *ls, expdesc *v) {
  for (;;) {
    switch (ls->token) {
      case '.':  /* var_or_func_tail -> '.' NAME */
        next(ls);
        luaK_1tostack(ls, v);  /* `v' must be on stack */
        luaK_kstr(ls, checkname(ls));
        v->k = VINDEXED;
        break;

      case '[':  /* var_or_func_tail -> '[' exp1 ']' */
        next(ls);
        luaK_1tostack(ls, v);  /* `v' must be on stack */
        v->k = VINDEXED;
        exp1(ls);
        check(ls, ']');
        break;

      case ':': {  /* var_or_func_tail -> ':' NAME funcargs */
        int name;
        next(ls);
        name = checkname(ls);
        luaK_1tostack(ls, v);  /* `v' must be on stack */
        luaK_U(ls, PUSHSELF, name, 1);
        funcargs(ls, 1);
        v->k = VEXP;
        break;
      }

      case '(': case STRING: case '{':  /* var_or_func_tail -> funcargs */
        luaK_1tostack(ls, v);  /* `v' must be on stack */
        funcargs(ls, 0);
        v->k = VEXP;
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
    case NAME:
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
  int n = 1;  /* one has been read before */
  int mod_n = 1;  /* mod_n == n%RFIELDS_PER_FLUSH */
  while (ls->token == ',') {
    next(ls);
    if (ls->token == ';' || ls->token == '}')
      break;
    recfield(ls);
    n++;
    if (++mod_n == RFIELDS_PER_FLUSH) {
      luaK_U(ls, SETMAP, RFIELDS_PER_FLUSH-1, -2*RFIELDS_PER_FLUSH);
      mod_n = 0;
    }
  }
  if (mod_n)
    luaK_U(ls, SETMAP, mod_n-1, -2*mod_n);
  return n;
}


static int listfields (LexState *ls) {
  /* listfields -> { ',' exp1 } [','] */
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
      luaK_AB(ls, SETLIST, n/LFIELDS_PER_FLUSH - 1, LFIELDS_PER_FLUSH-1,
              -LFIELDS_PER_FLUSH);
      mod_n = 0;
    }
  }
  if (mod_n > 0)
    luaK_AB(ls, SETLIST, n/LFIELDS_PER_FLUSH, mod_n-1, -mod_n);
  return n;
}



static void constructor_part (LexState *ls, constdesc *cd) {
  switch (ls->token) {
    case ';': case '}':  /* constructor_part -> empty */
      cd->n = 0;
      cd->k = ls->token;
      return;

    case NAME: {
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
        luaK_tostack(ls, &v);
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
  int line = ls->linenumber;
  int pc = luaK_U(ls, CREATETABLE, 0, 1);
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
  ls->fs->f->code[pc] = SETARG_U(ls->fs->f->code[pc], nelems);
}

/* }====================================================================== */




/*
** {======================================================================
** Expression parsing
** =======================================================================
*/


static void simpleexp (LexState *ls, expdesc *v) {
  check_debugline(ls);
  switch (ls->token) {
    case NUMBER: {  /* simpleexp -> NUMBER */
      real r = ls->seminfo.r;
      next(ls);
      luaK_number(ls, r);
      break;
    }

    case STRING:  /* simpleexp -> STRING */
      code_string(ls, ls->seminfo.ts);  /* must use `seminfo' before `next' */
      next(ls);
      break;

    case NIL:  /* simpleexp -> NIL */
      luaK_adjuststack(ls, -1);
      next(ls);
      break;

    case '{':  /* simpleexp -> constructor */
      constructor(ls);
      break;

    case FUNCTION:  /* simpleexp -> FUNCTION body */
      next(ls);
      body(ls, 0, ls->linenumber);
      break;

    case '(':  /* simpleexp -> '(' expr ')' */
      next(ls);
      expr(ls, v);
      check(ls, ')');
      return;

    case NAME: case '%':
      var_or_func(ls, v);
      return;

    default:
      luaK_error(ls, "<expression> expected");
      return;
  }
  v->k = VEXP;
}


static void exp1 (LexState *ls) {
  expdesc v;
  expr(ls, &v);
  luaK_1tostack(ls, &v);
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

    case  CONC: *rp = 3; return 4;  /* right associative (?) */

    case EQ: case  NE: case  '>': case  '<': case  LE: case  GE:
      *rp = 2; return 2;

    case AND: case OR: *rp = 1; return 1;

    default: *rp = -1; return -1;
  }
}


/*
** subexpr -> (simplexep | (NOT | '-') subexpr) { binop subexpr }
** where `binop' is any binary operator with a priority higher than `limit'
*/
static void subexpr (LexState *ls, expdesc *v, int limit) {
  int rp;
  if (ls->token == '-' || ls->token == NOT) {
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
  luaK_adjuststack(ls, fs->nlocalvar - nlocalvar);  /* remove local variables */
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
    luaK_U(ls, SETTABLE, left+(nvars-1), -1);
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
  int while_init = luaK_getlabel(ls);
  int loopentry;  /* point to jump to repeat the loop */
  int cond_size;
  int i;
  next(ls);  /* skip WHILE */
  exp1(ls);  /* read condition */
  cond_size = fs->pc - while_init;
  /* save condition (to move it to after body) */
  if (cond_size > MAX_WHILE_EXP)
    luaK_error(ls, "while condition too complex");
  for (i=0; i<cond_size; i++) buffer[i] = fs->f->code[while_init+i];
  /* go back to state prior condition */
  fs->pc = while_init;
  luaK_deltastack(ls, -1);
  luaK_S(ls, JMP, 0, 0);  /* initial jump to condition */
  check(ls, DO);
  loopentry = luaK_getlabel(ls);
  block(ls);
  check_match(ls, END, WHILE, line);
  luaK_fixjump(ls, while_init, luaK_getlabel(ls));
  /* copy condition to new position, and correct stack */
  for (i=0; i<cond_size; i++) luaK_primitivecode(ls, buffer[i]);
  luaK_deltastack(ls, 1);
  luaK_fixjump(ls, luaK_S(ls, IFTJMP, 0, -1), loopentry);
}


static void repeatstat (LexState *ls, int line) {
  /* repeatstat -> REPEAT block UNTIL exp1 */
  int repeat_init = luaK_getlabel(ls);
  next(ls);
  block(ls);
  check_match(ls, UNTIL, REPEAT, line);
  exp1(ls);
  luaK_fixjump(ls, luaK_S(ls, IFFJMP, 0, -1), repeat_init);
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
    luaK_1tostack(ls, v);
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
  expdesc v;
  check_debugline(ls);
  var_or_func(ls, &v);
  if (v.k == VEXP) {  /* stat -> func */
    if (!luaK_lastisopen(ls))  /* is just an upvalue? */
      luaK_error(ls, "syntax error");
    luaK_setcallreturns(ls, 0);  /* call statement uses no results */
  }
  else {  /* stat -> ['%'] NAME assignment */
    int left = assignment(ls, &v, 1);
    luaK_adjuststack(ls, left);  /* remove eventual garbage left on stack */
  }
}


static void ifpart (LexState *ls, int line) {
  /* ifpart -> cond THEN block (ELSEIF ifpart | [ELSE block] END) */
  FuncState *fs = ls->fs;
  int c;  /* address of the conditional jump */
  int elseinit;
  next(ls);  /* skip IF or ELSEIF */
  exp1(ls);  /* cond */
  c = luaK_S(ls, IFFJMP, 0, -1);  /* 1st jump: over `then' part */
  check(ls, THEN);
  block(ls);  /* `then' part */
  luaK_S(ls, JMP, 0, 0);  /* 2nd jump: over `else' part */
  elseinit = luaK_getlabel(ls);  /* address of 2nd jump == elseinit-1 */
  if (ls->token == ELSEIF)
    ifpart(ls, line);
  else {
    if (optional(ls, ELSE))
      block(ls);  /* `else' part */
    check_match(ls, END, IF, line);
  }
  if (fs->pc > elseinit) {  /* is there an `else' part? */
    luaK_fixjump(ls, c, elseinit);  /* fix 1st jump to `else' part */
    luaK_fixjump(ls, elseinit-1, luaK_getlabel(ls));  /* fix 2nd jump */
  }
  else {  /* no else part */
    fs->pc--;  /* remove 2nd jump */
    luaK_fixjump(ls, c, luaK_getlabel(ls));  /* fix 1st jump to `if' end */
  }
}


static int stat (LexState *ls) {
  int line = ls->linenumber;  /* may be needed for error messages */
  switch (ls->token) {
    case IF:  /* stat -> IF ifpart END */
      ifpart(ls, line);
      return 1;

    case WHILE:  /* stat -> whilestat */
      whilestat(ls, line);
      return 1;

    case DO: {  /* stat -> DO block END */
      next(ls);
      block(ls);
      check_match(ls, END, DO, line);
      return 1;
    }

    case REPEAT:  /* stat -> repeatstat */
      repeatstat(ls, line);
      return 1;

    case FUNCTION:  /* stat -> funcstat */
      return funcstat(ls, line);

    case LOCAL:  /* stat -> localstat */
      localstat(ls);
      return 1;

    case NAME: case '%':  /* stat -> namestat */
      namestat(ls);
      return 1;

    case RETURN: case ';': case ELSE: case ELSEIF:
    case END: case UNTIL: case EOS:  /* `stat' follow */
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
    case DOTS:  /* parlist -> DOTS */
      next(ls);
      dots = 1;
      break;

    case NAME:  /* parlist, tailparlist -> NAME [',' tailparlist] */
      init:
      store_localvar(ls, str_checkname(ls), nparams++);
      if (ls->token == ',') {
        next(ls);
        switch (ls->token) {
          case DOTS:  /* tailparlist -> DOTS */
            next(ls);
            dots = 1;
            break;

          case NAME:  /* tailparlist -> NAME [',' tailparlist] */
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
  check_match(ls, END, FUNCTION, line);
  close_func(ls);
  func_onstack(ls, &new_fs);
}


static void ret (LexState *ls) {
  /* ret -> [RETURN explist sc] */
  if (ls->token == RETURN) {
    int nexps;  /* number of expressions returned */
    check_debugline(ls);
    next(ls);
    nexps = explist(ls);
    luaK_retcode(ls, ls->fs->nlocalvar, nexps);
    ls->fs->stacksize = ls->fs->nlocalvar;  /* removes all temp values */
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

