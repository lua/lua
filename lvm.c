/*
** $Id: $
** Lua virtual machine
** See Copyright Notice in lua.h
*/


#include <stdio.h>
#include <string.h>

#include "lauxlib.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lglobal.h"
#include "lmem.h"
#include "lopcodes.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "luadebug.h"
#include "lvm.h"


#define get_word(w,pc)  {w=*pc+(*(pc+1)<<8); pc+=2;}


/* Extra stack to run a function: LUA_T_LINE(1), TM calls(2), ... */
#define	EXTRA_STACK	4



static TaggedString *strconc (char *l, char *r)
{
  size_t nl = strlen(l);
  char *buffer = luaM_buffer(nl+strlen(r)+1);
  strcpy(buffer, l);
  strcpy(buffer+nl, r);
  return luaS_new(buffer);
}


int luaV_tonumber (TObject *obj)
{
  double t;
  char c;
  if (ttype(obj) != LUA_T_STRING)
    return 1;
  else if (sscanf(svalue(obj), "%lf %c",&t, &c) == 1) {
    nvalue(obj) = (real)t;
    ttype(obj) = LUA_T_NUMBER;
    return 0;
  }
  else
    return 2;
}


int luaV_tostring (TObject *obj)
{
  if (ttype(obj) != LUA_T_NUMBER)
    return 1;
  else {
    char s[60];
    real f = nvalue(obj);
    int i;
    if ((real)(-MAX_INT) <= f && f <= (real)MAX_INT && (real)(i=(int)f) == f)
      sprintf (s, "%d", i);
    else
      sprintf (s, "%g", (double)nvalue(obj));
    tsvalue(obj) = luaS_new(s);
    ttype(obj) = LUA_T_STRING;
    return 0;
  }
}


void luaV_closure (void)
{
  int nelems = (luaD_stack.top-1)->value.tf->nupvalues;
  Closure *c = luaF_newclosure(nelems);
  c->consts[0] = *(luaD_stack.top-1);
  memcpy(&c->consts[1], luaD_stack.top-(nelems+1), nelems*sizeof(TObject));
  luaD_stack.top -= nelems;
  ttype(luaD_stack.top-1) = LUA_T_FUNCTION;
  (luaD_stack.top-1)->value.cl = c;
}


/*
** Function to index a table.
** Receives the table at top-2 and the index at top-1.
*/
void luaV_gettable (void)
{
  TObject *im;
  if (ttype(luaD_stack.top-2) != LUA_T_ARRAY)  /* not a table, get "gettable" method */
    im = luaT_getimbyObj(luaD_stack.top-2, IM_GETTABLE);
  else {  /* object is a table... */
    int tg = (luaD_stack.top-2)->value.a->htag;
    im = luaT_getim(tg, IM_GETTABLE);
    if (ttype(im) == LUA_T_NIL) {  /* and does not have a "gettable" method */
      TObject *h = luaH_get(avalue(luaD_stack.top-2), luaD_stack.top-1);
      if (h != NULL && ttype(h) != LUA_T_NIL) {
        --luaD_stack.top;
        *(luaD_stack.top-1) = *h;
      }
      else if (ttype(im=luaT_getim(tg, IM_INDEX)) != LUA_T_NIL)
        luaD_callTM(im, 2, 1);
      else {
        --luaD_stack.top;
        ttype(luaD_stack.top-1) = LUA_T_NIL;
      }
      return;
    }
    /* else it has a "gettable" method, go through to next command */
  }
  /* object is not a table, or it has a "gettable" method */
  if (ttype(im) != LUA_T_NIL)
    luaD_callTM(im, 2, 1);
  else
    lua_error("indexed expression not a table");
}


