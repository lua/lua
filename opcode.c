/*
** opcode.c
** TecCGraf - PUC-Rio
*/

char *rcs_opcode="$Id: opcode.c,v 3.1 1994/11/02 20:30:53 roberto Exp roberto $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <math.h>
#ifdef __GNUC__
#include <floatingpoint.h>
#endif

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define tonumber(o) ((tag(o) != LUA_T_NUMBER) && (lua_tonumber(o) != 0))
#define tostring(o) ((tag(o) != LUA_T_STRING) && (lua_tostring(o) != 0))


#define STACK_BUFFER (STACKGAP+128)

static Long    maxstack;
static Object *stack=NULL;
static Object *top;


static int CBase = 0;   /* when Lua calls C or C calls Lua, points to the */
                        /* first slot after the last parameter. */
static int CnResults = 0; /* when Lua calls C, has the number of parameters; */
                         /* when C calls Lua, has the number of results. */

static  jmp_buf *errorJmp = NULL; /* current error recover point */


static int lua_execute (Byte *pc, int base);


static void lua_message (char *s)
{
  fprintf (stderr, "lua: %s\n", s);
}

/*
** Reports an error, and jumps up to the available recover label
*/
void lua_error (char *s)
{
  lua_message(s);
  if (errorJmp)
    longjmp(*errorJmp, 1);
  else
  {
    fprintf (stderr, "lua: exit(1). Unable to recover\n");
    exit(1);
  }
}


/*
** Init stack
*/
static void lua_initstack (void)
{
 maxstack = STACK_BUFFER;
 stack = (Object *)calloc(maxstack, sizeof(Object));
 if (stack == NULL)
   lua_error("stack - not enough memory");
 top = stack;
}


/*
** Check stack overflow and, if necessary, realloc vector
*/
static void lua_checkstack (Word n)
{
 if (stack == NULL)
   lua_initstack();
 if (n > maxstack)
 {
  int t = top-stack;
  maxstack *= 2;
  stack = (Object *)realloc(stack, maxstack*sizeof(Object));
  if (stack == NULL)
    lua_error("stack - not enough memory");
  top = stack + t;
 }
}


/*
** Concatenate two given strings, creating a mark space at the beginning.
** Return the new string pointer.
*/
static char *lua_strconc (char *l, char *r)
{
 static char buffer[1024];
 int n = strlen(l)+strlen(r)+1;
 if (n > 1024)
   lua_error ("string too large");
 return strcat(strcpy(buffer,l),r);
}


/*
** Convert, if possible, to a number object.
** Return 0 if success, not 0 if error.
*/
static int lua_tonumber (Object *obj)
{
 char c;
 float t;
 if (tag(obj) != LUA_T_STRING)
   return 1;
 else if (sscanf(svalue(obj), "%f %c",&t,&c) == 1)
 {
   nvalue(obj) = t;
   tag(obj) = LUA_T_NUMBER;
   return 0;
 }
 else
   return 2;
}


/*
** Convert, if possible, to a string tag
** Return 0 in success or not 0 on error.
*/
static int lua_tostring (Object *obj)
{
 static char s[256];
 if (tag(obj) != LUA_T_NUMBER)
   lua_reportbug ("unexpected type at conversion to string");
 if ((int) nvalue(obj) == nvalue(obj))
  sprintf (s, "%d", (int) nvalue(obj));
 else
  sprintf (s, "%g", nvalue(obj));
 svalue(obj) = lua_createstring(s);
 if (svalue(obj) == NULL)
  return 1;
 tag(obj) = LUA_T_STRING;
 return 0;
}


/*
** Adjust stack. Set top to the given value, pushing NILs if needed.
*/
static void adjust_top (Object *newtop)
{
  while (top < newtop) tag(top++) = LUA_T_NIL;
  top = newtop;  /* top could be bigger than newtop */
}


