/*
** opcode.c
** TecCGraf - PUC-Rio
*/

char *rcs_opcode="$Id: opcode.c,v 2.1 1994/04/20 22:07:57 celes Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* stdlib.h does not have this in SunOS */
extern double strtod(const char *, char **);

#include "mm.h"

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "lua.h"

#define tonumber(o) ((tag(o) != T_NUMBER) && (lua_tonumber(o) != 0))
#define tostring(o) ((tag(o) != T_STRING) && (lua_tostring(o) != 0))

#ifndef MAXSTACK
#define MAXSTACK 256
#endif
static Object stack[MAXSTACK] = {{T_MARK, {NULL}}};
static Object *top=stack+1, *base=stack+1;


/*
** Concatenate two given string, creating a mark space at the beginning.
** Return the new string pointer.
*/
static char *lua_strconc (char *l, char *r)
{
 char *s = calloc (strlen(l)+strlen(r)+2, sizeof(char));
 if (s == NULL)
 {
  lua_error ("not enough memory");
  return NULL;
 }
 *s++ = 0; 			/* create mark space */
 return strcat(strcpy(s,l),r);
}

/*
** Duplicate a string,  creating a mark space at the beginning.
** Return the new string pointer.
*/
char *lua_strdup (char *l)
{
 char *s = calloc (strlen(l)+2, sizeof(char));
 if (s == NULL)
 {
  lua_error ("not enough memory");
  return NULL;
 }
 *s++ = 0; 			/* create mark space */
 return strcpy(s,l);
}

/*
** Convert, if possible, to a number tag.
** Return 0 in success or not 0 on error.
*/ 
static int lua_tonumber (Object *obj)
{
 char *ptr;
 if (tag(obj) != T_STRING)
 {
  lua_reportbug ("unexpected type at conversion to number");
  return 1;
 }
 nvalue(obj) = strtod(svalue(obj), &ptr);
 if (*ptr)
 {
  lua_reportbug ("string to number convertion failed");
  return 2;
 }
 tag(obj) = T_NUMBER;
 return 0;
}

/*
** Test if is possible to convert an object to a number one.
** If possible, return the converted object, otherwise return nil object.
*/ 
static Object *lua_convtonumber (Object *obj)
{
 static Object cvt;
 
 if (tag(obj) == T_NUMBER)
 {
  cvt = *obj;
  return &cvt;
 }
  
 tag(&cvt) = T_NIL;
 if (tag(obj) == T_STRING)
 {
  char *ptr;
  nvalue(&cvt) = strtod(svalue(obj), &ptr);
  if (*ptr == 0)
   tag(&cvt) = T_NUMBER;
 }
 return &cvt;
}



/*
** Convert, if possible, to a string tag
** Return 0 in success or not 0 on error.
*/ 
static int lua_tostring (Object *obj)
{
 static char s[256];
 if (tag(obj) != T_NUMBER)
 {
  lua_reportbug ("unexpected type at conversion to string");
  return 1;
 }
 if ((int) nvalue(obj) == nvalue(obj))
  sprintf (s, "%d", (int) nvalue(obj));
 else
  sprintf (s, "%g", nvalue(obj));
 svalue(obj) = lua_createstring(lua_strdup(s));
 if (svalue(obj) == NULL)
  return 1;
 tag(obj) = T_STRING;
 return 0;
}


