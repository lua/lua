/*
** $Id: lvm.c,v 1.58 1999/06/22 20:37:23 roberto Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "luadebug.h"
#include "lvm.h"


#ifdef OLD_ANSI
#define strcoll(a,b)	strcmp(a,b)
#endif


#define highbyte(x)	((x)<<8)


/* Extra stack size to run a function: LUA_T_LINE(1), TM calls(2), ... */
#define	EXTRA_STACK	5



static TaggedString *strconc (TaggedString *l, TaggedString *r) {
  long nl = l->u.s.len;
  long nr = r->u.s.len;
  char *buffer = luaL_openspace(nl+nr);
  memcpy(buffer, l->str, nl);
  memcpy(buffer+nl, r->str, nr);
  return luaS_newlstr(buffer, nl+nr);
}


int luaV_tonumber (TObject *obj) {  /* LUA_NUMBER */
  if (ttype(obj) != LUA_T_STRING)
    return 1;
  else {
    double t;
    char *e = svalue(obj);
    int sig = 1;
    while (isspace((unsigned char)*e)) e++;
    if (*e == '-') {
      e++;
      sig = -1;
    }
    else if (*e == '+') e++;
    /* no digit before or after decimal point? */
    if (!isdigit((unsigned char)*e) && !isdigit((unsigned char)*(e+1)))
      return 2;
    t = luaO_str2d(e);
    if (t<0) return 2;
    nvalue(obj) = (real)t*sig;
    ttype(obj) = LUA_T_NUMBER;
    return 0;
  }
}


int luaV_tostring (TObject *obj) {  /* LUA_NUMBER */
  if (ttype(obj) != LUA_T_NUMBER)
    return 1;
  else {
    char s[32];  /* 16 digits, signal, point and \0  (+ some extra...) */
    sprintf(s, "%.16g", (double)nvalue(obj));
    tsvalue(obj) = luaS_new(s);
    ttype(obj) = LUA_T_STRING;
    return 0;
  }
}


void luaV_setn (Hash *t, int val) {
  TObject index, value;
  ttype(&index) = LUA_T_STRING; tsvalue(&index) = luaS_new("n");
  ttype(&value) = LUA_T_NUMBER; nvalue(&value) = val;
  luaH_set(t, &index, &value);
}


void luaV_closure (int nelems) {
  if (nelems > 0) {
    struct Stack *S = &L->stack;
    Closure *c = luaF_newclosure(nelems);
    c->consts[0] = *(S->top-1);
    memcpy(&c->consts[1], S->top-(nelems+1), nelems*sizeof(TObject));
    S->top -= nelems;
    ttype(S->top-1) = LUA_T_CLOSURE;
    (S->top-1)->value.cl = c;
  }
}


/*
** Function to index a table.
** Receives the table at top-2 and the index at top-1.
*/
void luaV_gettable (void) {
  TObject *table = L->stack.top-2;
  TObject *im;
  if (ttype(table) != LUA_T_ARRAY) {  /* not a table, get gettable method */
    im = luaT_getimbyObj(table, IM_GETTABLE);
    if (ttype(im) == LUA_T_NIL)
      lua_error("indexed expression not a table");
  }
  else {  /* object is a table... */
    int tg = table->value.a->htag;
    im = luaT_getim(tg, IM_GETTABLE);
    if (ttype(im) == LUA_T_NIL) {  /* and does not have a "gettable" method */
      TObject *h = luaH_get(avalue(table), table+1);
      if (ttype(h) == LUA_T_NIL &&
          (ttype(im=luaT_getim(tg, IM_INDEX)) != LUA_T_NIL)) {
        /* result is nil and there is an "index" tag method */
        luaD_callTM(im, 2, 1);  /* calls it */
      }
      else {
        L->stack.top--;
        *table = *h;  /* "push" result into table position */
      }
      return;
    }
    /* else it has a "gettable" method, go through to next command */
  }
  /* object is not a table, or it has a "gettable" method */
  luaD_callTM(im, 2, 1);
}