/*
** Call a C function. CBase will point to the top of the stack,
** and CnResults is the number of parameters. Returns an index
** to the first result from C.
*/
static int callC (lua_CFunction func, int base)
{
  int oldBase = CBase;
  int oldCnResults = CnResults;
  int firstResult;
  CnResults = (top-stack) - base;
  /* incorporate parameters on the stack */
  CBase = base+CnResults;
  (*func)();
  firstResult = CBase;
  CBase = oldBase;
  CnResults = oldCnResults;
  return firstResult;
}


/*
** Call a function (C or Lua). The parameters must be on the stack,
** between [stack+base,top). When returns, the results are on the stack,
** between [stack+whereRes,top). The number of results is nResults, unless
** nResults=MULT_RET.
*/
static void do_call (Object *func, int base, int nResults, int whereRes)
{
  int firstResult;
  if (tag(func) == LUA_T_CFUNCTION)
    firstResult = callC(fvalue(func), base);
  else if (tag(func) == LUA_T_FUNCTION)
    firstResult = lua_execute(bvalue(func), base);
  else
   lua_reportbug ("call expression not a function");
  /* adjust the number of results */
  if (nResults != MULT_RET && top - (stack+firstResult) != nResults)
    adjust_top(stack+firstResult+nResults);
  /* move results to the given position */
  if (firstResult != whereRes)
  {
    int i = top - (stack+firstResult);  /* number of results */
    top -= firstResult-whereRes;
    while (i--)
      *(stack+whereRes+i) = *(stack+firstResult+i);
  }
}


/*
** Function to index a table. Receives the table at top-2 and the index
** at top-1. Remove them from stack and push the result.
*/
static void pushsubscript (void)
{
  Object *h;
  if (tag(top-2) != LUA_T_ARRAY)
    lua_reportbug ("indexed expression not a table");
  h = lua_hashget (avalue(top-2), top-1);
  --top;
  *(top-1) = *h;
}


/*
** Function to store indexed based on values at the top
*/
int lua_storesubscript (void)
{
 if (tag(top-3) != LUA_T_ARRAY)
 {
  lua_reportbug ("indexed expression not a table");
  return 1;
 }
 {
  Object *h = lua_hashdefine (avalue(top-3), top-2);
  if (h == NULL) return 1;
  *h = *(top-1);
 }
 top -= 3;
 return 0;
}


/*
** Traverse all objects on stack
*/
void lua_travstack (void (*fn)(Object *))
{
 Object *o;
 for (o = top-1; o >= stack; o--)
  fn (o);
}


/*
** Execute a protected call. If function is null compiles the pre-set input.
** Leave nResults on the stack.
*/
static int do_protectedrun (Object *function, int nResults)
{
  Object f;
  jmp_buf myErrorJmp;
  int status;
  int oldCBase = CBase;
  jmp_buf *oldErr = errorJmp;
  errorJmp = &myErrorJmp;
  if (setjmp(myErrorJmp) == 0)
  {
    if (function == NULL)
    {
      function = &f;
      tag(function) = LUA_T_FUNCTION;
      bvalue(function) = lua_parse();
    }
    do_call(function, CBase, nResults, CBase);
    CnResults = (top-stack) - CBase;  /* number of results */
    CBase += CnResults;  /* incorporate results on the stack */
    status = 0;
  }
  else
  {
    CBase = oldCBase;
    top = stack+CBase;
    status = 1;
  }
  errorJmp = oldErr;
  return status;
}

/*
** Execute the given lua function. Return 0 on success or 1 on error.
*/
int lua_callfunction (Object *function)
{
  if (function == NULL)
    return 1;
  else
    return do_protectedrun (function, MULT_RET);
}


/*
** Open file, generate opcode and execute global statement. Return 0 on
** success or 1 on error.
*/
int lua_dofile (char *filename)
{
  int status;
  char *message = lua_openfile (filename);
  if (message)
  {
    lua_message(message);
    return 1;
  }
  status = do_protectedrun(NULL, 0);
  lua_closefile();
  return status;
}

