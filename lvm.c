/*
** $Id: lvm.c,v 1.66 1999/11/22 13:12:07 roberto Exp roberto $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_REENTRANT

#include "lauxlib.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


#ifdef OLD_ANSI
#define strcoll(a,b)	strcmp(a,b)
#endif


#define highbyte(L, x)	((x)<<8)


/* Extra stack size to run a function: LUA_T_LINE(1), TM calls(2), ... */
#define	EXTRA_STACK	5



static TaggedString *strconc (lua_State *L, const TaggedString *l, const TaggedString *r) {
  long nl = l->u.s.len;
  long nr = r->u.s.len;
  char *buffer = luaL_openspace(L, nl+nr);
  memcpy(buffer, l->str, nl);
  memcpy(buffer+nl, r->str, nr);
  return luaS_newlstr(L, buffer, nl+nr);
}


int luaV_tonumber (TObject *obj) {  /* LUA_NUMBER */
  if (ttype(obj) != LUA_T_STRING)
    return 1;
  else {
    if (!luaO_str2d(svalue(obj), &nvalue(obj)))
      return 2;
    ttype(obj) = LUA_T_NUMBER;
    return 0;
  }
}


int luaV_tostring (lua_State *L, TObject *obj) {  /* LUA_NUMBER */
  if (ttype(obj) != LUA_T_NUMBER)
    return 1;
  else {
    char s[32];  /* 16 digits, signal, point and \0  (+ some extra...) */
    sprintf(s, "%.16g", (double)nvalue(obj));
    tsvalue(obj) = luaS_new(L, s);
    ttype(obj) = LUA_T_STRING;
    return 0;
  }
}


void luaV_setn (lua_State *L, Hash *t, int val) {
  TObject index, value;
  ttype(&index) = LUA_T_STRING; tsvalue(&index) = luaS_new(L, "n");
  ttype(&value) = LUA_T_NUMBER; nvalue(&value) = val;
  luaH_set(L, t, &index, &value);
}