/*
** Function to store indexed based on values at the luaD_stack.top
** mode = 0: raw store (without internal methods)
** mode = 1: normal store (with internal methods)
** mode = 2: "deep luaD_stack.stack" store (with internal methods)
*/
void luaV_settable (TObject *t, int mode)
{
  TObject *im = (mode == 0) ? NULL : luaT_getimbyObj(t, IM_SETTABLE);
  if (ttype(t) == LUA_T_ARRAY && (im == NULL || ttype(im) == LUA_T_NIL)) {
    TObject *h = luaH_set(avalue(t), t+1);
    *h = *(luaD_stack.top-1);
    luaD_stack.top -= (mode == 2) ? 1 : 3;
  }
  else {  /* object is not a table, and/or has a specific "settable" method */
    if (im && ttype(im) != LUA_T_NIL) {
      if (mode == 2) {
        *(luaD_stack.top+1) = *(luaD_stack.top-1);
        *(luaD_stack.top) = *(t+1);
        *(luaD_stack.top-1) = *t;
        luaD_stack.top += 2;  /* WARNING: caller must assure stack space */
      }
      luaD_callTM(im, 3, 0);
    }
    else
      lua_error("indexed expression not a table");
  }
}


void luaV_getglobal (Word n)
{
  /* WARNING: caller must assure stack space */
  TObject *value = &luaG_global[n].object;
  TObject *im = luaT_getimbyObj(value, IM_GETGLOBAL);
  if (ttype(im) == LUA_T_NIL) {  /* default behavior */
    *luaD_stack.top = *value;
    luaD_stack.top++;
  }
  else {
    ttype(luaD_stack.top) = LUA_T_STRING;
    tsvalue(luaD_stack.top) = luaG_global[n].varname;
    luaD_stack.top++;
    *luaD_stack.top = *value;
    luaD_stack.top++;
    luaD_callTM(im, 2, 1);
  }
}


void luaV_setglobal (Word n)
{
  TObject *oldvalue = &luaG_global[n].object;
  TObject *im = luaT_getimbyObj(oldvalue, IM_SETGLOBAL);
  if (ttype(im) == LUA_T_NIL)  /* default behavior */
    s_object(n) = *(--luaD_stack.top);
  else {
    /* WARNING: caller must assure stack space */
    TObject newvalue = *(luaD_stack.top-1);
    ttype(luaD_stack.top-1) = LUA_T_STRING;
    tsvalue(luaD_stack.top-1) = luaG_global[n].varname;
    *luaD_stack.top = *oldvalue;
    luaD_stack.top++;
    *luaD_stack.top = newvalue;
    luaD_stack.top++;
    luaD_callTM(im, 3, 0);
  }
}