/*
** Generate opcode stored on string and execute global statement. Return 0 on
** success or 1 on error.
*/
int lua_dostring (char *string)
{
  int status;
  char *message = lua_openstring(string);
  if (message)
  {
    lua_message(message);
    return 1;
  }
  status = do_protectedrun(NULL, 0);
  lua_closestring();
  return status;
}


/*
** Get a parameter, returning the object handle or NULL on error.
** 'number' must be 1 to get the first parameter.
*/
Object *lua_getparam (int number)
{
 if (number <= 0 || number > CnResults) return NULL;
 return (stack+(CBase-CnResults+number-1));
}

/*
** Given an object handle, return its number value. On error, return 0.0.
*/
real lua_getnumber (Object *object)
{
 if (object == NULL || tag(object) == LUA_T_NIL) return 0.0;
 if (tonumber (object)) return 0.0;
 else                   return (nvalue(object));
}

/*
** Given an object handle, return its string pointer. On error, return NULL.
*/
char *lua_getstring (Object *object)
{
 if (object == NULL || tag(object) == LUA_T_NIL) return NULL;
 if (tostring (object)) return NULL;
 else                   return (svalue(object));
}

/*
** Given an object handle, return a copy of its string. On error, return NULL.
*/
char *lua_copystring (Object *object)
{
 if (object == NULL || tag(object) == LUA_T_NIL) return NULL;
 if (tostring (object)) return NULL;
 else                   return (strdup(svalue(object)));
}

/*
** Given an object handle, return its cfuntion pointer. On error, return NULL.
*/
lua_CFunction lua_getcfunction (Object *object)
{
 if (object == NULL) return NULL;
 if (tag(object) != LUA_T_CFUNCTION) return NULL;
 else                            return (fvalue(object));
}

/*
** Given an object handle, return its user data. On error, return NULL.
*/
void *lua_getuserdata (Object *object)
{
 if (object == NULL) return NULL;
 if (tag(object) != LUA_T_USERDATA) return NULL;
 else                           return (uvalue(object));
}

/*
** Given an object handle, return its table. On error, return NULL.
*/
void *lua_gettable (Object *object)
{
 if (object == NULL) return NULL;
 if (tag(object) != LUA_T_ARRAY) return NULL;
 else                        return (avalue(object));
}

/*
** Get a global object. Return the object handle or NULL on error.
*/
Object *lua_getglobal (char *name)
{
 int n = lua_findsymbol(name);
 if (n < 0) return NULL;
 return &s_object(n);
}

/*
** Push a nil object
*/
int lua_pushnil (void)
{
 lua_checkstack(top-stack+1);
 tag(top++) = LUA_T_NIL;
 return 0;
}

/*
** Push an object (tag=number) to stack. Return 0 on success or 1 on error.
*/
int lua_pushnumber (real n)
{
 lua_checkstack(top-stack+1);
 tag(top) = LUA_T_NUMBER; nvalue(top++) = n;
 return 0;
}

/*
** Push an object (tag=string) to stack. Return 0 on success or 1 on error.
*/
int lua_pushstring (char *s)
{
 lua_checkstack(top-stack+1);
 tag(top) = LUA_T_STRING;
 svalue(top++) = lua_createstring(s);
 return 0;
}

/*
** Push an object (tag=cfunction) to stack. Return 0 on success or 1 on error.
*/
int lua_pushcfunction (lua_CFunction fn)
{
 lua_checkstack(top-stack+1);
 tag(top) = LUA_T_CFUNCTION; fvalue(top++) = fn;
 return 0;
}

/*
** Push an object (tag=userdata) to stack. Return 0 on success or 1 on error.
*/
int lua_pushuserdata (void *u)
{
 lua_checkstack(top-stack+1);
 tag(top) = LUA_T_USERDATA; uvalue(top++) = u;
 return 0;
}

/*
** Push an object (tag=userdata) to stack. Return 0 on success or 1 on error.
*/
int lua_pushtable (void *t)
{
 lua_checkstack(top-stack+1);
 tag(top) = LUA_T_ARRAY; avalue(top++) = t;
 return 0;
}

