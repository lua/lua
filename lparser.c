/*
** $Id: lparser.c,v 1.176 2002/04/10 19:14:45 roberto Exp roberto $
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
** nodes for block list (list of active blocks)
*/
typedef struct BlockCnt {
  struct BlockCnt *previous;  /* chain */
  int breaklist;  /* list of jumps out of this loop */
  int nactloc;  /* # active local variables outside the breakable structure */
  int nactvar;
  int defaultglob;
  int upval;  /* true if some variable in the block is an upvalue */
  int isbreakable;  /* true if `block' is a loop */
} BlockCnt;



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


#define check_condition(ls,c,msg)	{ if (!(c)) luaK_error(ls, msg); }


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
  e->info = i;
}


static void codestring (LexState *ls, expdesc *e, TString *s) {
  init_exp(e, VK, luaK_stringK(ls->fs, s));
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


static vardesc *new_var (LexState *ls, int n) {
  FuncState *fs = ls->fs;
  luaX_checklimit(ls, fs->nactvar+n+1, MAXVARS, "variables");
  return &fs->actvar[fs->nactvar+n];
}


static void new_localvar (LexState *ls, TString *name, int n) {
  vardesc *v = new_var(ls, n);  
  v->k = VLOCAL;
  v->i = luaI_registerlocalvar(ls, name);
  v->level = ls->fs->nactloc + n;
}


static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  while (nvars--) {
    lua_assert(fs->actvar[fs->nactvar].k == VLOCAL);
    fs->f->locvars[fs->actvar[fs->nactvar].i].startpc = fs->pc;
    fs->nactvar++;
    fs->nactloc++;
  }
}


static void adjustglobalvars (LexState *ls, int nvars, int level) {
  FuncState *fs = ls->fs;
  while (nvars--) {
    fs->actvar[fs->nactvar].k = VGLOBAL;
    fs->actvar[fs->nactvar].level = level;
    fs->nactvar++;
  }
}


static void removevars (LexState *ls, int tolevel) {
  FuncState *fs = ls->fs;
  while (fs->nactvar > tolevel) {
    fs->nactvar--;
    if (fs->actvar[fs->nactvar].k == VLOCAL) {
      fs->nactloc--;
      fs->f->locvars[fs->actvar[fs->nactvar].i].endpc = fs->pc;
    }
  }
}


static void new_localvarstr (LexState *ls, const char *name, int n) {
  new_localvar(ls, luaS_new(ls->L, name), n);
}


static void create_local (LexState *ls, const char *name) {
  new_localvarstr(ls, name, 0);
  adjustlocalvars(ls, 1);
}


static int indexupvalue (FuncState *fs, expdesc *v) {
  int i;
  for (i=0; i<fs->f->nupvalues; i++) {
    if (fs->upvalues[i].k == v->k && fs->upvalues[i].info == v->info)
      return i;
  }
  /* new one */
  luaX_checklimit(fs->ls, fs->f->nupvalues+1, MAXUPVALUES, "upvalues");
  fs->upvalues[fs->f->nupvalues] = *v;
  return fs->f->nupvalues++;
}


static vardesc *searchvar (FuncState *fs, TString *n) {
  int i;
  for (i=fs->nactvar-1; i >= 0; i--) {
    vardesc *v = &fs->actvar[i];
    if (v->k == VLOCAL ? n == fs->f->locvars[v->i].varname
                       : n == tsvalue(&fs->f->k[v->i]))
      return v;
  }
  return NULL;  /* not found */
}


static void markupval (FuncState *fs, int level) {
  BlockCnt *bl = fs->bl;
  while (bl && bl->nactloc > level) bl = bl->previous;
  if (bl) bl->upval = 1;
}


static int singlevar_aux (FuncState *fs, TString *n, expdesc *var, int nd) {
  if (fs == NULL) {  /* no more levels? */
    init_exp(var, VGLOBAL, NO_REG);  /* default is free global */
    return VNIL;  /* not found */
  }
  else {
    vardesc *v = searchvar(fs, n);  /* look up at current level */
    if (v) {
      if (v->level == NO_REG) {  /* free global? */
        lua_assert(v->k == VGLOBAL);
        init_exp(var, VGLOBAL, NO_REG);
      }
      else
        init_exp(var, VLOCAL, v->level);
      return v->k;
    }
    else {  /* not found at current level; try upper one */
      int k = singlevar_aux(fs->prev, n, var, nd && fs->defaultglob == NO_REG);
      if (var->k == VGLOBAL) {
        if (k == VNIL && nd && fs->defaultglob != NO_REG) {
          if (fs->defaultglob == NO_REG1)
            luaK_error(fs->ls, "undeclared global");
          init_exp(var, VLOCAL, fs->defaultglob);
          k = VGLOBAL;  /* now there is a declaration */
        }
      }
      else {  /* LOCAL or UPVAL */
        if (var->k == VLOCAL)
          markupval(fs->prev, var->info);  /* local will be used as an upval */
        var->info = indexupvalue(fs, var);
        var->k = VUPVAL;  /* upvalue in this level */
      }
      return k;
    }
  }
}


