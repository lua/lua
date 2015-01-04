/*
** $Id: lparser.c,v 1.37a 1999/06/17 17:04:03 roberto Exp $
** LL(1) Parser and code generator for Lua
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#include "lauxlib.h"
#include "ldo.h"
#include "lfunc.h"
#include "llex.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "lua.h"
#include "luadebug.h"
#include "lzio.h"



/* size of a "normal" jump instruction: OpCode + 1 byte */
#define JMPSIZE	2

/* maximum number of local variables */
#ifndef MAXLOCALS
#define MAXLOCALS 200  /* arbitrary limit (<256) */
#endif


/* maximum number of upvalues */
#ifndef MAXUPVALUES
#define MAXUPVALUES 32  /* arbitrary limit (<256) */
#endif


/* maximum number of variables in the left side of an assignment */
#ifndef MAXVARSLH
#define MAXVARSLH	100  /* arbitrary limit (<255) */
#endif


/* maximum number of parameters in a function */
#ifndef MAXPARAMS
#define MAXPARAMS	100  /* arbitrary limit (<ZEROVARARG) */
#endif


/*
** Variable descriptor:
** must include an "exp" option because LL(1) cannot distinguish
** between variables, upvalues and function calls on first sight.
*/
typedef enum {
  VGLOBAL,  /* info is constant index of global name */
  VLOCAL,   /* info is stack index */
  VDOT,     /* info is constant index of index name */
  VINDEXED, /* no info (table and index are on the stack) */
  VEXP      /* info is pc index of "nparam" of a call (or 0 if exp is closed) */
} varkind;

typedef struct vardesc {
  varkind k;
  int info;
} vardesc;


/*
** Expression List descriptor:
** tells number of expressions in the list,
** and, if last expression is open (a function call),
** where is its pc index of "nparam"
*/
typedef struct listdesc {
  int n;
  int pc;  /* 0 if last expression is closed */
} listdesc;


/*
** Constructors descriptor:
** "n" indicates number of elements, and "k" signals whether
** it is a list constructor (k = 0) or a record constructor (k = 1)
** or empty (k = ';' or '}')
*/
typedef struct constdesc {
  int n;
  int k;
} constdesc;


/* state needed to generate code for a given function */
typedef struct FuncState {
  TProtoFunc *f;  /* current function header */
  struct FuncState *prev;  /* enclosuring function */
  int pc;  /* next position to code */
  int stacksize;  /* number of values on activation register */
  int maxstacksize;  /* maximum number of values on activation register */
  int nlocalvar;  /* number of active local variables */
  int nupvalues;  /* number of upvalues */
  int nvars;  /* number of entries in f->locvars (-1 if no debug information) */
  int lastsetline;  /* line where last SETLINE was issued */
  vardesc upvalues[MAXUPVALUES];  /* upvalues */
  TaggedString *localvar[MAXLOCALS];  /* store local variable names */
} FuncState;


/*
** prototypes for non-terminal functions
*/
static int assignment (LexState *ls, vardesc *v, int nvars);
static int cond (LexState *ls);
static int funcname (LexState *ls, vardesc *v);
static int funcparams (LexState *ls, int slf);
static int listfields (LexState *ls);
static int localnamelist (LexState *ls);
static int optional (LexState *ls, int c);
static int recfields (LexState *ls);
static int stat (LexState *ls);
static void block (LexState *ls);
static void body (LexState *ls, int needself, int line);
static void chunk (LexState *ls);
static void constructor (LexState *ls);
static void decinit (LexState *ls, listdesc *d);
static void exp0 (LexState *ls, vardesc *v);
static void exp1 (LexState *ls);
static void exp2 (LexState *ls, vardesc *v);
static void explist (LexState *ls, listdesc *e);
static void explist1 (LexState *ls, listdesc *e);
static void ifpart (LexState *ls, int line);
static void parlist (LexState *ls);
static void part (LexState *ls, constdesc *cd);
static void recfield (LexState *ls);
static void ret (LexState *ls);
static void statlist (LexState *ls);
static void var_or_func (LexState *ls, vardesc *v);
static void var_or_func_tail (LexState *ls, vardesc *v);



static void checklimit (LexState *ls, int val, int limit, char *msg) {
  if (val > limit) {
    char buff[100];
    sprintf(buff, "too many %s (limit=%d)", msg, limit);
    luaX_error(ls, buff);
  }
}


static void check_pc (FuncState *fs, int n) {
  luaM_growvector(fs->f->code, fs->pc, n, Byte, codeEM, MAX_INT);
}


static void code_byte (FuncState *fs, Byte c) {
  check_pc(fs, 1);
  fs->f->code[fs->pc++] = c;
}


static void deltastack (LexState *ls, int delta) {
  FuncState *fs = ls->fs;
  fs->stacksize += delta;
  if (fs->stacksize > fs->maxstacksize) {
    if (fs->stacksize > MAX_BYTE)
      luaX_error(ls, "function/expression too complex");
    fs->maxstacksize = fs->stacksize;
  }
}


