/*
** $Id: lutf8lib.c,v 1.4 2014/03/20 19:36:02 roberto Exp $
** Standard library for UTF-8 manipulation
** See Copyright Notice in lua.h
*/


#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define lutf8lib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#define MAXUNICODE	0x10FFFF

#define iscont(p)	((*(p) & 0xC0) == 0x80)


/* from strlib */
/* translate a relative string position: negative means back from end */
static lua_Integer u_posrelat (lua_Integer pos, size_t len) {
  if (pos >= 0) return pos;
  else if (0u - (size_t)pos > len) return 0;
  else return (lua_Integer)len + pos + 1;
}


/*
** Decode an UTF-8 sequence, returning NULL if byte sequence is invalid.
*/
static const char *utf8_decode (const char *o, int *val) {
  static unsigned int limits[] = {0xFF, 0x7F, 0x7FF, 0xFFFF};
  const unsigned char *s = (const unsigned char *)o;
  unsigned int c = s[0];
  unsigned int res = 0;  /* final result */
  if (c < 0x80)  /* ascii? */
    res = c;
  else {
    int count = 0;  /* to count number of continuation bytes */
    while (c & 0x40) {  /* still have continuation bytes? */
      int cc = s[++count];  /* read next byte */
      if ((cc & 0xC0) != 0x80)  /* not a continuation byte? */
        return NULL;  /* invalid byte sequence */
      res = (res << 6) | (cc & 0x3F);  /* add lower 6 bits from cont. byte */
      c <<= 1;  /* to test next bit */
    }
    res |= ((c & 0x7F) << (count * 5));  /* add first byte */
    if (count > 3 || res > MAXUNICODE || res <= limits[count])
      return NULL;  /* invalid byte sequence */
    s += count;  /* skip continuation bytes read */
  }
  if (val) *val = res;
  return (const char *)s + 1;  /* +1 to include first byte */
}


/*
** utf8len(s, [i])   --> number of codepoints in 's' after 'i';
** nil if 's' not well formed
*/
static int utflen (lua_State *L) {
  int n = 0;
  const char *ends;
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  lua_Integer posi = u_posrelat(luaL_optinteger(L, 2, 1), 1);
  luaL_argcheck(L, 1 <= posi && posi <= (lua_Integer)len, 1,
                   "initial position out of string");
  ends = s + len;
  s += posi - 1;
  while (s < ends && (s = utf8_decode(s, NULL)) != NULL)
    n++;
  if (s == ends)
    lua_pushinteger(L, n);
  else
    lua_pushnil(L);
  return 1;
}


/*
** codepoint(s, [i, [j]])  -> returns codepoints for all characters
** between i and j
*/
static int codepoint (lua_State *L) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  lua_Integer posi = u_posrelat(luaL_optinteger(L, 2, 1), len);
  lua_Integer pose = u_posrelat(luaL_optinteger(L, 3, posi), len);
  int n;
  const char *se;
  luaL_argcheck(L, posi >= 1, 2, "out of range");
  luaL_argcheck(L, pose <= (lua_Integer)len, 3, "out of range");
  if (posi > pose) return 0;  /* empty interval; return no values */
  n = (int)(pose -  posi + 1);
  if (posi + n <= pose)  /* (lua_Integer -> int) overflow? */
    return luaL_error(L, "string slice too long");
  luaL_checkstack(L, n, "string slice too long");
  n = 0;
  se = s + pose;
  for (s += posi - 1; s < se;) {
    int code;
    s = utf8_decode(s, &code);
    if (s == NULL)
      return luaL_error(L, "invalid UTF-8 code");
    lua_pushinteger(L, code);
    n++;
  }
  return n;
}


static void pushutfchar (lua_State *L, int arg) {
  int code = luaL_checkint(L, arg);
  luaL_argcheck(L, 0 <= code && code <= MAXUNICODE, arg, "value out of range");
  lua_pushfstring(L, "%U", code);
}


/*
** utfchar(n1, n2, ...)  -> char(n1)..char(n2)...
*/
static int utfchar (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  if (n == 1)  /* optimize common case of single char */
    pushutfchar(L, 1);
  else {
    int i;
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (i = 1; i <= n; i++) {
      pushutfchar(L, i);
      luaL_addvalue(&b);
    }
    luaL_pushresult(&b);
  }
  return 1;
}


/*
** offset(s, n, [i])  -> index where n-th character *after*
**   position 'i' starts; 0 means character at 'i'.
*/
static int byteoffset (lua_State *L) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  int n  = luaL_checkint(L, 2);
  lua_Integer posi = u_posrelat(luaL_optinteger(L, 3, 1), len) - 1;
  luaL_argcheck(L, 0 <= posi && posi <= (lua_Integer)len, 3,
                   "position out of range");
  if (n == 0) {
    /* find beginning of current byte sequence */
    while (posi > 0 && iscont(s + posi)) posi--;
  }
  else if (n < 0) {
    while (n < 0 && posi > 0) {  /* move back */
      do {  /* find beginning of previous character */
        posi--;
      } while (posi > 0 && iscont(s + posi));
      n++;
    }
  }
  else {
    n--;  /* do not move for 1st character */
    while (n > 0 && posi < (lua_Integer)len) {
      do {  /* find beginning of next character */
        posi++;
      } while (iscont(s + posi));  /* ('\0' is not continuation) */
      n--;
    }
  }
  if (n == 0)
    lua_pushinteger(L, posi + 1);
  else
    lua_pushnil(L);  /* no such position */
  return 1;  
}


static int iter_aux (lua_State *L) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  int n = lua_tointeger(L, 2) - 1;
  if (n < 0)  /* first iteration? */
    n = 0;  /* start from here */
  else if (n < (lua_Integer)len) {
    n++;  /* skip current byte */
    while (iscont(s + n)) n++;  /* and its continuations */
  }
  if (n >= (lua_Integer)len)
    return 0;  /* no more codepoints */
  else {
    int code;
    const char *next = utf8_decode(s + n, &code);
    if (next == NULL || iscont(next))
      return luaL_error(L, "invalid UTF-8 code");
    lua_pushinteger(L, n + 1);
    lua_pushinteger(L, code);
    return 2;
  }
}


static int iter_codes (lua_State *L) {
  luaL_checkstring(L, 1);
  lua_pushcfunction(L, iter_aux);
  lua_pushvalue(L, 1);
  lua_pushinteger(L, 0);
  return 3;
}


/* pattern to match a single UTF-8 character */
#define UTF8PATT	"[\0-\x7F\xC2-\xF4][\x80-\xBF]*"


static struct luaL_Reg funcs[] = {
  {"offset", byteoffset},
  {"codepoint", codepoint},
  {"char", utfchar},
  {"len", utflen},
  {"codes", iter_codes},
  {NULL, NULL}
};


int luaopen_utf8 (lua_State *L) {
  luaL_newlib(L, funcs);
  lua_pushliteral(L, UTF8PATT);
  lua_setfield(L, -2, "charpatt");
  return 1;
}