/*
** Receives table at *t, index at *(t+1) and value at top.
*/
void luaV_settable (TObject *t) {
  struct Stack *S = &L->stack;
  TObject *im;
  if (ttype(t) != LUA_T_ARRAY) {  /* not a table, get "settable" method */
    im = luaT_getimbyObj(t, IM_SETTABLE);
    if (ttype(im) == LUA_T_NIL)
      lua_error("indexed expression not a table");
  }
  else {  /* object is a table... */
    im = luaT_getim(avalue(t)->htag, IM_SETTABLE);
    if (ttype(im) == LUA_T_NIL) {  /* and does not have a "settable" method */
      luaH_set(avalue(t), t+1, S->top-1);
      S->top--;  /* pop value */
      return;
    }
    /* else it has a "settable" method, go through to next command */
  }
  /* object is not a table, or it has a "settable" method */
  /* prepare arguments and call the tag method */
  *(S->top+1) = *(L->stack.top-1);
  *(S->top) = *(t+1);
  *(S->top-1) = *t;
  S->top += 2;  /* WARNING: caller must assure stack space */
  luaD_callTM(im, 3, 0);
}


void luaV_rawsettable (TObject *t) {
  if (ttype(t) != LUA_T_ARRAY)
    lua_error("indexed expression not a table");
  else {
    struct Stack *S = &L->stack;
    luaH_set(avalue(t), t+1, S->top-1);
    S->top -= 3;
  }
}


void luaV_getglobal (TaggedString *ts) {
  /* WARNING: caller must assure stack space */
  /* only userdata, tables and nil can have getglobal tag methods */
  static char valid_getglobals[] = {1, 0, 0, 1, 0, 0, 1, 0};  /* ORDER LUA_T */
  TObject *value = &ts->u.s.globalval;
  if (valid_getglobals[-ttype(value)]) {
    TObject *im = luaT_getimbyObj(value, IM_GETGLOBAL);
    if (ttype(im) != LUA_T_NIL) {  /* is there a tag method? */
      struct Stack *S = &L->stack;
      ttype(S->top) = LUA_T_STRING;
      tsvalue(S->top) = ts;
      S->top++;
      *S->top++ = *value;
      luaD_callTM(im, 2, 1);
      return;
    }
    /* else no tag method: go through to default behavior */
  }
  *L->stack.top++ = *value;  /* default behavior */
}


void luaV_setglobal (TaggedString *ts) {
  TObject *oldvalue = &ts->u.s.globalval;
  TObject *im = luaT_getimbyObj(oldvalue, IM_SETGLOBAL);
  if (ttype(im) == LUA_T_NIL)  /* is there a tag method? */
    luaS_rawsetglobal(ts, --L->stack.top);
  else {
    /* WARNING: caller must assure stack space */
    struct Stack *S = &L->stack;
    TObject newvalue;
    newvalue = *(S->top-1);
    ttype(S->top-1) = LUA_T_STRING;
    tsvalue(S->top-1) = ts;
    *S->top++ = *oldvalue;
    *S->top++ = newvalue;
    luaD_callTM(im, 3, 0);
  }
}


static void call_binTM (IMS event, char *msg)
{
  TObject *im = luaT_getimbyObj(L->stack.top-2, event);/* try first operand */
  if (ttype(im) == LUA_T_NIL) {
    im = luaT_getimbyObj(L->stack.top-1, event);  /* try second operand */
    if (ttype(im) == LUA_T_NIL) {
      im = luaT_getim(0, event);  /* try a 'global' i.m. */
      if (ttype(im) == LUA_T_NIL)
        lua_error(msg);
    }
  }
  lua_pushstring(luaT_eventname[event]);
  luaD_callTM(im, 3, 1);
}


static void call_arith (IMS event)
{
  call_binTM(event, "unexpected type in arithmetic operation");
}