static void code_oparg_at (LexState *ls, int pc, OpCode op,
                           int arg, int delta) {
  Byte *code = ls->fs->f->code;
  deltastack(ls, delta);
  if (arg <= MAX_BYTE) {
    code[pc] = (Byte)op;
    code[pc+1] = (Byte)arg;
  }
  else if (arg > MAX_ARG)
    luaX_error(ls, "code too long");
  else {  /* MAX_BYTE < arg < MAX_ARG */
    if (arg > MAX_WORD) {
      code[pc] = (Byte)LONGARG;
      code[pc+1] = (Byte)(arg>>16);
      pc += 2;
    }
    code[pc] = (Byte)(op-1);  /* opcode for word argument */
    code[pc+1] = (Byte)((arg&0xFFFF)>>8);
    code[pc+2] = (Byte)(arg&0xFF);
  }
}


static int codesize (int arg) {
  if      (arg <= MAX_BYTE) return 2;  /* opcode + 1 byte */
  else if (arg <= MAX_WORD) return 3;  /* opcode + 1 word (2 bytes) */
  else return 5;    /* LONGARG + 1 byte + opcode + 1 word (2 bytes) */
}


static int fix_opcode (LexState *ls, int pc, OpCode op, int arg) {
  int tomove = codesize(arg)-2;
  if (tomove > 0) {  /* need to open space? */
    FuncState *fs = ls->fs;
    TProtoFunc *f = fs->f;
    check_pc(fs, tomove);
    luaO_memup(f->code+pc+tomove, f->code+pc, fs->pc-pc);
    fs->pc += tomove;
  }
  code_oparg_at(ls, pc, op, arg, 0);
  return tomove;
}


static void code_oparg (LexState *ls, OpCode op, int arg, int delta) {
  int size = codesize(arg);
  check_pc(ls->fs, size);
  code_oparg_at(ls, ls->fs->pc, op, arg, delta);
  ls->fs->pc += size;
}


static void code_opcode (LexState *ls, OpCode op, int delta) {
  deltastack(ls, delta);
  code_byte(ls->fs, (Byte)op);
}


static void code_constant (LexState *ls, int c) {
  code_oparg(ls, PUSHCONSTANT, c, 1);
}


static int next_constant (FuncState *fs) {
  TProtoFunc *f = fs->f;
  luaM_growvector(f->consts, f->nconsts, 1, TObject, constantEM, MAX_ARG);
  return f->nconsts++;
}


static int string_constant (FuncState *fs, TaggedString *s) {
  TProtoFunc *f = fs->f;
  int c = s->constindex;
  if (!(c < f->nconsts &&
      ttype(&f->consts[c]) == LUA_T_STRING && tsvalue(&f->consts[c]) == s)) {
    c = next_constant(fs);
    ttype(&f->consts[c]) = LUA_T_STRING;
    tsvalue(&f->consts[c]) = s;
    s->constindex = c;  /* hint for next time */
  }
  return c;
}


static void code_string (LexState *ls, TaggedString *s) {
  code_constant(ls, string_constant(ls->fs, s));
}


#define LIM 20
static int real_constant (FuncState *fs, real r) {
  /* check whether 'r' has appeared within the last LIM entries */
  TObject *cnt = fs->f->consts;
  int c = fs->f->nconsts;
  int lim = c < LIM ? 0 : c-LIM;
  while (--c >= lim) {
    if (ttype(&cnt[c]) == LUA_T_NUMBER && nvalue(&cnt[c]) == r)
      return c;
  }
  /* not found; create a new entry */
  c = next_constant(fs);
  cnt = fs->f->consts;  /* 'next_constant' may have reallocated this vector */
  ttype(&cnt[c]) = LUA_T_NUMBER;
  nvalue(&cnt[c]) = r;
  return c;
}


static void code_number (LexState *ls, real f) {
  real af = (f<0) ? -f : f;
  if (0 <= af && af <= (real)MAX_WORD && (int)af == af) {
    /* abs(f) has a short integer value */
    code_oparg(ls, (f<0) ? PUSHNUMBERNEG : PUSHNUMBER, (int)af, 1);
  }
  else
    code_constant(ls, real_constant(ls->fs, f));
}


static void flush_record (LexState *ls, int n) {
  if (n > 0)
    code_oparg(ls, SETMAP, n-1, -2*n);
}


static void flush_list (LexState *ls, int m, int n) {
  if (n > 0) {
    code_oparg(ls, SETLIST, m, -n);
    code_byte(ls->fs, (Byte)n);
  }
}


static void luaI_registerlocalvar (FuncState *fs, TaggedString *varname,
                                   int line) {
  if (fs->nvars != -1) {  /* debug information? */
    TProtoFunc *f = fs->f;
    luaM_growvector(f->locvars, fs->nvars, 1, LocVar, "", MAX_INT);
    f->locvars[fs->nvars].varname = varname;
    f->locvars[fs->nvars].line = line;
    fs->nvars++;
  }
}


static void luaI_unregisterlocalvar (FuncState *fs, int line) {
  luaI_registerlocalvar(fs, NULL, line);
}