/*
** Push an object to stack.
*/
int lua_pushobject (Object *o)
{
 lua_checkstack(top-stack+1);
 *top++ = *o;
 return 0;
}

/*
** Store top of the stack at a global variable array field.
** Return 1 on error, 0 on success.
*/
int lua_storeglobal (char *name)
{
 int n = lua_findsymbol (name);
 if (n < 0) return 1;
 if (top-stack <= CBase) return 1;
 s_object(n) = *(--top);
 return 0;
}


/*
** Store top of the stack at an array field. Return 1 on error, 0 on success.
*/
int lua_storefield (lua_Object object, char *field)
{
 if (tag(object) != LUA_T_ARRAY)
  return 1;
 else
 {
  Object ref, *h;
  tag(&ref) = LUA_T_STRING;
  svalue(&ref) = lua_createstring(field);
  h = lua_hashdefine(avalue(object), &ref);
  if (h == NULL) return 1;
  if (tag(top-1) == LUA_T_MARK) return 1;
  *h = *(--top);
 }
 return 0;
}


/*
** Store top of the stack at an array index. Return 1 on error, 0 on success.
*/
int lua_storeindexed (lua_Object object, float index)
{
 if (tag(object) != LUA_T_ARRAY)
  return 1;
 else
 {
  Object ref, *h;
  tag(&ref) = LUA_T_NUMBER;
  nvalue(&ref) = index;
  h = lua_hashdefine(avalue(object), &ref);
  if (h == NULL) return 1;
  if (tag(top-1) == LUA_T_MARK) return 1;
  *h = *(--top);
 }
 return 0;
}


int lua_type (lua_Object o)
{
  if (o == NULL)
    return LUA_T_NIL;
  else
    return tag(o);
}