static int luaV_strcomp (char *l, long ll, char *r, long lr)
{
  for (;;) {
    long temp = strcoll(l, r);
    if (temp != 0) return temp;
    /* strings are equal up to a '\0' */
    temp = strlen(l);  /* index of first '\0' in both strings */
    if (temp == ll)  /* l is finished? */
      return (temp == lr) ? 0 : -1;  /* l is equal or smaller than r */
    else if (temp == lr)  /* r is finished? */
      return 1;  /* l is greater than r (because l is not finished) */
    /* both strings longer than temp; go on comparing (after the '\0') */
    temp++;
    l += temp; ll -= temp; r += temp; lr -= temp;
  }
}

void luaV_comparison (lua_Type ttype_less, lua_Type ttype_equal,
                      lua_Type ttype_great, IMS op) {
  struct Stack *S = &L->stack;
  TObject *l = S->top-2;
  TObject *r = S->top-1;
  real result;
  if (ttype(l) == LUA_T_NUMBER && ttype(r) == LUA_T_NUMBER)
    result = nvalue(l)-nvalue(r);
  else if (ttype(l) == LUA_T_STRING && ttype(r) == LUA_T_STRING)
    result = luaV_strcomp(svalue(l), tsvalue(l)->u.s.len,
                          svalue(r), tsvalue(r)->u.s.len);
  else {
    call_binTM(op, "unexpected type in comparison");
    return;
  }
  S->top--;
  nvalue(S->top-1) = 1;
  ttype(S->top-1) = (result < 0) ? ttype_less :
                                (result == 0) ? ttype_equal : ttype_great;
}


void luaV_pack (StkId firstel, int nvararg, TObject *tab) {
  TObject *firstelem = L->stack.stack+firstel;
  int i;
  Hash *htab;
  if (nvararg < 0) nvararg = 0;
  htab = avalue(tab) = luaH_new(nvararg+1);  /* +1 for field 'n' */
  ttype(tab) = LUA_T_ARRAY;
  for (i=0; i<nvararg; i++)
    luaH_setint(htab, i+1, firstelem+i);
  luaV_setn(htab, nvararg);  /* store counter in field "n" */
}


static void adjust_varargs (StkId first_extra_arg)
{
  TObject arg;
  luaV_pack(first_extra_arg,
       (L->stack.top-L->stack.stack)-first_extra_arg, &arg);
  luaD_adjusttop(first_extra_arg);
  *L->stack.top++ = arg;
}