static void store_localvar (LexState *ls, TaggedString *name, int n) {
  FuncState *fs = ls->fs;
  checklimit(ls, fs->nlocalvar+n+1, MAXLOCALS, "local variables");
  fs->localvar[fs->nlocalvar+n] = name;
  luaI_registerlocalvar(fs, name, ls->linenumber);
}


static void add_localvar (LexState *ls, TaggedString *name) {
  store_localvar(ls, name, 0);
  ls->fs->nlocalvar++;
}


static void correctvarlines (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  if (fs->nvars != -1) {  /* debug information? */
    for (; nvars; nvars--) { /* correct line information */
      fs->f->locvars[fs->nvars-nvars].line = fs->lastsetline; 
    }
  }
}


static int aux_localname (FuncState *fs, TaggedString *n) {
  int i;
  for (i=fs->nlocalvar-1; i >= 0; i--)
    if (n == fs->localvar[i]) return i;  /* local var index */
  return -1;  /* not found */
}


static void singlevar (LexState *ls, TaggedString *n, vardesc *var, int prev) {
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
    var->info = string_constant(fs, n);
  }
}


static int indexupvalue (LexState *ls, TaggedString *n) {
  FuncState *fs = ls->fs;
  vardesc v;
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
  code_oparg(ls, PUSHUPVALUE, indexupvalue(ls, n), 1);
}



static void check_debugline (LexState *ls) {
  if (L->debug && ls->linenumber != ls->fs->lastsetline) {
    code_oparg(ls, SETLINE, ls->linenumber, 0);
    ls->fs->lastsetline = ls->linenumber;
  }
}


static void adjuststack (LexState *ls, int n) {
  if (n > 0)
    code_oparg(ls, POP, n, -n);
  else if (n < 0)
    code_oparg(ls, PUSHNIL, (-n)-1, -n);
}


static void close_exp (LexState *ls, int pc, int nresults) {
  if (pc > 0) {  /* expression is an open function call? */
    Byte *code = ls->fs->f->code;
    code[pc-1] = (Byte)nresults;  /* set nresults */
    /* push results, pop params (at code[pc]) and function */
    deltastack(ls, nresults-(code[pc]+1));
  }
#ifdef DEBUG
  code_oparg(ls, CHECKSTACK, ls->fs->stacksize, 0);
#endif
}


static void adjust_mult_assign (LexState *ls, int nvars, listdesc *d) {
  int diff = d->n - nvars;
  if (d->pc == 0) {  /* list is closed */
    /* push or pop eventual difference between list lengths */
    adjuststack(ls, diff);
  }
  else {  /* must correct function call */
    diff--;  /* do not count function call itself */
    if (diff <= 0) {  /* more variables than values? */
      /* function call must provide extra values */
      close_exp(ls, d->pc, -diff);
    }
    else {  /* more values than variables */
      close_exp(ls, d->pc, 0);  /* call should provide no value */
      adjuststack(ls, diff);  /* pop eventual extra values */
    }
  }
}


static void code_args (LexState *ls, int nparams, int dots) {
  FuncState *fs = ls->fs;
  fs->nlocalvar += nparams;  /* "self" may already be there */
  checklimit(ls, fs->nlocalvar, MAXPARAMS, "parameters");
  nparams = fs->nlocalvar;
  if (!dots) {
    fs->f->code[1] = (Byte)nparams;  /* fill-in arg information */
    deltastack(ls, nparams);
  }
  else {
    fs->f->code[1] = (Byte)(nparams+ZEROVARARG);
    deltastack(ls, nparams+1);
    add_localvar(ls, luaS_new("arg"));
  }
}


static void unloaddot (LexState *ls, vardesc *v) {
  /* dotted variables <a.x> must be stored like regular indexed vars <a["x"]> */
  if (v->k == VDOT) {
    code_constant(ls, v->info);
    v->k = VINDEXED;
  }
}


static void lua_pushvar (LexState *ls, vardesc *var) {
  switch (var->k) {
    case VLOCAL:
      code_oparg(ls, PUSHLOCAL, var->info, 1);
      break;
    case VGLOBAL:
      code_oparg(ls, GETGLOBAL, var->info, 1);
      break;
    case VDOT:
      code_oparg(ls, GETDOTTED, var->info, 0);
      break;
    case VINDEXED:
      code_opcode(ls, GETTABLE, -1);
      break;
    case VEXP:
      close_exp(ls, var->info, 1);  /* function must return 1 value */
      break;
  }
  var->k = VEXP;
  var->info = 0;  /* now this is a closed expression */
}


static void storevar (LexState *ls, vardesc *var) {
  switch (var->k) {
    case VLOCAL:
      code_oparg(ls, SETLOCAL, var->info, -1);
      break;
    case VGLOBAL:
      code_oparg(ls, SETGLOBAL, var->info, -1);
      break;
    case VINDEXED:
      code_opcode(ls, SETTABLEPOP, -3);
      break;
    default:
      LUA_INTERNALERROR("invalid var kind to store");
  }
}


static int fix_jump (LexState *ls, int pc, OpCode op, int n) {
  /* jump is relative to position following jump instruction */
  return fix_opcode(ls, pc, op, n-(pc+JMPSIZE));
}