void luaV_closure (lua_State *L, int nelems) {
  if (nelems > 0) {
    struct Stack *S = &L->stack;
    Closure *c = luaF_newclosure(L, nelems);
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
void luaV_gettable (lua_State *L) {
  TObject *table = L->stack.top-2;
  const TObject *im;
  if (ttype(table) != LUA_T_ARRAY) {  /* not a table, get gettable method */
    im = luaT_getimbyObj(L, table, IM_GETTABLE);
    if (ttype(im) == LUA_T_NIL)
      lua_error(L, "indexed expression not a table");
  }
  else {  /* object is a table... */
    int tg = table->value.a->htag;
    im = luaT_getim(L, tg, IM_GETTABLE);
    if (ttype(im) == LUA_T_NIL) {  /* and does not have a "gettable" method */
      const TObject *h = luaH_get(L, avalue(table), table+1);
      if (ttype(h) == LUA_T_NIL &&
          (ttype(im=luaT_getim(L, tg, IM_INDEX)) != LUA_T_NIL)) {
        /* result is nil and there is an "index" tag method */
        luaD_callTM(L, im, 2, 1);  /* calls it */
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
  luaD_callTM(L, im, 2, 1);
}


/*
** Receives table at *t, index at *(t+1) and value at top.
*/
void luaV_settable (lua_State *L, const TObject *t) {
  struct Stack *S = &L->stack;
  const TObject *im;
  if (ttype(t) != LUA_T_ARRAY) {  /* not a table, get "settable" method */
    im = luaT_getimbyObj(L, t, IM_SETTABLE);
    if (ttype(im) == LUA_T_NIL)
      lua_error(L, "indexed expression not a table");
  }
  else {  /* object is a table... */
    im = luaT_getim(L, avalue(t)->htag, IM_SETTABLE);
    if (ttype(im) == LUA_T_NIL) {  /* and does not have a "settable" method */
      luaH_set(L, avalue(t), t+1, S->top-1);
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
  luaD_callTM(L, im, 3, 0);
}


void luaV_rawsettable (lua_State *L, const TObject *t) {
  if (ttype(t) != LUA_T_ARRAY)
    lua_error(L, "indexed expression not a table");
  else {
    struct Stack *S = &L->stack;
    luaH_set(L, avalue(t), t+1, S->top-1);
    S->top -= 3;
  }
}


void luaV_getglobal (lua_State *L, GlobalVar *gv) {
  /* WARNING: caller must assure stack space */
  const TObject *value = &gv->value;
  switch (ttype(value)) {
    /* only userdata, tables and nil can have getglobal tag methods */
    case LUA_T_USERDATA: case LUA_T_ARRAY: case LUA_T_NIL: {
      TObject *im = luaT_getimbyObj(L, value, IM_GETGLOBAL);
      if (ttype(im) != LUA_T_NIL) {  /* is there a tag method? */
        struct Stack *S = &L->stack;
        ttype(S->top) = LUA_T_STRING;
        tsvalue(S->top) = gv->name;  /* global name */
        S->top++;
        *S->top++ = *value;
        luaD_callTM(L, im, 2, 1);
        return;
      }
      /* else no tag method: go through to default behavior */
    }
    default: *L->stack.top++ = *value;  /* default behavior */
  }
}


void luaV_setglobal (lua_State *L, GlobalVar *gv) {
  const TObject *oldvalue = &gv->value;
  const TObject *im = luaT_getimbyObj(L, oldvalue, IM_SETGLOBAL);
  if (ttype(im) == LUA_T_NIL)  /* is there a tag method? */
    gv->value = *(--L->stack.top);
  else {
    /* WARNING: caller must assure stack space */
    struct Stack *S = &L->stack;
    TObject newvalue;
    newvalue = *(S->top-1);
    ttype(S->top-1) = LUA_T_STRING;
    tsvalue(S->top-1) = gv->name;
    *S->top++ = *oldvalue;
    *S->top++ = newvalue;
    luaD_callTM(L, im, 3, 0);
  }
}


static void call_binTM (lua_State *L, IMS event, const char *msg) {
  /* try first operand */
  const TObject *im = luaT_getimbyObj(L, L->stack.top-2, event);
  if (ttype(im) == LUA_T_NIL) {
    im = luaT_getimbyObj(L, L->stack.top-1, event);  /* try second operand */
    if (ttype(im) == LUA_T_NIL) {
      im = luaT_getim(L, 0, event);  /* try a 'global' i.m. */
      if (ttype(im) == LUA_T_NIL)
        lua_error(L, msg);
    }
  }
  lua_pushstring(L, luaT_eventname[event]);
  luaD_callTM(L, im, 3, 1);
}


static void call_arith (lua_State *L, IMS event) {
  call_binTM(L, event, "unexpected type in arithmetic operation");
}


static int luaV_strcomp (const char *l, long ll, const char *r, long lr) {
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

void luaV_comparison (lua_State *L, lua_Type ttype_less, lua_Type ttype_equal,
                      lua_Type ttype_great, IMS op) {
  struct Stack *S = &L->stack;
  const TObject *l = S->top-2;
  const TObject *r = S->top-1;
  real result;
  if (ttype(l) == LUA_T_NUMBER && ttype(r) == LUA_T_NUMBER)
    result = nvalue(l)-nvalue(r);
  else if (ttype(l) == LUA_T_STRING && ttype(r) == LUA_T_STRING)
    result = luaV_strcomp(svalue(l), tsvalue(l)->u.s.len,
                          svalue(r), tsvalue(r)->u.s.len);
  else {
    call_binTM(L, op, "unexpected type in comparison");
    return;
  }
  S->top--;
  nvalue(S->top-1) = 1;
  ttype(S->top-1) = (result < 0) ? ttype_less :
                                (result == 0) ? ttype_equal : ttype_great;
}


void luaV_pack (lua_State *L, StkId firstel, int nvararg, TObject *tab) {
  TObject *firstelem = L->stack.stack+firstel;
  int i;
  Hash *htab;
  if (nvararg < 0) nvararg = 0;
  htab = avalue(tab) = luaH_new(L, nvararg+1);  /* +1 for field 'n' */
  ttype(tab) = LUA_T_ARRAY;
  for (i=0; i<nvararg; i++)
    luaH_setint(L, htab, i+1, firstelem+i);
  luaV_setn(L, htab, nvararg);  /* store counter in field "n" */
}


static void adjust_varargs (lua_State *L, StkId first_extra_arg) {
  TObject arg;
  luaV_pack(L, first_extra_arg,
       (L->stack.top-L->stack.stack)-first_extra_arg, &arg);
  luaD_adjusttop(L, first_extra_arg);
  *L->stack.top++ = arg;
}



/*
** Execute the given opcode, until a RET. Parameters are between
** [stack+base,top). Returns n such that the the results are between
** [stack+n,top).
*/
StkId luaV_execute (lua_State *L, const Closure *cl, const TProtoFunc *tf, StkId base) {
  struct Stack *S = &L->stack;  /* to optimize */
  register const Byte *pc = tf->code;
  const TObject *consts = tf->consts;
  if (L->callhook)
    luaD_callHook(L, base, tf, 0);
  luaD_checkstack(L, (*pc++)+EXTRA_STACK);
  if (*pc < ZEROVARARG)
    luaD_adjusttop(L, base+*(pc++));
  else {  /* varargs */
    luaC_checkGC(L);
    adjust_varargs(L, base+(*pc++)-ZEROVARARG);
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
        luaD_call(L, (S->stack+base) + *pc++, aux);
        break;

      case TAILCALL: aux = *pc++;
        luaD_call(L, (S->stack+base) + *pc++, MULT_RET);
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

      case PUSHNUMBERW: aux += highbyte(L, *pc++);
      case PUSHNUMBER:  aux += *pc++;
        ttype(S->top) = LUA_T_NUMBER;
        nvalue(S->top) = aux;
        S->top++;
        break;

      case PUSHNUMBERNEGW: aux += highbyte(L, *pc++);
      case PUSHNUMBERNEG:  aux += *pc++;
        ttype(S->top) = LUA_T_NUMBER;
        nvalue(S->top) = -aux;
        S->top++;
        break;

      case PUSHCONSTANTW: aux += highbyte(L, *pc++);
      case PUSHCONSTANT:  aux += *pc++;
        *S->top++ = consts[aux];
        break;

      case PUSHUPVALUE: aux = *pc++;
        *S->top++ = cl->consts[aux+1];
        break;

      case PUSHLOCAL: aux = *pc++;
        *S->top++ = *((S->stack+base) + aux);
        break;

      case GETGLOBALW: aux += highbyte(L, *pc++);
      case GETGLOBAL:  aux += *pc++;
        luaV_getglobal(L, tsvalue(&consts[aux])->u.s.gv);
        break;

      case GETTABLE:
        luaV_gettable(L);
        break;

      case GETDOTTEDW: aux += highbyte(L, *pc++);
      case GETDOTTED:  aux += *pc++;
        *S->top++ = consts[aux];
        luaV_gettable(L);
        break;

      case PUSHSELFW: aux += highbyte(L, *pc++);
      case PUSHSELF:  aux += *pc++; {
        TObject receiver;
        receiver = *(S->top-1);
        *S->top++ = consts[aux];
        luaV_gettable(L);
        *S->top++ = receiver;
        break;
      }

      case CREATEARRAYW: aux += highbyte(L, *pc++);
      case CREATEARRAY:  aux += *pc++;
        luaC_checkGC(L);
        avalue(S->top) = luaH_new(L, aux);
        ttype(S->top) = LUA_T_ARRAY;
        S->top++;
        break;

      case SETLOCAL: aux = *pc++;
        *((S->stack+base) + aux) = *(--S->top);
        break;

      case SETGLOBALW: aux += highbyte(L, *pc++);
      case SETGLOBAL:  aux += *pc++;
        luaV_setglobal(L, tsvalue(&consts[aux])->u.s.gv);
        break;

      case SETTABLEPOP:
        luaV_settable(L, S->top-3);
        S->top -= 2;  /* pop table and index */
        break;

      case SETTABLE:
        luaV_settable(L, S->top-3-(*pc++));
        break;

      case SETLISTW: aux += highbyte(L, *pc++);
      case SETLIST:  aux += *pc++; {
        int n = *(pc++);
        Hash *arr = avalue(S->top-n-1);
        aux *= LFIELDS_PER_FLUSH;
        for (; n; n--)
          luaH_setint(L, arr, n+aux, --S->top);
        break;
      }

      case SETMAP:  aux = *pc++; {
        Hash *arr = avalue(S->top-(2*aux)-3);
        do {
          luaH_set(L, arr, S->top-2, S->top-1);
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
         luaV_comparison(L, LUA_T_NUMBER, LUA_T_NIL, LUA_T_NIL, IM_LT);
         break;

      case LEOP:
        luaV_comparison(L, LUA_T_NUMBER, LUA_T_NUMBER, LUA_T_NIL, IM_LE);
        break;

      case GTOP:
        luaV_comparison(L, LUA_T_NIL, LUA_T_NIL, LUA_T_NUMBER, IM_GT);
        break;

      case GEOP:
        luaV_comparison(L, LUA_T_NIL, LUA_T_NUMBER, LUA_T_NUMBER, IM_GE);
        break;

      case ADDOP: {
        TObject *l = S->top-2;
        TObject *r = S->top-1;
        if (tonumber(r) || tonumber(l))
          call_arith(L, IM_ADD);
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
          call_arith(L, IM_SUB);
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
          call_arith(L, IM_MUL);
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
          call_arith(L, IM_DIV);
        else {
          nvalue(l) /= nvalue(r);
          --S->top;
        }
        break;
      }

      case POWOP:
        call_binTM(L, IM_POW, "undefined operation");
        break;

      case CONCOP: {
        TObject *l = S->top-2;
        TObject *r = S->top-1;
        if (tostring(L, l) || tostring(L, r))
          call_binTM(L, IM_CONCAT, "unexpected type for concatenation");
        else {
          tsvalue(l) = strconc(L, tsvalue(l), tsvalue(r));
          --S->top;
        }
        luaC_checkGC(L);
        break;
      }

      case MINUSOP:
        if (tonumber(S->top-1)) {
          ttype(S->top) = LUA_T_NIL;
          S->top++;
          call_arith(L, IM_UNM);
        }
        else
          nvalue(S->top-1) = - nvalue(S->top-1);
        break;

      case NOTOP:
        ttype(S->top-1) =
           (ttype(S->top-1) == LUA_T_NIL) ? LUA_T_NUMBER : LUA_T_NIL;
        nvalue(S->top-1) = 1;
        break;

      case ONTJMPW: aux += highbyte(L, *pc++);
      case ONTJMP:  aux += *pc++;
        if (ttype(S->top-1) != LUA_T_NIL) pc += aux;
        else S->top--;
        break;

      case ONFJMPW: aux += highbyte(L, *pc++);
      case ONFJMP:  aux += *pc++;
        if (ttype(S->top-1) == LUA_T_NIL) pc += aux;
        else S->top--;
        break;

      case JMPW: aux += highbyte(L, *pc++);
      case JMP:  aux += *pc++;
        pc += aux;
        break;

      case IFFJMPW: aux += highbyte(L, *pc++);
      case IFFJMP:  aux += *pc++;
        if (ttype(--S->top) == LUA_T_NIL) pc += aux;
        break;

      case IFTUPJMPW: aux += highbyte(L, *pc++);
      case IFTUPJMP:  aux += *pc++;
        if (ttype(--S->top) != LUA_T_NIL) pc -= aux;
        break;

      case IFFUPJMPW: aux += highbyte(L, *pc++);
      case IFFUPJMP:  aux += *pc++;
        if (ttype(--S->top) == LUA_T_NIL) pc -= aux;
        break;

      case CLOSUREW: aux += highbyte(L, *pc++);
      case CLOSURE:  aux += *pc++;
        *S->top++ = consts[aux];
        luaV_closure(L, *pc++);
        luaC_checkGC(L);
        break;

      case SETLINEW: aux += highbyte(L, *pc++);
      case SETLINE:  aux += *pc++;
        if ((S->stack+base-1)->ttype != LUA_T_LINE) {
          /* open space for LINE value */
          luaD_openstack(L, (S->top-S->stack)-base);
          base++;
          (S->stack+base-1)->ttype = LUA_T_LINE;
        }
        (S->stack+base-1)->value.i = aux;
        if (L->linehook)
          luaD_lineHook(L, aux);
        break;

      case LONGARGW: aux += highbyte(L, *pc++);
      case LONGARG:  aux += *pc++;
        aux = highbyte(L, highbyte(L, aux));
        goto switchentry;  /* do not reset "aux" */

    }
  } ret:
  if (L->callhook)
    luaD_callHook(L, 0, NULL, 1);
  return base;
}