static void singlevar (FuncState *fs, TString *n, expdesc *var) {
  int k = singlevar_aux(fs, n, var, 1);
  if (k == VNIL || k == VGLOBAL) {  /* global? */
    if (var->k == VGLOBAL)  /* free global? */
      var->info = luaK_stringK(fs, n);
    else {  /* `indexed' global */
      expdesc e;
      codestring(fs->ls, &e, n);
      luaK_exp2anyreg(fs, var);
      var->aux = luaK_exp2RK(fs, &e);
      var->k = VINDEXED;
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


static void code_params (LexState *ls, int nparams, int dots) {
  FuncState *fs = ls->fs;
  adjustlocalvars(ls, nparams);
  luaX_checklimit(ls, fs->nactvar, MAXPARAMS, "parameters");
  fs->f->numparams = cast(lu_byte, fs->nactloc);
  fs->f->is_vararg = cast(lu_byte, dots);
  if (dots)
    create_local(ls, "arg");
  luaK_reserveregs(fs, fs->nactloc);  /* reserve register for parameters */
}


static void enterblock (FuncState *fs, BlockCnt *bl, int isbreakable) {
  bl->breaklist = NO_JUMP;
  bl->isbreakable = isbreakable;
  bl->nactloc = fs->nactloc;
  bl->nactvar = fs->nactvar;
  bl->defaultglob = fs->defaultglob;
  bl->upval = 0;
  bl->previous = fs->bl;
  fs->bl = bl;
  lua_assert(fs->freereg == fs->nactloc);
}


static void leaveblock (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  fs->bl = bl->previous;
  removevars(fs->ls, bl->nactvar);
  if (bl->upval)
    luaK_codeABC(fs, OP_CLOSE, bl->nactloc, 0, 0);
  lua_assert(bl->nactloc == fs->nactloc);
  lua_assert(bl->nactvar == fs->nactvar);
  fs->freereg = fs->nactloc;  /* free registers */
  fs->defaultglob = bl->defaultglob;
  luaK_patchtohere(fs, bl->breaklist);
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
  fs->nlocvars = 0;
  fs->nactloc = 0;
  fs->nactvar = 0;
  fs->defaultglob = NO_REG;  /* default is free globals */
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
  removevars(ls, 0);
  luaK_codeABC(fs, OP_RETURN, 0, 1, 0);  /* final return */
  luaK_getlabel(fs);  /* close eventual list of pending jumps */
  lua_assert(G(L)->roottable == fs->h);
  G(L)->roottable = fs->h->next;
  luaH_free(L, fs->h);
  luaM_reallocvector(L, f->code, f->sizecode, fs->pc, Instruction);
  luaM_reallocvector(L, f->lineinfo, f->sizecode, fs->pc, int);
  f->sizecode = fs->pc;
  luaM_reallocvector(L, f->k, f->sizek, fs->nk, TObject);
  f->sizek = fs->nk;
  luaM_reallocvector(L, f->p, f->sizep, fs->np, Proto *);
  f->sizep = fs->np;
  luaM_reallocvector(L, f->locvars, f->sizelocvars, fs->nlocvars, LocVar);
  f->sizelocvars = fs->nlocvars;
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
  check_condition(&lexstate, (lexstate.t.token == TK_EOS), "<eof> expected");
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
  int line = ls->linenumber;
  switch (ls->t.token) {
    case '(': {  /* funcargs -> `(' [ explist1 ] `)' */
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
      return;
    }
  }
  lua_assert(f->k == VNONRELOC);
  base = f->info;  /* base register for call */
  if (args.k == VCALL)
    nparams = LUA_MULTRET;  /* open call */
  else {
    if (args.k != VVOID)
      luaK_exp2nextreg(fs, &args);  /* close last argument */
    nparams = fs->freereg - (base+1);
  }
  init_exp(f, VCALL, luaK_codeABC(fs, OP_CALL, base, nparams+1, 2));
  fs->f->lineinfo[f->info] = line;
  fs->freereg = base+1;  /* call remove function and arguments and leaves
                            (unless changed) one result */
}



/*
** {======================================================================
** Rules for Constructors
** =======================================================================
*/


struct ConsControl {
  expdesc v;  /* last list item read */
  expdesc *t;  /* table descriptor */
  int nh;  /* total number of `record' elements */
  int na;  /* total number of array elements */
  int tostore;  /* number of array elements pending to be stored */
};


static void recfield (LexState *ls, struct ConsControl *cc) {
  /* recfield -> (NAME | `['exp1`]') = exp1 */
  FuncState *fs = ls->fs;
  int reg = ls->fs->freereg;
  expdesc key, val;
  if (ls->t.token == TK_NAME) {
    luaX_checklimit(ls, cc->nh, MAX_INT, "items in a constructor");
    cc->nh++;
    checkname(ls, &key);
  }
  else  /* ls->t.token == '[' */
    luaY_index(ls, &key);
  check(ls, '=');
  luaK_exp2RK(fs, &key);
  expr(ls, &val);
  luaK_exp2anyreg(fs, &val);
  luaK_codeABC(fs, OP_SETTABLE, val.info, cc->t->info, luaK_exp2RK(fs, &key));
  fs->freereg = reg;  /* free registers */
}


static void closelistfield (FuncState *fs, struct ConsControl *cc) {
  if (cc->v.k == VVOID) return;  /* there is no list item */
  luaK_exp2nextreg(fs, &cc->v);
  cc->v.k = VVOID;
  if (cc->tostore == LFIELDS_PER_FLUSH) {
    luaK_codeABc(fs, OP_SETLIST, cc->t->info, cc->na-1);  /* flush */
    cc->tostore = 0;  /* no more items pending */
    fs->freereg = cc->t->info + 1;  /* free registers */
  }
}


static void lastlistfield (FuncState *fs, struct ConsControl *cc) {
  if (cc->tostore == 0) return;
  if (cc->v.k == VCALL) {
    luaK_setcallreturns(fs, &cc->v, LUA_MULTRET);
    luaK_codeABc(fs, OP_SETLISTO, cc->t->info, cc->na-1);
  }
  else {
    if (cc->v.k != VVOID)
      luaK_exp2nextreg(fs, &cc->v);
    luaK_codeABc(fs, OP_SETLIST, cc->t->info, cc->na-1);
  }
  fs->freereg = cc->t->info + 1;  /* free registers */
}


static void listfield (LexState *ls, struct ConsControl *cc) {
  expr(ls, &cc->v);
  luaX_checklimit(ls, cc->na, MAXARG_Bc, "items in a constructor");
  cc->na++;
  cc->tostore++;
}


static void constructor (LexState *ls, expdesc *t) {
  /* constructor -> ?? */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  struct ConsControl cc;
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VRELOCABLE, pc);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  luaK_exp2nextreg(ls->fs, t);  /* fix it at stack top (for gc) */
  check(ls, '{');
  for (;;) {
    lua_assert(cc.v.k == VVOID || cc.tostore > 0);
    optional(ls, ';');  /* compatibility only */
    if (ls->t.token == '}') break;
    closelistfield(fs, &cc);
    switch(ls->t.token) {
      case TK_NAME: {  /* may be listfields or recfields */
        lookahead(ls);
        if (ls->lookahead.token != '=')  /* expression? */
          listfield(ls, &cc);
        else
          recfield(ls, &cc);
        break;
      }
      case '[': {  /* constructor_item -> recfield */
        recfield(ls, &cc);
        break;
      }
      default: {  /* constructor_part -> listfield */
        listfield(ls, &cc);
        break;
      }
    }
    if (ls->t.token == ',' || ls->t.token == ';')
      next(ls);
    else
      break;
  }
  check_match(ls, '}', '{', line);
  lastlistfield(fs, &cc);
  if (cc.na > 0)
    SETARG_B(fs->f->code[pc], luaO_log2(cc.na-1)+2); /* set initial table size */
  SETARG_C(fs->f->code[pc], luaO_log2(cc.nh)+1);  /* set initial table size */
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
      singlevar(ls->fs, str_checkname(ls), v);
      next(ls);
      return;
    }
    case '%': {  /* for compatibility only */
      next(ls);  /* skip `%' */
      singlevar(ls->fs, str_checkname(ls), v);
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
      init_exp(v, VK, luaK_numberK(ls->fs, ls->t.seminfo.r));
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
    case TK_TRUE: {
      init_exp(v, VTRUE, 0);
      next(ls);
      break;
    }
    case TK_FALSE: {
      init_exp(v, VFALSE, 0);
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
  BlockCnt bl;
  enterblock(fs, &bl, 0);
  chunk(ls);
  lua_assert(bl.breaklist == NO_JUMP);
  leaveblock(fs);
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
      if (lh->v.info == v->info) {  /* conflict? */
        conflict = 1;
        lh->v.info = extra;  /* previous assignment will use safe copy */
      }
      if (lh->v.aux == v->info) {  /* conflict? */
        conflict = 1;
        lh->v.aux = extra;  /* previous assignment will use safe copy */
      }
    }
  }
  if (conflict) {
    luaK_codeABC(fs, OP_MOVE, fs->freereg, v->info, 0);  /* make copy */
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
      luaK_setcallreturns(ls->fs, &e, 1);  /* close last expression */
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
  BlockCnt bl;
  enterblock(fs, &bl, 1);
  next(ls);
  cond(ls, &v);
  check(ls, TK_DO);
  block(ls);
  luaK_patchlist(fs, luaK_jump(fs), while_init);
  luaK_patchtohere(fs, v.f);
  check_match(ls, TK_END, TK_WHILE, line);
  leaveblock(fs);
}


static void repeatstat (LexState *ls, int line) {
  /* repeatstat -> REPEAT block UNTIL cond */
  FuncState *fs = ls->fs;
  int repeat_init = luaK_getlabel(fs);
  expdesc v;
  BlockCnt bl;
  enterblock(fs, &bl, 1);
  next(ls);
  block(ls);
  check_match(ls, TK_UNTIL, TK_REPEAT, line);
  cond(ls, &v);
  luaK_patchlist(fs, v.f, repeat_init);
  leaveblock(fs);
}


static int exp1 (LexState *ls) {
  expdesc e;
  int k;
  expr(ls, &e);
  k = e.k;
  luaK_exp2nextreg(ls->fs, &e);
  return k;
}


static void fornum (LexState *ls, TString *varname, int line) {
  /* fornum -> NAME = exp1,exp1[,exp1] DO body */
  FuncState *fs = ls->fs;
  int prep, endfor;
  int base = fs->freereg;
  new_localvar(ls, varname, 0);
  new_localvarstr(ls, "(for limit)", 1);
  new_localvarstr(ls, "(for step)", 2);
  check(ls, '=');
  exp1(ls);  /* initial value */
  check(ls, ',');
  exp1(ls);  /* limit */
  if (optional(ls, ','))
    exp1(ls);  /* optional step */
  else {  /* default step = 1 */
    luaK_codeABc(fs, OP_LOADK, fs->freereg, luaK_numberK(fs, 1));
    luaK_reserveregs(fs, 1);
  }
  adjustlocalvars(ls, 3);  /* scope for control variables */
  luaK_codeABC(fs, OP_SUB, fs->freereg - 3, fs->freereg - 3, fs->freereg - 1);
  luaK_jump(fs);
  prep = luaK_getlabel(fs);
  check(ls, TK_DO);
  block(ls);
  luaK_patchtohere(fs, prep-1);
  endfor = luaK_codeAsBc(fs, OP_FORLOOP, base, NO_JUMP);
  luaK_patchlist(fs, endfor, prep);
  fs->f->lineinfo[endfor] = line;  /* pretend that `OP_FOR' starts the loop */
}


static void forlist (LexState *ls, TString *indexname) {
  /* forlist -> NAME {,NAME} IN explist1 DO body */
  FuncState *fs = ls->fs;
  expdesc e;
  int nvars = 0;
  int prep;
  int base = fs->freereg;
  new_localvarstr(ls, "(for generator)", nvars++);
  new_localvarstr(ls, "(for state)", nvars++);
  new_localvar(ls, indexname, nvars++);
  while (optional(ls, ',')) {
    new_localvar(ls, str_checkname(ls), nvars++);
    next(ls);
  }
  check(ls, TK_IN);
  adjust_assign(ls, 3, explist1(ls, &e), &e);
  luaK_reserveregs(fs, nvars - 3);  /* registers for other variables */
  luaK_codeABC(fs, OP_TFORPREP, base, 0, 0);
  luaK_codeABC(fs, OP_TFORLOOP, base, 0, nvars - 3);
  adjustlocalvars(ls, nvars);  /* scope for all variables */
  prep = luaK_jump(fs);
  check(ls, TK_DO);
  block(ls);
  luaK_patchlist(fs, luaK_jump(fs), prep-1);
  luaK_patchtohere(fs, prep);
}


static void forstat (LexState *ls, int line) {
  /* forstat -> fornum | forlist */
  FuncState *fs = ls->fs;
  TString *varname;
  BlockCnt bl;
  enterblock(fs, &bl, 1);
  next(ls);  /* skip `for' */
  varname = str_checkname(ls);  /* first variable name */
  next(ls);  /* skip var name */
  switch (ls->t.token) {
    case '=': fornum(ls, varname, line); break;
    case ',': case TK_IN: forlist(ls, varname); break;
    default: luaK_error(ls, "`=' or `in' expected");
  }
  check_match(ls, TK_END, TK_FOR, line);
  leaveblock(fs);
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
    luaK_patchtohere(fs, v.f);
    test_then_block(ls, &v);  /* ELSEIF cond THEN block */
  }
  if (ls->t.token == TK_ELSE) {
    luaK_concat(fs, &escapelist, luaK_jump(fs));
    luaK_patchtohere(fs, v.f);
    next(ls);  /* skip ELSE */
    block(ls);  /* `else' part */
  }
  else
    luaK_concat(fs, &escapelist, v.f);
  luaK_patchtohere(fs, escapelist);
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


static void globalstat (LexState *ls) {
  /* stat -> GLOBAL NAME {`,' NAME} [IN exp] | GLOBAL IN exp */
  FuncState *fs = ls->fs;
  int nvars = 0;
  next(ls);  /* skip GLOBAL */
  if (ls->t.token == TK_NAME) {
    do {
      vardesc *v = new_var(ls, nvars++);  
      v->i = luaK_stringK(ls->fs, str_checkname(ls));
      next(ls);  /* skip name */
    } while (optional(ls, ','));
  }
  if (!optional(ls, TK_IN)) {  /* free globals? */
    if (nvars == 0)  /* default - free is invalid */
      error_expected(ls, TK_IN);
    adjustglobalvars(ls, nvars, NO_REG);  /* mark globals as free */
  }
  else {
    int baselocal = fs->freereg;
    int k = exp1(ls);
    if (nvars == 0)
      fs->defaultglob = (k == VNIL) ? NO_REG1 : baselocal;
    adjustglobalvars(ls, nvars, baselocal);
    create_local(ls, "*");
  }
}


static int funcname (LexState *ls, expdesc *v) {
  /* funcname -> NAME {field} [`:' NAME] */
  int needself = 0;
  singlevar(ls->fs, str_checkname(ls), v);
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
    nret = explist1(ls, &e);  /* optional return values */
    if (e.k == VCALL) {
      if (nret == 1) {  /* tail call? */
        SET_OPCODE(getcode(fs,&e), OP_TAILCALL);
        return;
      }
      luaK_setcallreturns(fs, &e, LUA_MULTRET);
      first = fs->nactloc;
      nret = LUA_MULTRET;  /* return all values */
    }
    else {
      if (nret == 1)  /* only one single value? */
        first = luaK_exp2anyreg(fs, &e);
      else {
        luaK_exp2nextreg(fs, &e);  /* values must go to the `stack' */
        first = fs->nactloc;  /* return all `active' values */
        lua_assert(nret == fs->freereg - first);
      }
    }
  }
  luaK_codeABC(fs, OP_RETURN, first, nret+1, 0);
}


static void breakstat (LexState *ls) {
  /* stat -> BREAK [NAME] */
  FuncState *fs = ls->fs;
  BlockCnt *bl = fs->bl;
  int upval = 0;
  next(ls);  /* skip BREAK */
  while (bl && !bl->isbreakable) {
    upval |= bl->upval;
    bl = bl->previous;
  }
  if (!bl)
    luaK_error(ls, "no loop to break");
  if (upval)
    luaK_codeABC(fs, OP_CLOSE, bl->nactloc, 0, 0);
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
    case TK_GLOBAL: {  /* stat -> globalstat */
      globalstat(ls);
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
  int dots = 0;
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
  if (needself)
    create_local(ls, "self");
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