static void fix_upjmp (LexState *ls, OpCode op, int pos) {
  int delta = ls->fs->pc+JMPSIZE - pos;  /* jump is relative */
  code_oparg(ls, op, delta+(codesize(delta)-2), 0);
}


static void codeIf (LexState *ls, int thenAdd, int elseAdd) {
  FuncState *fs = ls->fs;
  int elseinit = elseAdd+JMPSIZE;
  if (fs->pc == elseinit) {  /* no else part? */
    fs->pc -= JMPSIZE;
    elseinit = fs->pc;
  }
  else
    elseinit += fix_jump(ls, elseAdd, JMP, fs->pc);
  fix_jump(ls, thenAdd, IFFJMP, elseinit);
}


static void func_onstack (LexState *ls, FuncState *func) {
  FuncState *fs = ls->fs;
  int i;
  int c = next_constant(fs);
  ttype(&fs->f->consts[c]) = LUA_T_PROTO;
  fs->f->consts[c].value.tf = func->f;
  if (func->nupvalues == 0)
    code_constant(ls, c);
  else {
    for (i=0; i<func->nupvalues; i++)
      lua_pushvar(ls, &func->upvalues[i]);
    deltastack(ls, 1);  /* CLOSURE puts one extra element (before poping) */
    code_oparg(ls, CLOSURE, c, -func->nupvalues);
    code_byte(fs, (Byte)func->nupvalues);
  }
}


static void init_state (LexState *ls, FuncState *fs, TaggedString *source) {
  TProtoFunc *f = luaF_newproto();
  fs->prev = ls->fs;  /* linked list of funcstates */
  ls->fs = fs;
  fs->stacksize = 0;
  fs->maxstacksize = 0;
  fs->nlocalvar = 0;
  fs->nupvalues = 0;
  fs->lastsetline = 0;
  fs->f = f;
  f->source = source;
  fs->pc = 0;
  f->code = NULL;
  fs->nvars = (L->debug) ? 0 : -1;  /* flag no debug information? */
  code_byte(fs, 0);  /* to be filled with maxstacksize */
  code_byte(fs, 0);  /* to be filled with arg information */
  /* push function (to avoid GC) */
  tfvalue(L->stack.top) = f; ttype(L->stack.top) = LUA_T_PROTO;
  incr_top;
}


static void close_func (LexState *ls) {
  FuncState *fs = ls->fs;
  TProtoFunc *f = fs->f;
  code_opcode(ls, ENDCODE, 0);
  f->code[0] = (Byte)fs->maxstacksize;
  luaM_reallocvector(f->code, fs->pc, Byte);
  luaM_reallocvector(f->consts, f->nconsts, TObject);
  if (fs->nvars != -1) {  /* debug information? */
    luaI_registerlocalvar(fs, NULL, -1);  /* flag end of vector */
    luaM_reallocvector(f->locvars, fs->nvars, LocVar);
  }
  ls->fs = fs->prev;
  L->stack.top--;  /* pop function */
}



static int expfollow [] = {ELSE, ELSEIF, THEN, IF, WHILE, REPEAT, DO, NAME,
   LOCAL, FUNCTION, END, UNTIL, RETURN, ')', ']', '}', ';', EOS, ',',  0};


static int is_in (int tok, int *toks) {
  int *t;
  for (t=toks; *t; t++)
    if (*t == tok) return t-toks;
  return -1;
}


static void next (LexState *ls) {
  ls->token = luaX_lex(ls);
}


static void error_expected (LexState *ls, int token) {
  char buff[100], t[TOKEN_LEN];
  luaX_token2str(token, t);
  sprintf(buff, "`%s' expected", t);
  luaX_error(ls, buff);
}


static void error_unexpected (LexState *ls) {
  luaX_error(ls, "unexpected token");
}


static void error_unmatched (LexState *ls, int what, int who, int where) {
  if (where == ls->linenumber)
    error_expected(ls, what);
  else {
    char buff[100];
    char t_what[TOKEN_LEN], t_who[TOKEN_LEN];
    luaX_token2str(what, t_what);
    luaX_token2str(who, t_who);
    sprintf(buff, "`%s' expected (to close `%s' at line %d)",
            t_what, t_who, where);
    luaX_error(ls, buff);
  }
}

static void check (LexState *ls, int c) {
  if (ls->token != c)
    error_expected(ls, c);
  next(ls);
}

static void check_match (LexState *ls, int what, int who, int where) {
  if (ls->token != what)
    error_unmatched(ls, what, who, where);
  check_debugline(ls);  /* to 'mark' the 'what' */
  next(ls);
}

static int checkname (LexState *ls) {
  int sc;
  if (ls->token != NAME)
    luaX_error(ls, "`NAME' expected");
  sc = string_constant(ls->fs, ls->seminfo.ts);
  next(ls);
  return sc;
}


static TaggedString *str_checkname (LexState *ls) {
  int i = checkname(ls);  /* this call may realloc `f->consts' */
  return tsvalue(&ls->fs->f->consts[i]);
}


