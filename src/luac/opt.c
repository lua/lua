/*
** $Id: opt.c,v 1.22 2000/10/31 16:57:23 lhf Exp $
** optimize bytecodes
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "luac.h"

static int MapConstant(Hash* t, int j, const TObject* key)
{
 const TObject* o=luaH_get(L,t,key);
 if (ttype(o)==LUA_TNUMBER)
  return (int) nvalue(o);
 else
 {
  TObject val;
  ttype(&val)=LUA_TNUMBER;
  nvalue(&val)=j;
  *luaH_set(L,t,key)=val;
  LUA_ASSERT(j>=0,"MapConstant returns negative!");
  return j;
 }
}

static int MapConstants(Proto* tf, Hash* map)
{
 int i,j,k,n,m=0;
 TObject o;
 j=0; n=tf->nknum; ttype(&o)=LUA_TNUMBER;
 for (i=0; i<n; i++)
 {
  nvalue(&o)=tf->knum[i];
  k=MapConstant(map,j,&o);
  if (k==j) j++;
 }
 m=j;
 j=0; n=tf->nkstr; ttype(&o)=LUA_TSTRING;
 for (i=0; i<n; i++)
 {
  tsvalue(&o)=tf->kstr[i];
  k=MapConstant(map,j,&o);
  if (k==j) j++;
 }
 return m+j;
}

static void PackConstants(Proto* tf, Hash* map)
{
 int i,j,k,n;
 TObject o;
#ifdef DEBUG
 printf("%p before pack nknum=%d nkstr=%d\n",tf,tf->nknum,tf->nkstr);
#endif
 j=0; n=tf->nknum; ttype(&o)=LUA_TNUMBER;
 for (i=0; i<n; i++)
 {
  nvalue(&o)=tf->knum[i];
  k=MapConstant(map,-1,&o);
  if (k==j) tf->knum[j++]=tf->knum[i];
 }
 tf->nknum=j;
 j=0; n=tf->nkstr; ttype(&o)=LUA_TSTRING;
 for (i=0; i<n; i++)
 {
  tsvalue(&o)=tf->kstr[i];
  k=MapConstant(map,-1,&o);
  if (k==j) tf->kstr[j++]=tf->kstr[i];
 }
 tf->nkstr=j;
#ifdef DEBUG
 printf("%p after  pack nknum=%d nkstr=%d\n",tf,tf->nknum,tf->nkstr);
#endif
}

static void OptConstants(Proto* tf)
{
 Instruction* p;
 int n=tf->nknum+tf->nkstr;
 Hash* map=luaH_new(L,n);
 int m=MapConstants(tf,map);
#ifdef DEBUG
 printf("%p n=%d m=%d %s\n",tf,n,m,(m==n)?"nothing to optimize":"yes!");
#endif
 if (m==n) return;
 for (p=tf->code;; p++)
 {
  Instruction i=*p;
  int op=GET_OPCODE(i);
  switch (op)
  {
   TObject o;
   int j,k;
   case OP_PUSHNUM: case OP_PUSHNEGNUM:
    j=GETARG_U(i);
    ttype(&o)=LUA_TNUMBER; nvalue(&o)=tf->knum[j];
    k=MapConstant(map,-1,&o);
    if (k!=j) *p=CREATE_U(op,k);
    break;
   case OP_PUSHSTRING: case OP_GETGLOBAL: case OP_GETDOTTED:
   case OP_PUSHSELF:   case OP_SETGLOBAL:
    j=GETARG_U(i);
    ttype(&o)=LUA_TSTRING; tsvalue(&o)=tf->kstr[j];
    k=MapConstant(map,-1,&o);
    if (k!=j) *p=CREATE_U(op,k);
    break;
   case OP_END:
    PackConstants(tf,map);
    luaH_free(L,map);
    return;
   default:
    break;
  }
 }
}

#define OptFunction luaU_optchunk

void OptFunction(Proto* tf)
{
 int i,n=tf->nkproto;
 OptConstants(tf);
 for (i=0; i<n; i++) OptFunction(tf->kproto[i]);
}