static void call_binTM (IMS event, char *msg)
{
  TObject *im = luaT_getimbyObj(luaD_stack.top-2, event);/* try first operand */
  if (ttype(im) == LUA_T_NIL) {
    im = luaT_getimbyObj(luaD_stack.top-1, event);  /* try second operand */
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
  call_binTM(event, "unexpected type at arithmetic operation");
}


static void comparison (lua_Type ttype_less, lua_Type ttype_equal,
                        lua_Type ttype_great, IMS op)
{
  TObject *l = luaD_stack.top-2;
  TObject *r = luaD_stack.top-1;
  int result;
  if (ttype(l) == LUA_T_NUMBER && ttype(r) == LUA_T_NUMBER)
    result = (nvalue(l) < nvalue(r)) ? -1 : (nvalue(l) == nvalue(r)) ? 0 : 1;
  else if (ttype(l) == LUA_T_STRING && ttype(r) == LUA_T_STRING)
    result = strcoll(svalue(l), svalue(r));
  else {
    call_binTM(op, "unexpected type at comparison");
    return;
  }
  luaD_stack.top--;
  nvalue(luaD_stack.top-1) = 1;
  ttype(luaD_stack.top-1) = (result < 0) ? ttype_less :
                                (result == 0) ? ttype_equal : ttype_great;
}


void luaV_pack (StkId firstel, int nvararg, TObject *tab)
{
  TObject *firstelem = luaD_stack.stack+firstel;
  int i;
  if (nvararg < 0) nvararg = 0;
  avalue(tab)  = luaH_new(nvararg+1);  /* +1 for field 'n' */
  ttype(tab) = LUA_T_ARRAY;
  for (i=0; i<nvararg; i++) {
    TObject index;
    ttype(&index) = LUA_T_NUMBER;
    nvalue(&index) = i+1;
    *(luaH_set(avalue(tab), &index)) = *(firstelem+i);
  }
  /* store counter in field "n" */ {
    TObject index, extra;
    ttype(&index) = LUA_T_STRING;
    tsvalue(&index) = luaS_new("n");
    ttype(&extra) = LUA_T_NUMBER;
    nvalue(&extra) = nvararg;
    *(luaH_set(avalue(tab), &index)) = extra;
  }
}


static void adjust_varargs (StkId first_extra_arg)
{
  TObject arg;
  luaV_pack(first_extra_arg,
       (luaD_stack.top-luaD_stack.stack)-first_extra_arg, &arg);
  luaD_adjusttop(first_extra_arg);
  *luaD_stack.top = arg;
  luaD_stack.top++;
}



/*
** Execute the given opcode, until a RET. Parameters are between
** [luaD_stack.stack+base,luaD_stack.top). Returns n such that the the results are between
** [luaD_stack.stack+n,luaD_stack.top).
*/
StkId luaV_execute (Closure *cl, StkId base)
{
  TProtoFunc *func = cl->consts[0].value.tf;
  Byte *pc = func->code;
  if (lua_callhook)
    luaD_callHook(base, LUA_T_MARK, 0);
  luaD_checkstack((*pc++)+EXTRA_STACK);
  while (1) {
    OpCode opcode;
    switch (opcode = (OpCode)*pc++) {

      case PUSHNIL:
        ttype(luaD_stack.top) = LUA_T_NIL;
        luaD_stack.top++;
        break;

      case PUSHNILS: {
        int n = *pc++;
        while (n--)
          ttype(luaD_stack.top++) = LUA_T_NIL;
        break;
      }

      case PUSH0: case PUSH1: case PUSH2:
        ttype(luaD_stack.top) = LUA_T_NUMBER;
        nvalue(luaD_stack.top) = opcode-PUSH0;
        luaD_stack.top++;
        break;

      case PUSHBYTE:
        ttype(luaD_stack.top) = LUA_T_NUMBER;
        nvalue(luaD_stack.top) = *pc++;
        luaD_stack.top++;
        break;

      case PUSHWORD: {
        Word w;
        get_word(w,pc);
        ttype(luaD_stack.top) = LUA_T_NUMBER;
        nvalue(luaD_stack.top) = w;
        luaD_stack.top++;
        break;
      }

      case PUSHLOCAL0: case PUSHLOCAL1: case PUSHLOCAL2:
      case PUSHLOCAL3: case PUSHLOCAL4: case PUSHLOCAL5:
      case PUSHLOCAL6: case PUSHLOCAL7: case PUSHLOCAL8:
      case PUSHLOCAL9:
        *luaD_stack.top = *((luaD_stack.stack+base) + (int)(opcode-PUSHLOCAL0));
        luaD_stack.top++;
        break;

      case PUSHLOCAL:
        *luaD_stack.top = *((luaD_stack.stack+base) + (*pc++));
        luaD_stack.top++;
        break;

      case PUSHGLOBAL: {
        Word w;
        get_word(w,pc);
        luaV_getglobal(w);
        break;
      }

      case PUSHTABLE:
       luaV_gettable();
       break;

      case PUSHSELF: {
        TObject receiver = *(luaD_stack.top-1);
        Word w;
        get_word(w,pc);
        *luaD_stack.top = func->consts[w];
        luaD_stack.top++;
        luaV_gettable();
        *luaD_stack.top = receiver;
        luaD_stack.top++;
        break;
      }

      case PUSHCONSTANTB: {
        *luaD_stack.top = func->consts[*pc++];
        luaD_stack.top++;
        break;
      }

      case PUSHCONSTANT: {
        Word w;
        get_word(w,pc);
        *luaD_stack.top = func->consts[w];
        luaD_stack.top++;
        break;
      }

      case PUSHUPVALUE0:
      case PUSHUPVALUE: {
        int i = (opcode == PUSHUPVALUE0) ? 0 : *pc++;
        *luaD_stack.top = cl->consts[i+1];
        luaD_stack.top++;
        break;
      }

      case SETLOCAL0: case SETLOCAL1: case SETLOCAL2:
      case SETLOCAL3: case SETLOCAL4: case SETLOCAL5:
      case SETLOCAL6: case SETLOCAL7: case SETLOCAL8:
      case SETLOCAL9:
        *((luaD_stack.stack+base) + (int)(opcode-SETLOCAL0)) =
                                                      *(--luaD_stack.top);
        break;

      case SETLOCAL:
        *((luaD_stack.stack+base) + (*pc++)) = *(--luaD_stack.top); break;

      case SETGLOBAL: {
        Word w;
        get_word(w,pc);
        luaV_setglobal(w);
        break;
      }

      case SETTABLE0:
       luaV_settable(luaD_stack.top-3, 1);
       break;

      case SETTABLE: {
        int n = *pc++;
        luaV_settable(luaD_stack.top-3-n, 2);
        break;
      }

      case SETLIST0:
      case SETLIST: {
        int m, n;
        TObject *arr;
        if (opcode == SETLIST0) m = 0;
        else m = *(pc++) * LFIELDS_PER_FLUSH;
        n = *(pc++);
        arr = luaD_stack.top-n-1;
        while (n) {
          ttype(luaD_stack.top) = LUA_T_NUMBER; nvalue(luaD_stack.top) = n+m;
          *(luaH_set (avalue(arr), luaD_stack.top)) = *(luaD_stack.top-1);
          luaD_stack.top--;
          n--;
        }
        break;
      }

      case SETMAP: {
        int n = *(pc++);
        TObject *arr = luaD_stack.top-(2*n)-1;
        while (n--) {
          *(luaH_set (avalue(arr), luaD_stack.top-2)) = *(luaD_stack.top-1);
          luaD_stack.top-=2;
        }
        break;
      }

      case POPS:
        luaD_stack.top -= *(pc++);
        break;

      case ARGS:
        luaD_adjusttop(base + *(pc++));
        break;

      case VARARGS:
        luaC_checkGC();
        adjust_varargs(base + *(pc++));
        break;

      case CREATEARRAY: {
        Word size;
        luaC_checkGC();
        get_word(size,pc);
        avalue(luaD_stack.top) = luaH_new(size);
        ttype(luaD_stack.top) = LUA_T_ARRAY;
        luaD_stack.top++;
        break;
      }

      case EQOP: case NEQOP: {
        int res = luaO_equalObj(luaD_stack.top-2, luaD_stack.top-1);
        --luaD_stack.top;
        if (opcode == NEQOP) res = !res;
        ttype(luaD_stack.top-1) = res ? LUA_T_NUMBER : LUA_T_NIL;
        nvalue(luaD_stack.top-1) = 1;
        break;
      }

       case LTOP:
         comparison(LUA_T_NUMBER, LUA_T_NIL, LUA_T_NIL, IM_LT);
         break;

      case LEOP:
        comparison(LUA_T_NUMBER, LUA_T_NUMBER, LUA_T_NIL, IM_LE);
        break;

      case GTOP:
        comparison(LUA_T_NIL, LUA_T_NIL, LUA_T_NUMBER, IM_GT);
        break;

      case GEOP:
        comparison(LUA_T_NIL, LUA_T_NUMBER, LUA_T_NUMBER, IM_GE);
        break;

      case ADDOP: {
        TObject *l = luaD_stack.top-2;
        TObject *r = luaD_stack.top-1;
        if (tonumber(r) || tonumber(l))
          call_arith(IM_ADD);
        else {
          nvalue(l) += nvalue(r);
          --luaD_stack.top;
        }
        break;
      }

      case SUBOP: {
        TObject *l = luaD_stack.top-2;
        TObject *r = luaD_stack.top-1;
        if (tonumber(r) || tonumber(l))
          call_arith(IM_SUB);
        else {
          nvalue(l) -= nvalue(r);
          --luaD_stack.top;
        }
        break;
      }

      case MULTOP: {
        TObject *l = luaD_stack.top-2;
        TObject *r = luaD_stack.top-1;
        if (tonumber(r) || tonumber(l))
          call_arith(IM_MUL);
        else {
          nvalue(l) *= nvalue(r);
          --luaD_stack.top;
        }
        break;
      }

      case DIVOP: {
        TObject *l = luaD_stack.top-2;
        TObject *r = luaD_stack.top-1;
        if (tonumber(r) || tonumber(l))
          call_arith(IM_DIV);
        else {
          nvalue(l) /= nvalue(r);
          --luaD_stack.top;
        }
        break;
      }

      case POWOP:
        call_arith(IM_POW);
        break;

      case CONCOP: {
        TObject *l = luaD_stack.top-2;
        TObject *r = luaD_stack.top-1;
        if (tostring(l) || tostring(r))
          call_binTM(IM_CONCAT, "unexpected type for concatenation");
        else {
          tsvalue(l) = strconc(svalue(l), svalue(r));
          --luaD_stack.top;
        }
        luaC_checkGC();
        break;
      }

      case MINUSOP:
        if (tonumber(luaD_stack.top-1)) {
          ttype(luaD_stack.top) = LUA_T_NIL;
          luaD_stack.top++;
          call_arith(IM_UNM);
        }
        else
          nvalue(luaD_stack.top-1) = - nvalue(luaD_stack.top-1);
        break;

      case NOTOP:
        ttype(luaD_stack.top-1) =
           (ttype(luaD_stack.top-1) == LUA_T_NIL) ? LUA_T_NUMBER : LUA_T_NIL;
        nvalue(luaD_stack.top-1) = 1;
        break;

      case ONTJMP: {
        Word w;
        get_word(w,pc);
        if (ttype(luaD_stack.top-1) != LUA_T_NIL) pc += w;
        else luaD_stack.top--;
      }
      break;

      case ONFJMP: {
        Word w;
        get_word(w,pc);
        if (ttype(luaD_stack.top-1) == LUA_T_NIL) pc += w;
        else luaD_stack.top--;
        break;
      }

      case JMP: {
        Word w;
        get_word(w,pc);
        pc += w;
        break;
      }

      case UPJMP: {
        Word w;
        get_word(w,pc);
        pc -= w;
        break;
      }

      case IFFJMP: {
        Word w;
        get_word(w,pc);
        luaD_stack.top--;
        if (ttype(luaD_stack.top) == LUA_T_NIL) pc += w;
        break;
      }

      case IFFUPJMP: {
        Word w;
        get_word(w,pc);
        luaD_stack.top--;
        if (ttype(luaD_stack.top) == LUA_T_NIL) pc -= w;
        break;
      }

      case CLOSURE:
        luaV_closure();
        luaC_checkGC();
        break;

      case CALLFUNC: {
        int nParams = *pc++;
        int nResults = *pc++;
        StkId newBase = (luaD_stack.top-luaD_stack.stack)-nParams;
        luaD_call(newBase, nResults);
        break;
      }

      case ENDCODE:
        luaD_stack.top = luaD_stack.stack + base;
        /* goes through */
      case RETCODE:
        if (lua_callhook)
          luaD_callHook(base, LUA_T_MARK, 1);
        return (base + ((opcode==RETCODE) ? *pc : 0));

      case SETLINE: {
        Word line;
        get_word(line,pc);
        if ((luaD_stack.stack+base-1)->ttype != LUA_T_LINE) {
          /* open space for LINE value */
          luaD_openstack((luaD_stack.top-luaD_stack.stack)-base);
          base++;
          (luaD_stack.stack+base-1)->ttype = LUA_T_LINE;
        }
        (luaD_stack.stack+base-1)->value.i = line;
        if (lua_linehook)
          luaD_lineHook(line);
        break;
      }

#ifdef DEBUG
      default:
        lua_error("internal error - opcode doesn't match");
#endif
    }
  }
}