static int optional (LexState *ls, int c) {
  if (ls->token == c) {
    next(ls);
    return 1;
  }
  else return 0;
}


TProtoFunc *luaY_parser (ZIO *z) {
  struct LexState lexstate;
  struct FuncState funcstate;
  luaX_setinput(&lexstate, z);
  init_state(&lexstate, &funcstate, luaS_new(zname(z)));
  next(&lexstate);  /* read first token */
  chunk(&lexstate);
  if (lexstate.token != EOS)
    luaX_error(&lexstate, "<eof> expected");
  close_func(&lexstate);
  return funcstate.f;
}



/*============================================================*/
/* GRAMAR RULES */
/*============================================================*/

static void chunk (LexState *ls) {
  /* chunk -> statlist ret */
  statlist(ls);
  ret(ls);
}

static void statlist (LexState *ls) {
  /* statlist -> { stat [;] } */
  while (stat(ls)) {
    LUA_ASSERT(ls->fs->stacksize == ls->fs->nlocalvar,
               "stack size != # local vars");
    optional(ls, ';');
  }
}

static int stat (LexState *ls) {
  int line = ls->linenumber;  /* may be needed for error messages */
  FuncState *fs = ls->fs;
  switch (ls->token) {
    case IF:  /* stat -> IF ifpart END */
      ifpart(ls, line);
      return 1;

    case WHILE: {  /* stat -> WHILE cond DO block END */
      TProtoFunc *f = fs->f;
      int while_init = fs->pc;
      int cond_end, cond_size;
      next(ls);
      cond_end = cond(ls);
      check(ls, DO);
      block(ls);
      check_match(ls, END, WHILE, line);
      cond_size = cond_end-while_init;
      check_pc(fs, cond_size);
      memcpy(f->code+fs->pc, f->code+while_init, cond_size);
      luaO_memdown(f->code+while_init, f->code+cond_end, fs->pc-while_init);
      while_init += JMPSIZE + fix_jump(ls, while_init, JMP, fs->pc-cond_size);
      fix_upjmp(ls, IFTUPJMP, while_init);
      return 1;
    }

    case DO: {  /* stat -> DO block END */
      next(ls);
      block(ls);
      check_match(ls, END, DO, line);
      return 1;
    }

    case REPEAT: {  /* stat -> REPEAT block UNTIL exp1 */
      int repeat_init = fs->pc;
      next(ls);
      block(ls);
      check_match(ls, UNTIL, REPEAT, line);
      exp1(ls);
      fix_upjmp(ls, IFFUPJMP, repeat_init);
      deltastack(ls, -1);  /* pops condition */
      return 1;
    }

    case FUNCTION: {  /* stat -> FUNCTION funcname body */
      int needself;
      vardesc v;
      if (ls->fs->prev)  /* inside other function? */
        return 0;
      check_debugline(ls);
      next(ls);
      needself = funcname(ls, &v);
      body(ls, needself, line);
      storevar(ls, &v);
      return 1;
    }

    case LOCAL: {  /* stat -> LOCAL localnamelist decinit */
      listdesc d;
      int nvars;
      check_debugline(ls);
      next(ls);
      nvars = localnamelist(ls);
      decinit(ls, &d);
      fs->nlocalvar += nvars;
      correctvarlines(ls, nvars);  /* vars will be alive only after decinit */
      adjust_mult_assign(ls, nvars, &d);
      return 1;
    }

    case NAME: case '%': {  /* stat -> func | ['%'] NAME assignment */
      vardesc v;
      check_debugline(ls);
      var_or_func(ls, &v);
      if (v.k == VEXP) {  /* stat -> func */
        if (v.info == 0)  /* is just an upper value? */
          luaX_error(ls, "syntax error");
        close_exp(ls, v.info, 0);
      }
      else {  /* stat -> ['%'] NAME assignment */
        int left = assignment(ls, &v, 1);
        adjuststack(ls, left);  /* remove eventual 'garbage' left on stack */
      }
      return 1;
    }

    case RETURN: case ';': case ELSE: case ELSEIF:
    case END: case UNTIL: case EOS:  /* 'stat' follow */
      return 0;

    default:
      error_unexpected(ls);
      return 0;  /* to avoid warnings */
  }
}

static int SaveWord (LexState *ls) {
  int res = ls->fs->pc;
  check_pc(ls->fs, JMPSIZE);
  ls->fs->pc += JMPSIZE;  /* open space */
  return res;
}

static int SaveWordPop (LexState *ls) {
  deltastack(ls, -1);  /* pop condition */
  return SaveWord(ls);
}

static int cond (LexState *ls) {
  /* cond -> exp1 */
  exp1(ls);
  return SaveWordPop(ls);
}

static void block (LexState *ls) {
  /* block -> chunk */
  FuncState *fs = ls->fs;
  int nlocalvar = fs->nlocalvar;
  chunk(ls);
  adjuststack(ls, fs->nlocalvar - nlocalvar);
  for (; fs->nlocalvar > nlocalvar; fs->nlocalvar--)
    luaI_unregisterlocalvar(fs, fs->lastsetline);
}