/*
** Execute the given opcode. Return 0 in success or 1 on error.
*/
int lua_execute (Byte *pc)
{
 Object *oldbase = base;
 base = top;
 while (1)
 {
  OpCode opcode;
  switch (opcode = (OpCode)*pc++)
  {
   case PUSHNIL: tag(top++) = T_NIL; break;
   
   case PUSH0: tag(top) = T_NUMBER; nvalue(top++) = 0; break;
   case PUSH1: tag(top) = T_NUMBER; nvalue(top++) = 1; break;
   case PUSH2: tag(top) = T_NUMBER; nvalue(top++) = 2; break;

   case PUSHBYTE: tag(top) = T_NUMBER; nvalue(top++) = *pc++; break;
   
   case PUSHWORD: 
   {
    CodeWord code;
    get_word(code,pc);
    tag(top) = T_NUMBER; nvalue(top++) = code.w;
   }
   break;
   
   case PUSHFLOAT:
   {
    CodeFloat code;
    get_float(code,pc);
    tag(top) = T_NUMBER; nvalue(top++) = code.f;
   }
   break;

   case PUSHSTRING:
   {
    CodeWord code;
    get_word(code,pc);
    tag(top) = T_STRING; svalue(top++) = lua_constant[code.w];
   }
   break;
   
   case PUSHLOCAL0: case PUSHLOCAL1: case PUSHLOCAL2:
   case PUSHLOCAL3: case PUSHLOCAL4: case PUSHLOCAL5:
   case PUSHLOCAL6: case PUSHLOCAL7: case PUSHLOCAL8:
   case PUSHLOCAL9: *top++ = *(base + (int)(opcode-PUSHLOCAL0)); break;
   
   case PUSHLOCAL: *top++ = *(base + (*pc++)); break;
   
   case PUSHGLOBAL: 
   {
    CodeWord code;
    get_word(code,pc);
    *top++ = s_object(code.w);
   }
   break;
   
   case PUSHINDEXED:
    --top;
    if (tag(top-1) != T_ARRAY)
    {
     lua_reportbug ("indexed expression not a table");
     return 1;
    }
    {
     Object *h = lua_hashdefine (avalue(top-1), top);
     if (h == NULL) return 1;
     *(top-1) = *h;
    }
   break;
   
   case PUSHMARK: tag(top++) = T_MARK; break;
   
   case PUSHOBJECT: *top = *(top-3); top++; break;
   
   case STORELOCAL0: case STORELOCAL1: case STORELOCAL2:
   case STORELOCAL3: case STORELOCAL4: case STORELOCAL5:
   case STORELOCAL6: case STORELOCAL7: case STORELOCAL8:
   case STORELOCAL9: *(base + (int)(opcode-STORELOCAL0)) = *(--top); break;
    
   case STORELOCAL: *(base + (*pc++)) = *(--top); break;
   
   case STOREGLOBAL:
   {
    CodeWord code;
    get_word(code,pc);
    s_object(code.w) = *(--top);
   }
   break;

   case STOREINDEXED0:
    if (tag(top-3) != T_ARRAY)
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
   break;
   
   case STOREINDEXED:
   {
    int n = *pc++;
    if (tag(top-3-n) != T_ARRAY)
    {
     lua_reportbug ("indexed expression not a table");
     return 1;
    }
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
    if (tag(arr) != T_ARRAY)
    {
     lua_reportbug ("internal error - table expected");
     return 1;
    }
    while (n)
    {
     tag(top) = T_NUMBER; nvalue(top) = n+m;
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
    if (tag(arr) != T_ARRAY)
    {
     lua_reportbug ("internal error - table expected");
     return 1;
    }
    while (n)
    {
     CodeWord code;
     get_word(code,pc);
     tag(top) = T_STRING; svalue(top) = lua_constant[code.w];
     *(lua_hashdefine (avalue(arr), top)) = *(top-1);
     top--;
     n--;
    }
   }
   break;
   
   case ADJUST:
   {
    Object *newtop = base + *(pc++);
    while (top < newtop) tag(top++) = T_NIL;
    top = newtop;  /* top could be bigger than newtop */
   }
   break;
   
   case CREATEARRAY:
    if (tag(top-1) == T_NIL) 
     nvalue(top-1) = 101;
    else 
    {
     if (tonumber(top-1)) return 1;
     if (nvalue(top-1) <= 0) nvalue(top-1) = 101;
    }
    avalue(top-1) = lua_createarray(nvalue(top-1));
    if (avalue(top-1) == NULL)
     return 1;
    tag(top-1) = T_ARRAY;
   break;
   
   case EQOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    --top;
    if (tag(l) != tag(r)) 
     tag(top-1) = T_NIL;
    else
    {
     switch (tag(l))
     {
      case T_NIL:       tag(top-1) = T_NUMBER; break;
      case T_NUMBER:    tag(top-1) = (nvalue(l) == nvalue(r)) ? T_NUMBER : T_NIL; break;
      case T_ARRAY:     tag(top-1) = (avalue(l) == avalue(r)) ? T_NUMBER : T_NIL; break;
      case T_FUNCTION:  tag(top-1) = (bvalue(l) == bvalue(r)) ? T_NUMBER : T_NIL; break;
      case T_CFUNCTION: tag(top-1) = (fvalue(l) == fvalue(r)) ? T_NUMBER : T_NIL; break;
      case T_USERDATA:  tag(top-1) = (uvalue(l) == uvalue(r)) ? T_NUMBER : T_NIL; break;
      case T_STRING:    tag(top-1) = (strcmp (svalue(l), svalue(r)) == 0) ? T_NUMBER : T_NIL; break;
      case T_MARK:      return 1;
     }
    }
    nvalue(top-1) = 1;
   }
   break;
    
   case LTOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    --top;
    if (tag(l) == T_NUMBER && tag(r) == T_NUMBER)
     tag(top-1) = (nvalue(l) < nvalue(r)) ? T_NUMBER : T_NIL;
    else
    {
     if (tostring(l) || tostring(r))
      return 1;
     tag(top-1) = (strcmp (svalue(l), svalue(r)) < 0) ? T_NUMBER : T_NIL;
    }
    nvalue(top-1) = 1; 
   }
   break;
   
   case LEOP:
   {
    Object *l = top-2;
    Object *r = top-1;
    --top;
    if (tag(l) == T_NUMBER && tag(r) == T_NUMBER)
     tag(top-1) = (nvalue(l) <= nvalue(r)) ? T_NUMBER : T_NIL;
    else
    {
     if (tostring(l) || tostring(r))
      return 1;
     tag(top-1) = (strcmp (svalue(l), svalue(r)) <= 0) ? T_NUMBER : T_NIL;
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
    tag(top-1) = tag(top-1) == T_NIL ? T_NUMBER : T_NIL;
   break; 
   
   case ONTJMP:
   {
    CodeWord code;
    get_word(code,pc);
    if (tag(top-1) != T_NIL) pc += code.w;
   }
   break;
   
   case ONFJMP:	   
   {
    CodeWord code;
    get_word(code,pc);
    if (tag(top-1) == T_NIL) pc += code.w;
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
    if (tag(top) == T_NIL) pc += code.w;
   }
   break;

   case IFFUPJMP:
   {
    CodeWord code;
    get_word(code,pc);
    top--;
    if (tag(top) == T_NIL) pc -= code.w;
   }
   break;

   case POP: --top; break;
   
   case CALLFUNC:
   {
    Byte *newpc;
    Object *b = top-1;
    while (tag(b) != T_MARK) b--;
    if (tag(b-1) == T_FUNCTION)
    {
     lua_debugline = 0;			/* always reset debug flag */
     newpc = bvalue(b-1);
     bvalue(b-1) = pc;		        /* store return code */
     nvalue(b) = (base-stack);		/* store base value */
     base = b+1;
     pc = newpc;
     if (MAXSTACK-(base-stack) < STACKGAP)
     {
      lua_error ("stack overflow");
      return 1;
     }
    }
    else if (tag(b-1) == T_CFUNCTION)
    {
     int nparam; 
     lua_debugline = 0;			/* always reset debug flag */
     nvalue(b) = (base-stack);		/* store base value */
     base = b+1;
     nparam = top-base;			/* number of parameters */
     (fvalue(b-1))();			/* call C function */
     
     /* shift returned values */
     { 
      int i;
      int nretval = top - base - nparam;
      top = base - 2;
      base = stack + (int) nvalue(base-1);
      for (i=0; i<nretval; i++)
      {
       *top = *(top+nparam+2);
       ++top;
      }
     }
    }
    else
    {
     lua_reportbug ("call expression not a function");
     return 1;
    }
   }
   break;
   
   case RETCODE:
   {
    int i;
    int shift = *pc++;
    int nretval = top - base - shift;
    top = base - 2;
    pc = bvalue(base-2);
    base = stack + (int) nvalue(base-1);
    for (i=0; i<nretval; i++)
    {
     *top = *(top+shift+2);
     ++top;
    }
   }
   break;
   
   case HALT:
    base = oldbase;
   return 0;		/* success */
   
   case SETFUNCTION:
   {
    CodeWord file, func;
    get_word(file,pc);
    get_word(func,pc);
    if (lua_pushfunction (file.w, func.w))
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
    lua_error ("internal error - opcode didn't match");
   return 1;
  }
 }
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
** Open file, generate opcode and execute global statement. Return 0 on
** success or 1 on error.
*/
int lua_dofile (char *filename)
{
 if (lua_openfile (filename)) return 1;
 if (lua_parse ()) { lua_closefile (); return 1; }
 lua_closefile ();
 return 0;
}

/*
** Generate opcode stored on string and execute global statement. Return 0 on
** success or 1 on error.
*/
int lua_dostring (char *string)
{
 if (lua_openstring (string)) return 1;
 if (lua_parse ()) return 1;
 lua_closestring();
 return 0;
}

/*
** Execute the given function. Return 0 on success or 1 on error.
*/
int lua_call (char *functionname, int nparam)
{
 static Byte startcode[] = {CALLFUNC, HALT};
 int i; 
 Object func = s_object(lua_findsymbol(functionname));
 if (tag(&func) != T_FUNCTION) return 1;
 for (i=1; i<=nparam; i++)
  *(top-i+2) = *(top-i);
 top += 2;
 tag(top-nparam-1) = T_MARK;
 *(top-nparam-2) = func;
 return (lua_execute (startcode));
}

/*
** Get a parameter, returning the object handle or NULL on error.
** 'number' must be 1 to get the first parameter.
*/
Object *lua_getparam (int number)
{
 if (number <= 0 || number > top-base) return NULL;
 return (base+number-1);
}

/*
** Given an object handle, return its number value. On error, return 0.0.
*/
real lua_getnumber (Object *object)
{
 if (object == NULL || tag(object) == T_NIL) return 0.0;
 if (tonumber (object)) return 0.0;
 else                   return (nvalue(object));
}

/*
** Given an object handle, return its string pointer. On error, return NULL.
*/
char *lua_getstring (Object *object)
{
 if (object == NULL || tag(object) == T_NIL) return NULL;
 if (tostring (object)) return NULL;
 else                   return (svalue(object));
}

/*
** Given an object handle, return a copy of its string. On error, return NULL.
*/
char *lua_copystring (Object *object)
{
 if (object == NULL || tag(object) == T_NIL) return NULL;
 if (tostring (object)) return NULL;
 else                   return (strdup(svalue(object)));
}

/*
** Given an object handle, return its cfuntion pointer. On error, return NULL.
*/
lua_CFunction lua_getcfunction (Object *object)
{
 if (object == NULL) return NULL;
 if (tag(object) != T_CFUNCTION) return NULL;
 else                            return (fvalue(object));
}

/*
** Given an object handle, return its user data. On error, return NULL.
*/
void *lua_getuserdata (Object *object)
{
 if (object == NULL) return NULL;
 if (tag(object) != T_USERDATA) return NULL;
 else                           return (uvalue(object));
}

/*
** Given an object handle and a field name, return its field object.
** On error, return NULL.
*/
Object *lua_getfield (Object *object, char *field)
{
 if (object == NULL) return NULL;
 if (tag(object) != T_ARRAY)
  return NULL;
 else
 {
  Object ref;
  tag(&ref) = T_STRING;
  svalue(&ref) = lua_createstring(lua_strdup(field));
  return (lua_hashdefine(avalue(object), &ref));
 }
}

/*
** Given an object handle and an index, return its indexed object.
** On error, return NULL.
*/
Object *lua_getindexed (Object *object, float index)
{
 if (object == NULL) return NULL;
 if (tag(object) != T_ARRAY)
  return NULL;
 else
 {
  Object ref;
  tag(&ref) = T_NUMBER;
  nvalue(&ref) = index;
  return (lua_hashdefine(avalue(object), &ref));
 }
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
** Pop and return an object
*/
Object *lua_pop (void)
{
 if (top <= base) return NULL;
 top--;
 return top;
}

/*
** Push a nil object
*/
int lua_pushnil (void)
{
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 tag(top) = T_NIL;
 return 0;
}

/*
** Push an object (tag=number) to stack. Return 0 on success or 1 on error.
*/
int lua_pushnumber (real n)
{
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 tag(top) = T_NUMBER; nvalue(top++) = n;
 return 0;
}

/*
** Push an object (tag=string) to stack. Return 0 on success or 1 on error.
*/
int lua_pushstring (char *s)
{
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 tag(top) = T_STRING; 
 svalue(top++) = lua_createstring(lua_strdup(s));
 return 0;
}

/*
** Push an object (tag=cfunction) to stack. Return 0 on success or 1 on error.
*/
int lua_pushcfunction (lua_CFunction fn)
{
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 tag(top) = T_CFUNCTION; fvalue(top++) = fn;
 return 0;
}

/*
** Push an object (tag=userdata) to stack. Return 0 on success or 1 on error.
*/
int lua_pushuserdata (void *u)
{
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
 tag(top) = T_USERDATA; uvalue(top++) = u;
 return 0;
}

/*
** Push an object to stack.
*/
int lua_pushobject (Object *o)
{
 if ((top-stack) >= MAXSTACK-1)
 {
  lua_error ("stack overflow");
  return 1;
 }
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
 if (tag(top-1) == T_MARK) return 1;
 s_object(n) = *(--top);
 return 0;
}

/*
** Store top of the stack at an array field. Return 1 on error, 0 on success.
*/
int lua_storefield (lua_Object object, char *field)
{
 if (tag(object) != T_ARRAY)
  return 1;
 else
 {
  Object ref, *h;
  tag(&ref) = T_STRING;
  svalue(&ref) = lua_createstring(lua_strdup(field));
  h = lua_hashdefine(avalue(object), &ref);
  if (h == NULL) return 1;
  if (tag(top-1) == T_MARK) return 1;
  *h = *(--top);
 }
 return 0;
}


/*
** Store top of the stack at an array index. Return 1 on error, 0 on success.
*/
int lua_storeindexed (lua_Object object, float index)
{
 if (tag(object) != T_ARRAY)
  return 1;
 else
 {
  Object ref, *h;
  tag(&ref) = T_NUMBER;
  nvalue(&ref) = index;
  h = lua_hashdefine(avalue(object), &ref);
  if (h == NULL) return 1;
  if (tag(top-1) == T_MARK) return 1;
  *h = *(--top);
 }
 return 0;
}


/*
** Given an object handle, return if it is nil.
*/
int lua_isnil (Object *object)
{
 return (object != NULL && tag(object) == T_NIL);
}

/*
** Given an object handle, return if it is a number one.
*/
int lua_isnumber (Object *object)
{
 return (object != NULL && tag(object) == T_NUMBER);
}

/*
** Given an object handle, return if it is a string one.
*/
int lua_isstring (Object *object)
{
 return (object != NULL && tag(object) == T_STRING);
}

/*
** Given an object handle, return if it is an array one.
*/
int lua_istable (Object *object)
{
 return (object != NULL && tag(object) == T_ARRAY);
}

/*
** Given an object handle, return if it is a cfunction one.
*/
int lua_iscfunction (Object *object)
{
 return (object != NULL && tag(object) == T_CFUNCTION);
}

/*
** Given an object handle, return if it is an user data one.
*/
int lua_isuserdata (Object *object)
{
 return (object != NULL && tag(object) == T_USERDATA);
}

/*
** Internal function: return an object type. 
*/
void lua_type (void)
{
 Object *o = lua_getparam(1);
 lua_pushstring (lua_constant[tag(o)]);
}

/*
** Internal function: convert an object to a number
*/
void lua_obj2number (void)
{
 Object *o = lua_getparam(1);
 lua_pushobject (lua_convtonumber(o));
}

/*
** Internal function: print object values
*/
void lua_print (void)
{
 int i=1;
 void *obj;
 while ((obj=lua_getparam (i++)) != NULL)
 {
  if      (lua_isnumber(obj))    printf("%g\n",lua_getnumber (obj));
  else if (lua_isstring(obj))    printf("%s\n",lua_getstring (obj));
  else if (lua_iscfunction(obj)) printf("cfunction: %p\n",lua_getcfunction (obj));
  else if (lua_isuserdata(obj))  printf("userdata: %p\n",lua_getuserdata (obj));
  else if (lua_istable(obj))     printf("table: %p\n",obj);
  else if (lua_isnil(obj))       printf("nil\n");
  else			         printf("invalid value to print\n");
 }
}

/*
** Internal function: do a file
*/
void lua_internaldofile (void)
{
 lua_Object obj = lua_getparam (1);
 if (lua_isstring(obj) && !lua_dofile(lua_getstring(obj)))
  lua_pushnumber(1);
 else
  lua_pushnil();
}

/*
** Internal function: do a string
*/
void lua_internaldostring (void)
{
 lua_Object obj = lua_getparam (1);
 if (lua_isstring(obj) && !lua_dostring(lua_getstring(obj)))
  lua_pushnumber(1);
 else
  lua_pushnil();
}