/*
** Execute the given opcode, until a RET. Parameters are between
** [stack+base,top). Returns n such that the the results are between
** [stack+n,top).
*/
static int lua_execute (Byte *pc, int base)
{
 lua_debugline = 0;  /* reset debug flag */
 lua_checkstack(STACKGAP+MAX_TEMPS+base);
 while (1)
 {
  OpCode opcode;
  switch (opcode = (OpCode)*pc++)
  {
   case PUSHNIL: tag(top++) = LUA_T_NIL; break;

   case PUSH0: tag(top) = LUA_T_NUMBER; nvalue(top++) = 0; break;
   case PUSH1: tag(top) = LUA_T_NUMBER; nvalue(top++) = 1; break;
   case PUSH2: tag(top) = LUA_T_NUMBER; nvalue(top++) = 2; break;

   case PUSHBYTE: tag(top) = LUA_T_NUMBER; nvalue(top++) = *pc++; break;

   case PUSHWORD:
   {
    CodeWord code;
    get_word(code,pc);
    tag(top) = LUA_T_NUMBER; nvalue(top++) = code.w;
   }
   break;

   case PUSHFLOAT:
   {
    CodeFloat code;
    get_float(code,pc);
    tag(top) = LUA_T_NUMBER; nvalue(top++) = code.f;
   }
   break;

   case PUSHSTRING:
   {
    CodeWord code;
    get_word(code,pc);
    tag(top) = LUA_T_STRING; svalue(top++) = lua_constant[code.w];
   }
   break;

   case PUSHFUNCTION:
   {
    CodeCode code;
    get_code(code,pc);
    tag(top) = LUA_T_FUNCTION; bvalue(top++) = code.b;
   }
   break;

   case PUSHLOCAL0: case PUSHLOCAL1: case PUSHLOCAL2:
   case PUSHLOCAL3: case PUSHLOCAL4: case PUSHLOCAL5:
   case PUSHLOCAL6: case PUSHLOCAL7: case PUSHLOCAL8:
   case PUSHLOCAL9: *top++ = *((stack+base) + (int)(opcode-PUSHLOCAL0)); break;

   case PUSHLOCAL: *top++ = *((stack+base) + (*pc++)); break;

   case PUSHGLOBAL:
   {
    CodeWord code;
    get_word(code,pc);
    *top++ = s_object(code.w);
   }
   break;

   case PUSHINDEXED:
    pushsubscript();
    break;

   case PUSHSELF:
   {
     Object receiver = *(top-2);
     pushsubscript();
     *(top++) = receiver;
     break;
   }

   case STORELOCAL0: case STORELOCAL1: case STORELOCAL2:
   case STORELOCAL3: case STORELOCAL4: case STORELOCAL5:
   case STORELOCAL6: case STORELOCAL7: case STORELOCAL8:
   case STORELOCAL9:
     *((stack+base) + (int)(opcode-STORELOCAL0)) = *(--top);
     break;

   case STORELOCAL: *((stack+base) + (*pc++)) = *(--top); break;

   case STOREGLOBAL:
   {
    CodeWord code;
    get_word(code,pc);
    s_object(code.w) = *(--top);
   }
   break;

   case STOREINDEXED0:
   {
    int s = lua_storesubscript();
    if (s == 1) return 1;
   }
   break;

   case STOREINDEXED:
   {
    int n = *pc++;
    if (tag(top-3-n) != LUA_T_ARRAY)
      lua_reportbug ("indexed expression not a table");
    {
     Object *h = lua_hashdefine (avalue(top-3-n), top-2-n);
     if (h == NULL) return 1;
     *h = *(top-1);
    }
    top--;
   }
   break;

   case STORELIST0:
   case STORELIST:
   {
    int m, n;
    Object *arr;
    if (opcode == STORELIST0) m = 0;
    else m = *(pc++) * FIELDS_PER_FLUSH;
    n = *(pc++);
    arr = top-n-1;
    if (tag(arr) != LUA_T_ARRAY)
      lua_reportbug ("internal error - table expected");
    while (n)
    {
     tag(top) = LUA_T_NUMBER; nvalue(top) = n+m;
     *(lua_hashdefine (avalue(arr), top)) = *(top-1);
     top--;
     n--;
    }
   }
   break;

   case STORERECORD:
   {
    int n = *(pc++);
    Object *arr = top-n-1;
    if (tag(arr) != LUA_T_ARRAY)
      lua_reportbug ("internal error - table expected");
    while (n)
    {
     CodeWord code;
     get_word(code,pc);
     tag(top) = LUA_T_STRING; svalue(top) = lua_constant[code.w];
     *(lua_hashdefine (avalue(arr), top)) = *(top-1);
     top--;
     n--;
    }
   }
   break;

   case ADJUST0:
     adjust_top((stack+base));
     break;

   case ADJUST:
     adjust_top((stack+base) + *(pc++));
     break;

   case CREATEARRAY:
   {
    CodeWord size;
    get_word(size,pc);
    top++;
    avalue(top-1) = lua_createarray(size.w);
    tag(top-1) = LUA_T_ARRAY;
   }
   break;

   case EQOP:
   {
    int res;
    Object *l = top-2;
    Object *r = top-1;
    --top;
    if (tag(l) != tag(r))
     res = 0;
    else
    {
     switch (tag(l))
     {
      case LUA_T_NIL:
        res = 1; break;
      case LUA_T_NUMBER:
        res = (nvalue(l) == nvalue(r)); break;
      case LUA_T_ARRAY:
        res = (avalue(l) == avalue(r)); break;
      case LUA_T_FUNCTION:
        res = (bvalue(l) == bvalue(r)); break;
      case LUA_T_CFUNCTION:
       res = (fvalue(l) == fvalue(r)); break;
      case LUA_T_STRING:
        res = (strcmp (svalue(l), svalue(r)) == 0); break;
      default:
        res = (uvalue(l) == uvalue(r)); break;
     }
    }
    tag(top-1) = res ? LUA_T_NUMBER : LUA_T_NIL;
    nvalue(top-1) = 1;
   }
   break;

   case LTOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    --top;
    if (tag(l) == LUA_T_NUMBER && tag(r) == LUA_T_NUMBER)
     tag(top-1) = (nvalue(l) < nvalue(r)) ? LUA_T_NUMBER : LUA_T_NIL;
    else
    {
     if (tostring(l) || tostring(r))
      return 1;
     tag(top-1) = (strcmp (svalue(l), svalue(r)) < 0) ? LUA_T_NUMBER : LUA_T_NIL;
    }
    nvalue(top-1) = 1;
   }
   break;

   case LEOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    --top;
    if (tag(l) == LUA_T_NUMBER && tag(r) == LUA_T_NUMBER)
     tag(top-1) = (nvalue(l) <= nvalue(r)) ? LUA_T_NUMBER : LUA_T_NIL;
    else
    {
     if (tostring(l) || tostring(r))
      return 1;
     tag(top-1) = (strcmp (svalue(l), svalue(r)) <= 0) ? LUA_T_NUMBER : LUA_T_NIL;
    }
    nvalue(top-1) = 1;
   }
   break;

   case ADDOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tonumber(r) || tonumber(l))
     return 1;
    nvalue(l) += nvalue(r);
    --top;
   }
   break;

   case SUBOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tonumber(r) || tonumber(l))
     return 1;
    nvalue(l) -= nvalue(r);
    --top;
   }
   break;

   case MULTOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tonumber(r) || tonumber(l))
     return 1;
    nvalue(l) *= nvalue(r);
    --top;
   }
   break;

   case DIVOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tonumber(r) || tonumber(l))
     return 1;
    nvalue(l) /= nvalue(r);
    --top;
   }
   break;

   case POWOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tonumber(r) || tonumber(l))
     return 1;
    nvalue(l) = pow(nvalue(l), nvalue(r));
    --top;
   }
   break;

   case CONCOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    if (tostring(r) || tostring(l))
     return 1;
    svalue(l) = lua_createstring (lua_strconc(svalue(l),svalue(r)));
    if (svalue(l) == NULL)
     return 1;
    --top;
   }
   break;

   case MINUSOP:
    if (tonumber(top-1))
     return 1;
    nvalue(top-1) = - nvalue(top-1);
   break;

   case NOTOP:
    tag(top-1) = tag(top-1) == LUA_T_NIL ? LUA_T_NUMBER : LUA_T_NIL;
   break;

   case ONTJMP:
   {
    CodeWord code;
    get_word(code,pc);
    if (tag(top-1) != LUA_T_NIL) pc += code.w;
   }
   break;

   case ONFJMP:	
   {
    CodeWord code;
    get_word(code,pc);
    if (tag(top-1) == LUA_T_NIL) pc += code.w;
   }
   break;

   case JMP:
   {
    CodeWord code;
    get_word(code,pc);
    pc += code.w;
   }
   break;

   case UPJMP:
   {
    CodeWord code;
    get_word(code,pc);
    pc -= code.w;
   }
   break;

   case IFFJMP:
   {
    CodeWord code;
    get_word(code,pc);
    top--;
    if (tag(top) == LUA_T_NIL) pc += code.w;
   }
   break;

   case IFFUPJMP:
   {
    CodeWord code;
    get_word(code,pc);
    top--;
    if (tag(top) == LUA_T_NIL) pc -= code.w;
   }
   break;

   case POP: --top; break;

   case CALLFUNC:
   {
     int nParams = *(pc++);
     int nResults = *(pc++);
     Object *func = top-1-nParams; /* function is below parameters */
     int newBase = (top-stack)-nParams;
     do_call(func, newBase, nResults, newBase-1);
   }
   break;

   case RETCODE0:
     return base;

   case RETCODE:
     return base+*pc;

   case SETFUNCTION:
   {
    CodeCode file;
    CodeWord func;
    get_code(file,pc);
    get_word(func,pc);
    if (lua_pushfunction ((char *)file.b, func.w))
     return 1;
   }
   break;

   case SETLINE:
   {
    CodeWord code;
    get_word(code,pc);
    lua_debugline = code.w;
   }
   break;

   case RESET:
    lua_popfunction ();
   break;

   default:
    lua_error ("internal error - opcode doesn't match");
   return 1;
  }
 }
}