static int funcname (LexState *ls, vardesc *v) {
  /* funcname -> NAME [':' NAME | '.' NAME] */
  int needself = 0;
  singlevar(ls, str_checkname(ls), v, 0);
  if (ls->token == ':' || ls->token == '.') {
    needself = (ls->token == ':');
    next(ls);
    lua_pushvar(ls, v);
    code_constant(ls, checkname(ls));
    v->k = VINDEXED;
  }
  return needself;
}

static void body (LexState *ls, int needself, int line) {
  /* body ->  '(' parlist ')' chunk END */
  FuncState newfs;
  init_state(ls, &newfs, ls->fs->f->source);
  newfs.f->lineDefined = line;
  check(ls, '(');
  if (needself)
    add_localvar(ls, luaS_new("self"));
  parlist(ls);
  check(ls, ')');
  chunk(ls);
  check_match(ls, END, FUNCTION, line);
  close_func(ls);
  func_onstack(ls, &newfs);
}


static void ifpart (LexState *ls, int line) {
  /* ifpart -> cond THEN block [ELSE block | ELSEIF ifpart] */
  int c;
  int e;
  next(ls);  /* skip IF or ELSEIF */
  c = cond(ls);
  check(ls, THEN);
  block(ls);
  e = SaveWord(ls);
  if (ls->token == ELSEIF)
    ifpart(ls, line);
  else {
    if (optional(ls, ELSE))
      block(ls);
    check_match(ls, END, IF, line);
  }
  codeIf(ls, c, e);
}


static void ret (LexState *ls) {
  /* ret -> [RETURN explist sc] */
  if (optional(ls, RETURN)) {
    listdesc e;
    check_debugline(ls);
    explist(ls, &e); 
    if (e.pc > 0) {  /* expression is an open function call? */
      Byte *code = ls->fs->f->code;
      code[e.pc-2] = TAILCALL;  /* instead of a conventional CALL */
      code[e.pc-1] = (Byte)ls->fs->nlocalvar;
    }
    else
      code_oparg(ls, RETCODE, ls->fs->nlocalvar, 0);
    ls->fs->stacksize = ls->fs->nlocalvar;  /* removes all temp values */
    optional(ls, ';');
  }
}


/*
** For parsing expressions, we use a classic stack with priorities.
** Each binary operator is represented by its index in "binop" + FIRSTBIN
** (EQ=2, NE=3, ... '^'=13). The unary NOT is 0 and UNMINUS is 1.
*/

#define INDNOT		0
#define INDMINUS	1

/* code of first binary operator */
#define FIRSTBIN	2

/* code for power operator (last operator)
** '^' needs special treatment because it is right associative
*/
#define POW	13

static int binop [] = {EQ, NE, '>', '<', LE, GE, CONC,
                        '+', '-', '*', '/', '^', 0};

static int priority [POW+1] =  {5, 5, 1, 1, 1, 1, 1, 1, 2, 3, 3, 4, 4, 6};

static OpCode opcodes [POW+1] = {NOTOP, MINUSOP, EQOP, NEQOP, GTOP, LTOP,
  LEOP, GEOP, CONCOP, ADDOP, SUBOP, MULTOP, DIVOP, POWOP};

#define MAXOPS	20  /* op's stack size (arbitrary limit) */

typedef struct stack_op {
  int ops[MAXOPS];
  int top;
} stack_op;


static void exp1 (LexState *ls) {
  vardesc v;
  exp0(ls, &v);
  lua_pushvar(ls, &v);
  if (is_in(ls->token, expfollow) < 0)
    luaX_error(ls, "ill-formed expression");
}


static void exp0 (LexState *ls, vardesc *v) {
  /* exp0 -> exp2 {(AND | OR) exp2} */
  exp2(ls, v);
  while (ls->token == AND || ls->token == OR) {
    int op = (ls->token == AND) ? ONFJMP : ONTJMP;
    int pc;
    lua_pushvar(ls, v);
    next(ls);
    pc = SaveWordPop(ls);
    exp2(ls, v);
    lua_pushvar(ls, v);
    fix_jump(ls, pc, op, ls->fs->pc);
  }
}


static void push (LexState *ls, stack_op *s, int op) {
  if (s->top >= MAXOPS)
    luaX_error(ls, "expression too complex");
  s->ops[s->top++] = op;
}


static void pop_to (LexState *ls, stack_op *s, int prio) {
  int op;
  while (s->top > 0 && priority[(op=s->ops[s->top-1])] >= prio) {
    code_opcode(ls, opcodes[op], op<FIRSTBIN?0:-1);
    s->top--;
  }
}