/*
** Execute the given opcode, until a RET. Parameters are between
** [stack+base,top). Returns n such that the the results are between
** [stack+n,top).
*/
StkId luaV_execute (Closure *cl, TProtoFunc *tf, StkId base) {
  struct Stack *S = &L->stack;  /* to optimize */
  register Byte *pc = tf->code;
  TObject *consts = tf->consts;
  if (L->callhook)
    luaD_callHook(base, tf, 0);
  luaD_checkstack((*pc++)+EXTRA_STACK);
  if (*pc < ZEROVARARG)
    luaD_adjusttop(base+*(pc++));
  else {  /* varargs */
    luaC_checkGC();
    adjust_varargs(base+(*pc++)-ZEROVARARG);
  }
  for (;;) {
    register int aux = 0;
    switchentry:
    switch ((OpCode)*pc++) {

      case ENDCODE:
        S->top = S->stack + base;
        goto ret;
        
      case RETCODE:
        base += *pc++;
        goto ret;

      case CALL: aux = *pc++;
        luaD_calln(*pc++, aux);
        break;

      case TAILCALL: aux = *pc++;
        luaD_calln(*pc++, MULT_RET);
        base += aux;
        goto ret;

      case PUSHNIL: aux = *pc++;
        do {
          ttype(S->top++) = LUA_T_NIL;
        } while (aux--);
        break;

      case POP: aux = *pc++;
        S->top -= aux;
        break;

      case PUSHNUMBERW: aux += highbyte(*pc++);
      case PUSHNUMBER:  aux += *pc++;
        ttype(S->top) = LUA_T_NUMBER;
        nvalue(S->top) = aux;
        S->top++;
        break;

      case PUSHNUMBERNEGW: aux += highbyte(*pc++);
      case PUSHNUMBERNEG:  aux += *pc++;
        ttype(S->top) = LUA_T_NUMBER;
        nvalue(S->top) = -aux;
        S->top++;
        break;

      case PUSHCONSTANTW: aux += highbyte(*pc++);
      case PUSHCONSTANT:  aux += *pc++;
        *S->top++ = consts[aux];
        break;

      case PUSHUPVALUE: aux = *pc++;
        *S->top++ = cl->consts[aux+1];
        break;

      case PUSHLOCAL: aux = *pc++;
        *S->top++ = *((S->stack+base) + aux);
        break;

      case GETGLOBALW: aux += highbyte(*pc++);
      case GETGLOBAL:  aux += *pc++;
        luaV_getglobal(tsvalue(&consts[aux]));
        break;

      case GETTABLE:
        luaV_gettable();
        break;

      case GETDOTTEDW: aux += highbyte(*pc++);
      case GETDOTTED:  aux += *pc++;
        *S->top++ = consts[aux];
        luaV_gettable();
        break;

      case PUSHSELFW: aux += highbyte(*pc++);
      case PUSHSELF:  aux += *pc++; {
        TObject receiver;
        receiver = *(S->top-1);
        *S->top++ = consts[aux];
        luaV_gettable();
        *S->top++ = receiver;
        break;
      }

      case CREATEARRAYW: aux += highbyte(*pc++);
      case CREATEARRAY:  aux += *pc++;
        luaC_checkGC();
        avalue(S->top) = luaH_new(aux);
        ttype(S->top) = LUA_T_ARRAY;
        S->top++;
        break;

      case SETLOCAL: aux = *pc++;
        *((S->stack+base) + aux) = *(--S->top);
        break;

      case SETGLOBALW: aux += highbyte(*pc++);
      case SETGLOBAL:  aux += *pc++;
        luaV_setglobal(tsvalue(&consts[aux]));
        break;

      case SETTABLEPOP:
        luaV_settable(S->top-3);
        S->top -= 2;  /* pop table and index */
        break;

      case SETTABLE:
        luaV_settable(S->top-3-(*pc++));
        break;

      case SETLISTW: aux += highbyte(*pc++);
      case SETLIST:  aux += *pc++; {
        int n = *(pc++);
        Hash *arr = avalue(S->top-n-1);
        aux *= LFIELDS_PER_FLUSH;
        for (; n; n--)
          luaH_setint(arr, n+aux, --S->top);
        break;
      }

      case SETMAP:  aux = *pc++; {
        Hash *arr = avalue(S->top-(2*aux)-3);
        do {
          luaH_set(arr, S->top-2, S->top-1);
          S->top-=2;
        } while (aux--);
        break;
      }

      case NEQOP: aux = 1;
      case EQOP: {
        int res = luaO_equalObj(S->top-2, S->top-1);
        if (aux) res = !res;
        S->top--;
        ttype(S->top-1) = res ? LUA_T_NUMBER : LUA_T_NIL;
        nvalue(S->top-1) = 1;
        break;
      }

       case LTOP:
         luaV_comparison(LUA_T_NUMBER, LUA_T_NIL, LUA_T_NIL, IM_LT);
         break;

      case LEOP:
        luaV_comparison(LUA_T_NUMBER, LUA_T_NUMBER, LUA_T_NIL, IM_LE);
        break;

      case GTOP:
        luaV_comparison(LUA_T_NIL, LUA_T_NIL, LUA_T_NUMBER, IM_GT);
        break;

      case GEOP:
        luaV_comparison(LUA_T_NIL, LUA_T_NUMBER, LUA_T_NUMBER, IM_GE);
        break;

      case ADDOP: {
        TObject *l = S->top-2;
        TObject *r = S->top-1;
        if (tonumber(r) || tonumber(l))
          call_arith(IM_ADD);
        else {
          nvalue(l) += nvalue(r);
          --S->top;
        }
        break;
      }

      case SUBOP: {
        TObject *l = S->top-2;
        TObject *r = S->top-1;
        if (tonumber(r) || tonumber(l))
          call_arith(IM_SUB);
        else {
          nvalue(l) -= nvalue(r);
          --S->top;
        }
        break;
      }

      case MULTOP: {
        TObject *l = S->top-2;
        TObject *r = S->top-1;
        if (tonumber(r) || tonumber(l))
          call_arith(IM_MUL);
        else {
          nvalue(l) *= nvalue(r);
          --S->top;
        }
        break;
      }

      case DIVOP: {
        TObject *l = S->top-2;
        TObject *r = S->top-1;
        if (tonumber(r) || tonumber(l))
          call_arith(IM_DIV);
        else {
          nvalue(l) /= nvalue(r);
          --S->top;
        }
        break;
      }

      case POWOP:
        call_binTM(IM_POW, "undefined operation");
        break;

      case CONCOP: {
        TObject *l = S->top-2;
        TObject *r = S->top-1;
        if (tostring(l) || tostring(r))
          call_binTM(IM_CONCAT, "unexpected type for concatenation");
        else {
          tsvalue(l) = strconc(tsvalue(l), tsvalue(r));
          --S->top;
        }
        luaC_checkGC();
        break;
      }

      case MINUSOP:
        if (tonumber(S->top-1)) {
          ttype(S->top) = LUA_T_NIL;
          S->top++;
          call_arith(IM_UNM);
        }
        else
          nvalue(S->top-1) = - nvalue(S->top-1);
        break;

      case NOTOP:
        ttype(S->top-1) =
           (ttype(S->top-1) == LUA_T_NIL) ? LUA_T_NUMBER : LUA_T_NIL;
        nvalue(S->top-1) = 1;
        break;

      case ONTJMPW: aux += highbyte(*pc++);
      case ONTJMP:  aux += *pc++;
        if (ttype(S->top-1) != LUA_T_NIL) pc += aux;
        else S->top--;
        break;

      case ONFJMPW: aux += highbyte(*pc++);
      case ONFJMP:  aux += *pc++;
        if (ttype(S->top-1) == LUA_T_NIL) pc += aux;
        else S->top--;
        break;

      case JMPW: aux += highbyte(*pc++);
      case JMP:  aux += *pc++;
        pc += aux;
        break;

      case IFFJMPW: aux += highbyte(*pc++);
      case IFFJMP:  aux += *pc++;
        if (ttype(--S->top) == LUA_T_NIL) pc += aux;
        break;

      case IFTUPJMPW: aux += highbyte(*pc++);
      case IFTUPJMP:  aux += *pc++;
        if (ttype(--S->top) != LUA_T_NIL) pc -= aux;
        break;

      case IFFUPJMPW: aux += highbyte(*pc++);
      case IFFUPJMP:  aux += *pc++;
        if (ttype(--S->top) == LUA_T_NIL) pc -= aux;
        break;

      case CLOSUREW: aux += highbyte(*pc++);
      case CLOSURE:  aux += *pc++;
        *S->top++ = consts[aux];
        luaV_closure(*pc++);
        luaC_checkGC();
        break;

      case SETLINEW: aux += highbyte(*pc++);
      case SETLINE:  aux += *pc++;
        if ((S->stack+base-1)->ttype != LUA_T_LINE) {
          /* open space for LINE value */
          luaD_openstack((S->top-S->stack)-base);
          base++;
          (S->stack+base-1)->ttype = LUA_T_LINE;
        }
        (S->stack+base-1)->value.i = aux;
        if (L->linehook)
          luaD_lineHook(aux);
        break;

      case LONGARGW: aux += highbyte(*pc++);
      case LONGARG:  aux += *pc++;
        aux = highbyte(highbyte(aux));
        goto switchentry;  /* do not reset "aux" */

      case CHECKSTACK: aux = *pc++;
        LUA_ASSERT((S->top-S->stack)-base == aux && S->last >= S->top,
                   "wrong stack size");
        break;

    }
  } ret:
  if (L->callhook)
    luaD_callHook(0, NULL, 1);
  return base;
}