static void simpleexp (LexState *ls, vardesc *v, stack_op *s) {
  check_debugline(ls);
  switch (ls->token) {
    case NUMBER: {  /* simpleexp -> NUMBER */
      real r = ls->seminfo.r;
      next(ls);
      /* dirty trick: check whether it is a -NUMBER not followed by '^' */
      /* (because the priority of '^' is closer than '-'...)            */
      if (s->top > 0 && s->ops[s->top-1] == INDMINUS && ls->token != '^') {
        s->top--;  /* remove '-' from stack */
        r = -r;
      }
      code_number(ls, r);
      break;
    }

    case STRING:  /* simpleexp -> STRING */
      code_string(ls, ls->seminfo.ts);  /* must use 'seminfo' before "next" */
      next(ls);
      break;

    case NIL:  /* simpleexp -> NIL */
      adjuststack(ls, -1);
      next(ls);
      break;

    case '{':  /* simpleexp -> constructor */
      constructor(ls);
      break;

    case FUNCTION:  /* simpleexp -> FUNCTION body */
      next(ls);
      body(ls, 0, ls->linenumber);
      break;

    case '(':  /* simpleexp -> '(' exp0 ')' */
      next(ls);
      exp0(ls, v);
      check(ls, ')');
      return;

    case NAME: case '%':
      var_or_func(ls, v);
      return;

    default:
      luaX_error(ls, "<expression> expected");
      return;
  }
  v->k = VEXP; v->info = 0;
}


static void prefixexp (LexState *ls, vardesc *v, stack_op *s) {
  /* prefixexp -> {NOT | '-'} simpleexp */
  while (ls->token == NOT || ls->token == '-') {
    push(ls, s, (ls->token==NOT)?INDNOT:INDMINUS);
    next(ls);
  }
  simpleexp(ls, v, s);
}


static void exp2 (LexState *ls, vardesc *v) {
  stack_op s;
  int op;
  s.top = 0;
  prefixexp(ls, v, &s);
  while ((op = is_in(ls->token, binop)) >= 0) {
    op += FIRSTBIN;
    lua_pushvar(ls, v);
    /* '^' is right associative, so must 'simulate' a higher priority */
    pop_to(ls, &s, (op == POW)?priority[op]+1:priority[op]);
    push(ls, &s, op);
    next(ls);
    prefixexp(ls, v, &s);
    lua_pushvar(ls, v);
  }
  if (s.top > 0) {
    lua_pushvar(ls, v);
    pop_to(ls, &s, 0);
  }
}


static void var_or_func (LexState *ls, vardesc *v) {
  /* var_or_func -> ['%'] NAME var_or_func_tail */
  if (optional(ls, '%')) {  /* upvalue? */
    pushupvalue(ls, str_checkname(ls));
    v->k = VEXP;
    v->info = 0;  /* closed expression */
  }
  else  /* variable name */
    singlevar(ls, str_checkname(ls), v, 0);
  var_or_func_tail(ls, v);
}


static void var_or_func_tail (LexState *ls, vardesc *v) {
  for (;;) {
    switch (ls->token) {
      case '.':  /* var_or_func_tail -> '.' NAME */
        next(ls);
        lua_pushvar(ls, v);  /* 'v' must be on stack */
        v->k = VDOT;
        v->info = checkname(ls);
        break;

      case '[':  /* var_or_func_tail -> '[' exp1 ']' */
        next(ls);
        lua_pushvar(ls, v);  /* 'v' must be on stack */
        exp1(ls);
        check(ls, ']');
        v->k = VINDEXED;
        break;

      case ':':  /* var_or_func_tail -> ':' NAME funcparams */
        next(ls);
        lua_pushvar(ls, v);  /* 'v' must be on stack */
        code_oparg(ls, PUSHSELF, checkname(ls), 1);
        v->k = VEXP;
        v->info = funcparams(ls, 1);
        break;

      case '(': case STRING: case '{':  /* var_or_func_tail -> funcparams */
        lua_pushvar(ls, v);  /* 'v' must be on stack */
        v->k = VEXP;
        v->info = funcparams(ls, 0);
        break;

      default: return;  /* should be follow... */
    }
  }
}

static int funcparams (LexState *ls, int slf) {
  FuncState *fs = ls->fs;
  int nparams = 1;  /* in cases STRING and constructor */
  switch (ls->token) {
    case '(': {  /* funcparams -> '(' explist ')' */
      int line = ls->linenumber;
      listdesc e;
      next(ls);
      explist(ls, &e);
      check_match(ls, ')', '(', line);
      close_exp(ls, e.pc, 1);
      nparams = e.n;
      break;
    }

    case '{':  /* funcparams -> constructor */
      constructor(ls);
      break;

    case STRING:  /* funcparams -> STRING */
      code_string(ls, ls->seminfo.ts);  /* must use 'seminfo' before "next" */
      next(ls);
      break;

    default:
      luaX_error(ls, "function arguments expected");
      break;
  }
  code_byte(fs, CALL);
  code_byte(fs, 0);  /* save space for nresult */
  code_byte(fs, (Byte)(nparams+slf));
  return fs->pc-1;
}

static void explist (LexState *ls, listdesc *d) {
  switch (ls->token) {
    case ELSE: case ELSEIF: case END: case UNTIL:
    case EOS: case ';': case ')':
      d->pc = 0;
      d->n = 0;
      break;

    default:
      explist1(ls, d);
  }
}

static void explist1 (LexState *ls, listdesc *d) {
  vardesc v;
  exp0(ls, &v);
  d->n = 1;
  while (ls->token == ',') {
    d->n++;
    lua_pushvar(ls, &v);
    next(ls);
    exp0(ls, &v);
  }
  if (v.k == VEXP)
    d->pc = v.info;
  else {
    lua_pushvar(ls, &v);
    d->pc = 0;
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

          default: luaX_error(ls, "NAME or `...' expected");
        }
      }
      break;

    case ')': break;  /* parlist -> empty */

    default: luaX_error(ls, "NAME or `...' expected");
  }
  code_args(ls, nparams, dots);
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

static void decinit (LexState *ls, listdesc *d) {
  /* decinit -> ['=' explist1] */
  if (ls->token == '=') {
    next(ls);
    explist1(ls, d);
  }
  else {
    d->n = 0;
    d->pc = 0;
  }
}


static int assignment (LexState *ls, vardesc *v, int nvars) {
  int left = 0;
  checklimit(ls, nvars, MAXVARSLH, "variables in a multiple assignment");
  unloaddot(ls, v);
  if (ls->token == ',') {  /* assignment -> ',' NAME assignment */
    vardesc nv;
    next(ls);
    var_or_func(ls, &nv);
    if (nv.k == VEXP)
      luaX_error(ls, "syntax error");
    left = assignment(ls, &nv, nvars+1);
  }
  else {  /* assignment -> '=' explist1 */
    listdesc d;
    check(ls, '=');
    explist1(ls, &d);
    adjust_mult_assign(ls, nvars, &d);
  }
  if (v->k != VINDEXED || left+(nvars-1) == 0) {
    /* global/local var or indexed var without values in between */
    storevar(ls, v);
  }
  else {  /* indexed var with values in between*/
    code_oparg(ls, SETTABLE, left+(nvars-1), -1);
    left += 2;  /* table&index are not popped, because they aren't on top */
  }
  return left;
}


static void constructor (LexState *ls) {
  /* constructor -> '{' part [';' part] '}' */
  int line = ls->linenumber;
  int pc = SaveWord(ls);
  int nelems;
  constdesc cd;
  deltastack(ls, 1);
  check(ls, '{');
  part(ls, &cd);
  nelems = cd.n;
  if (ls->token == ';') {
    constdesc other_cd;
    next(ls);
    part(ls, &other_cd);
    if (cd.k == other_cd.k)  /* repeated parts? */
      luaX_error(ls, "invalid constructor syntax");
    nelems += other_cd.n;
  }
  check_match(ls, '}', '{', line);
  fix_opcode(ls, pc, CREATEARRAY, nelems);
}

static void part (LexState *ls, constdesc *cd) {
  switch (ls->token) {
    case ';': case '}':  /* part -> empty */
      cd->n = 0;
      cd->k = ls->token;
      return;

    case NAME: {
      vardesc v;
      exp0(ls, &v);
      if (ls->token == '=') {
        switch (v.k) {
          case VGLOBAL:
            code_constant(ls, v.info);
            break;
          case VLOCAL:
            code_string(ls, ls->fs->localvar[v.info]);
            break;
          default:
            error_unexpected(ls);
        }
        next(ls);
        exp1(ls);
        cd->n = recfields(ls);
        cd->k = 1;  /* record */
      }
      else {
        lua_pushvar(ls, &v);
        cd->n = listfields(ls);
        cd->k = 0;  /* list */
      }
      break;
    }

    case '[':  /* part -> recfield recfields */
      recfield(ls);
      cd->n = recfields(ls);
      cd->k = 1;  /* record */
      break;

    default:  /* part -> exp1 listfields */
      exp1(ls);
      cd->n = listfields(ls);
      cd->k = 0;  /* list */
      break;
  }
}

static int recfields (LexState *ls) {
  /* recfields -> { ',' recfield } [','] */
  int n = 1;  /* one has been read before */
  while (ls->token == ',') {
    next(ls);
    if (ls->token == ';' || ls->token == '}')
      break;
    recfield(ls);
    n++;
    if (n%RFIELDS_PER_FLUSH == 0)
      flush_record(ls, RFIELDS_PER_FLUSH);
  }
  flush_record(ls, n%RFIELDS_PER_FLUSH);
  return n;
}

static int listfields (LexState *ls) {
  /* listfields -> { ',' exp1 } [','] */
  int n = 1;  /* one has been read before */
  while (ls->token == ',') {
    next(ls);
    if (ls->token == ';' || ls->token == '}')
      break;
    exp1(ls);
    n++;
    if (n%LFIELDS_PER_FLUSH == 0)
      flush_list(ls, n/LFIELDS_PER_FLUSH - 1, LFIELDS_PER_FLUSH);
  }
  flush_list(ls, n/LFIELDS_PER_FLUSH, n%LFIELDS_PER_FLUSH);
  return n;
}

static void recfield (LexState *ls) {
  /* recfield -> (NAME | '['exp1']') = exp1 */
  switch (ls->token) {
    case NAME:
      code_constant(ls, checkname(ls));
      break;

    case '[':
      next(ls);
      exp1(ls);
      check(ls, ']');
      break;

    default: luaX_error(ls, "NAME or `[' expected");
  }
  check(ls, '=');
  exp1(ls);
}

