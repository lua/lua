/*
** $Id: lstrlib.c,v 1.56 2000/10/27 16:15:53 roberto Exp $
** Standard library for string operations and pattern-matching
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"



static int str_len (lua_State *L) {
  size_t l;
  luaL_check_lstr(L, 1, &l);
  lua_pushnumber(L, l);
  return 1;
}


static long posrelat (long pos, size_t len) {
  /* relative string position: negative means back from end */
  return (pos>=0) ? pos : (long)len+pos+1;
}


static int str_sub (lua_State *L) {
  size_t l;
  const char *s = luaL_check_lstr(L, 1, &l);
  long start = posrelat(luaL_check_long(L, 2), l);
  long end = posrelat(luaL_opt_long(L, 3, -1), l);
  if (start < 1) start = 1;
  if (end > (long)l) end = l;
  if (start <= end)
    lua_pushlstring(L, s+start-1, end-start+1);
  else lua_pushstring(L, "");
  return 1;
}


static int str_lower (lua_State *L) {
  size_t l;
  size_t i;
  luaL_Buffer b;
  const char *s = luaL_check_lstr(L, 1, &l);
  luaL_buffinit(L, &b);
  for (i=0; i<l; i++)
    luaL_putchar(&b, tolower((unsigned char)(s[i])));
  luaL_pushresult(&b);
  return 1;
}


static int str_upper (lua_State *L) {
  size_t l;
  size_t i;
  luaL_Buffer b;
  const char *s = luaL_check_lstr(L, 1, &l);
  luaL_buffinit(L, &b);
  for (i=0; i<l; i++)
    luaL_putchar(&b, toupper((unsigned char)(s[i])));
  luaL_pushresult(&b);
  return 1;
}

static int str_rep (lua_State *L) {
  size_t l;
  luaL_Buffer b;
  const char *s = luaL_check_lstr(L, 1, &l);
  int n = luaL_check_int(L, 2);
  luaL_buffinit(L, &b);
  while (n-- > 0)
    luaL_addlstring(&b, s, l);
  luaL_pushresult(&b);
  return 1;
}


static int str_byte (lua_State *L) {
  size_t l;
  const char *s = luaL_check_lstr(L, 1, &l);
  long pos = posrelat(luaL_opt_long(L, 2, 1), l);
  luaL_arg_check(L, 0<pos && (size_t)pos<=l, 2,  "out of range");
  lua_pushnumber(L, (unsigned char)s[pos-1]);
  return 1;
}


static int str_char (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for (i=1; i<=n; i++) {
    int c = luaL_check_int(L, i);
    luaL_arg_check(L, (unsigned char)c == c, i, "invalid value");
    luaL_putchar(&b, (unsigned char)c);
  }
  luaL_pushresult(&b);
  return 1;
}



/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/

#ifndef MAX_CAPTURES
#define MAX_CAPTURES 32  /* arbitrary limit */
#endif


struct Capture {
  const char *src_end;  /* end ('\0') of source string */
  int level;  /* total number of captures (finished or unfinished) */
  struct {
    const char *init;
    long len;  /* -1 signals unfinished capture */
  } capture[MAX_CAPTURES];
};


#define ESC		'%'
#define SPECIALS	"^$*+?.([%-"


static int check_capture (lua_State *L, int l, struct Capture *cap) {
  l -= '1';
  if (!(0 <= l && l < cap->level && cap->capture[l].len != -1))
    lua_error(L, "invalid capture index");
  return l;
}


static int capture_to_close (lua_State *L, struct Capture *cap) {
  int level = cap->level;
  for (level--; level>=0; level--)
    if (cap->capture[level].len == -1) return level;
  lua_error(L, "invalid pattern capture");
  return 0;  /* to avoid warnings */
}


const char *luaI_classend (lua_State *L, const char *p) {
  switch (*p++) {
    case ESC:
      if (*p == '\0') lua_error(L, "malformed pattern (ends with `%')");
      return p+1;
    case '[':
      if (*p == '^') p++;
      do {  /* look for a ']' */
        if (*p == '\0') lua_error(L, "malformed pattern (missing `]')");
        if (*(p++) == ESC && *p != '\0') p++;  /* skip escapes (e.g. '%]') */
      } while (*p != ']');
      return p+1;
    default:
      return p;
  }
}


static int match_class (int c, int cl) {
  int res;
  switch (tolower(cl)) {
    case 'a' : res = isalpha(c); break;
    case 'c' : res = iscntrl(c); break;
    case 'd' : res = isdigit(c); break;
    case 'l' : res = islower(c); break;
    case 'p' : res = ispunct(c); break;
    case 's' : res = isspace(c); break;
    case 'u' : res = isupper(c); break;
    case 'w' : res = isalnum(c); break;
    case 'x' : res = isxdigit(c); break;
    case 'z' : res = (c == '\0'); break;
    default: return (cl == c);
  }
  return (islower(cl) ? res : !res);
}



static int matchbracketclass (int c, const char *p, const char *endclass) {
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the '^' */
  }
  while (++p < endclass) {
    if (*p == ESC) {
      p++;
      if (match_class(c, (unsigned char)*p))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < endclass)) {
      p+=2;
      if ((int)(unsigned char)*(p-2) <= c && c <= (int)(unsigned char)*p)
        return sig;
    }
    else if ((int)(unsigned char)*p == c) return sig;
  }
  return !sig;
}



int luaI_singlematch (int c, const char *p, const char *ep) {
  switch (*p) {
    case '.':  /* matches any char */
      return 1;
    case ESC:
      return match_class(c, (unsigned char)*(p+1));
    case '[':
      return matchbracketclass(c, p, ep-1);
    default:
      return ((unsigned char)*p == c);
  }
}


static const char *match (lua_State *L, const char *s, const char *p,
                          struct Capture *cap);


static const char *matchbalance (lua_State *L, const char *s, const char *p,
                                 struct Capture *cap) {
  if (*p == 0 || *(p+1) == 0)
    lua_error(L, "unbalanced pattern");
  if (*s != *p) return NULL;
  else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < cap->src_end) {
      if (*s == e) {
        if (--cont == 0) return s+1;
      }
      else if (*s == b) cont++;
    }
  }
  return NULL;  /* string ends out of balance */
}


static const char *max_expand (lua_State *L, const char *s, const char *p,
                               const char *ep, struct Capture *cap) {
  long i = 0;  /* counts maximum expand for item */
  while ((s+i)<cap->src_end && luaI_singlematch((unsigned char)*(s+i), p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(L, (s+i), ep+1, cap);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


static const char *min_expand (lua_State *L, const char *s, const char *p,
                               const char *ep, struct Capture *cap) {
  for (;;) {
    const char *res = match(L, s, ep+1, cap);
    if (res != NULL)
      return res;
    else if (s<cap->src_end && luaI_singlematch((unsigned char)*s, p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


static const char *start_capture (lua_State *L, const char *s, const char *p,
                                  struct Capture *cap) {
  const char *res;
  int level = cap->level;
  if (level >= MAX_CAPTURES) lua_error(L, "too many captures");
  cap->capture[level].init = s;
  cap->capture[level].len = -1;
  cap->level = level+1;
  if ((res=match(L, s, p+1, cap)) == NULL)  /* match failed? */
    cap->level--;  /* undo capture */
  return res;
}


static const char *end_capture (lua_State *L, const char *s, const char *p,
                                struct Capture *cap) {
  int l = capture_to_close(L, cap);
  const char *res;
  cap->capture[l].len = s - cap->capture[l].init;  /* close capture */
  if ((res = match(L, s, p+1, cap)) == NULL)  /* match failed? */
    cap->capture[l].len = -1;  /* undo capture */
  return res;
}


static const char *match_capture (lua_State *L, const char *s, int level,
                                  struct Capture *cap) {
  int l = check_capture(L, level, cap);
  size_t len = cap->capture[l].len;
  if ((size_t)(cap->src_end-s) >= len &&
      memcmp(cap->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}


static const char *match (lua_State *L, const char *s, const char *p,
                          struct Capture *cap) {
  init: /* using goto's to optimize tail recursion */
  switch (*p) {
    case '(':  /* start capture */
      return start_capture(L, s, p, cap);
    case ')':  /* end capture */
      return end_capture(L, s, p, cap);
    case ESC:  /* may be %[0-9] or %b */
      if (isdigit((unsigned char)(*(p+1)))) {  /* capture? */
        s = match_capture(L, s, *(p+1), cap);
        if (s == NULL) return NULL;
        p+=2; goto init;  /* else return match(L, s, p+2, cap) */
      }
      else if (*(p+1) == 'b') {  /* balanced string? */
        s = matchbalance(L, s, p+2, cap);
        if (s == NULL) return NULL;
        p+=4; goto init;  /* else return match(L, s, p+4, cap); */
      }
      else goto dflt;  /* case default */
    case '\0':  /* end of pattern */
      return s;  /* match succeeded */
    case '$':
      if (*(p+1) == '\0')  /* is the '$' the last char in pattern? */
        return (s == cap->src_end) ? s : NULL;  /* check end of string */
      else goto dflt;
    default: dflt: {  /* it is a pattern item */
      const char *ep = luaI_classend(L, p);  /* points to what is next */
      int m = s<cap->src_end && luaI_singlematch((unsigned char)*s, p, ep);
      switch (*ep) {
        case '?': {  /* optional */
          const char *res;
          if (m && ((res=match(L, s+1, ep+1, cap)) != NULL))
            return res;
          p=ep+1; goto init;  /* else return match(L, s, ep+1, cap); */
        }
        case '*':  /* 0 or more repetitions */
          return max_expand(L, s, p, ep, cap);
        case '+':  /* 1 or more repetitions */
          return (m ? max_expand(L, s+1, p, ep, cap) : NULL);
        case '-':  /* 0 or more repetitions (minimum) */
          return min_expand(L, s, p, ep, cap);
        default:
          if (!m) return NULL;
          s++; p=ep; goto init;  /* else return match(L, s+1, ep, cap); */
      }
    }
  }
}



static const char *lmemfind (const char *s1, size_t l1,
                             const char *s2, size_t l2) {
  if (l2 == 0) return s1;  /* empty strings are everywhere */
  else if (l2 > l1) return NULL;  /* avoids a negative `l1' */
  else {
    const char *init;  /* to search for a `*s2' inside `s1' */
    l2--;  /* 1st char will be checked by `memchr' */
    l1 = l1-l2;  /* `s2' cannot be found after that */
    while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
      init++;   /* 1st char is already checked */
      if (memcmp(init, s2+1, l2) == 0)
        return init-1;
      else {  /* correct `l1' and `s1' to try again */
        l1 -= init-s1;
        s1 = init;
      }
    }
    return NULL;  /* not found */
  }
}


static int push_captures (lua_State *L, struct Capture *cap) {
  int i;
  luaL_checkstack(L, cap->level, "too many captures");
  for (i=0; i<cap->level; i++) {
    int l = cap->capture[i].len;
    if (l == -1) lua_error(L, "unfinished capture");
    lua_pushlstring(L, cap->capture[i].init, l);
  }
  return cap->level;  /* number of strings pushed */
}


static int str_find (lua_State *L) {
  size_t l1, l2;
  const char *s = luaL_check_lstr(L, 1, &l1);
  const char *p = luaL_check_lstr(L, 2, &l2);
  long init = posrelat(luaL_opt_long(L, 3, 1), l1) - 1;
  struct Capture cap;
  luaL_arg_check(L, 0 <= init && (size_t)init <= l1, 3, "out of range");
  if (lua_gettop(L) > 3 ||  /* extra argument? */
      strpbrk(p, SPECIALS) == NULL) {  /* or no special characters? */
    const char *s2 = lmemfind(s+init, l1-init, p, l2);
    if (s2) {
      lua_pushnumber(L, s2-s+1);
      lua_pushnumber(L, s2-s+l2);
      return 2;
    }
  }
  else {
    int anchor = (*p == '^') ? (p++, 1) : 0;
    const char *s1=s+init;
    cap.src_end = s+l1;
    do {
      const char *res;
      cap.level = 0;
      if ((res=match(L, s1, p, &cap)) != NULL) {
        lua_pushnumber(L, s1-s+1);  /* start */
        lua_pushnumber(L, res-s);   /* end */
        return push_captures(L, &cap) + 2;
      }
    } while (s1++<cap.src_end && !anchor);
  }
  lua_pushnil(L);  /* not found */
  return 1;
}


static void add_s (lua_State *L, luaL_Buffer *b, struct Capture *cap) {
  if (lua_isstring(L, 3)) {
    const char *news = lua_tostring(L, 3);
    size_t l = lua_strlen(L, 3);
    size_t i;
    for (i=0; i<l; i++) {
      if (news[i] != ESC)
        luaL_putchar(b, news[i]);
      else {
        i++;  /* skip ESC */
        if (!isdigit((unsigned char)news[i]))
          luaL_putchar(b, news[i]);
        else {
          int level = check_capture(L, news[i], cap);
          luaL_addlstring(b, cap->capture[level].init, cap->capture[level].len);
        }
      }
    }
  }
  else {  /* is a function */
    int n;
    lua_pushvalue(L, 3);
    n = push_captures(L, cap);
    lua_rawcall(L, n, 1);
    if (lua_isstring(L, -1))
      luaL_addvalue(b);  /* add return to accumulated result */
    else
      lua_pop(L, 1);  /* function result is not a string: pop it */
  }
}


static int str_gsub (lua_State *L) {
  size_t srcl;
  const char *src = luaL_check_lstr(L, 1, &srcl);
  const char *p = luaL_check_string(L, 2);
  int max_s = luaL_opt_int(L, 4, srcl+1);
  int anchor = (*p == '^') ? (p++, 1) : 0;
  int n = 0;
  struct Capture cap;
  luaL_Buffer b;
  luaL_arg_check(L,
    lua_gettop(L) >= 3 && (lua_isstring(L, 3) || lua_isfunction(L, 3)),
    3, "string or function expected");
  luaL_buffinit(L, &b);
  cap.src_end = src+srcl;
  while (n < max_s) {
    const char *e;
    cap.level = 0;
    e = match(L, src, p, &cap);
    if (e) {
      n++;
      add_s(L, &b, &cap);
    }
    if (e && e>src) /* non empty match? */
      src = e;  /* skip it */
    else if (src < cap.src_end)
      luaL_putchar(&b, *src++);
    else break;
    if (anchor) break;
  }
  luaL_addlstring(&b, src, cap.src_end-src);
  luaL_pushresult(&b);
  lua_pushnumber(L, n);  /* number of substitutions */
  return 2;
}

/* }====================================================== */


static void luaI_addquoted (lua_State *L, luaL_Buffer *b, int arg) {
  size_t l;
  const char *s = luaL_check_lstr(L, arg, &l);
  luaL_putchar(b, '"');
  while (l--) {
    switch (*s) {
      case '"':  case '\\':  case '\n':
        luaL_putchar(b, '\\');
        luaL_putchar(b, *s);
        break;
      case '\0': luaL_addlstring(b, "\\000", 4); break;
      default: luaL_putchar(b, *s);
    }
    s++;
  }
  luaL_putchar(b, '"');
}

/* maximum size of each formatted item (> len(format('%99.99f', -1e308))) */
#define MAX_ITEM	512
/* maximum size of each format specification (such as '%-099.99d') */
#define MAX_FORMAT	20

static int str_format (lua_State *L) {
  int arg = 1;
  const char *strfrmt = luaL_check_string(L, arg);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (*strfrmt) {
    if (*strfrmt != '%')
      luaL_putchar(&b, *strfrmt++);
    else if (*++strfrmt == '%')
      luaL_putchar(&b, *strfrmt++);  /* %% */
    else { /* format item */
      struct Capture cap;
      char form[MAX_FORMAT];  /* to store the format ('%...') */
      char buff[MAX_ITEM];  /* to store the formatted item */
      const char *initf = strfrmt;
      form[0] = '%';
      if (isdigit((unsigned char)*initf) && *(initf+1) == '$') {
        arg = *initf - '0';
        initf += 2;  /* skip the 'n$' */
      }
      arg++;
      cap.src_end = strfrmt+strlen(strfrmt)+1;
      cap.level = 0;
      strfrmt = match(L, initf, "[-+ #0]*(%d*)%.?(%d*)", &cap);
      if (cap.capture[0].len > 2 || cap.capture[1].len > 2 ||  /* < 100? */
          strfrmt-initf > MAX_FORMAT-2)
        lua_error(L, "invalid format (width or precision too long)");
      strncpy(form+1, initf, strfrmt-initf+1); /* +1 to include conversion */
      form[strfrmt-initf+2] = 0;
      switch (*strfrmt++) {
        case 'c':  case 'd':  case 'i':
          sprintf(buff, form, luaL_check_int(L, arg));
          break;
        case 'o':  case 'u':  case 'x':  case 'X':
          sprintf(buff, form, (unsigned int)luaL_check_number(L, arg));
          break;
        case 'e':  case 'E': case 'f': case 'g': case 'G':
          sprintf(buff, form, luaL_check_number(L, arg));
          break;
        case 'q':
          luaI_addquoted(L, &b, arg);
          continue;  /* skip the "addsize" at the end */
        case 's': {
          size_t l;
          const char *s = luaL_check_lstr(L, arg, &l);
          if (cap.capture[1].len == 0 && l >= 100) {
            /* no precision and string is too long to be formatted;
               keep original string */
            lua_pushvalue(L, arg);
            luaL_addvalue(&b);
            continue;  /* skip the "addsize" at the end */
          }
          else {
            sprintf(buff, form, s);
            break;
          }
        }
        default:  /* also treat cases 'pnLlh' */
          lua_error(L, "invalid option in `format'");
      }
      luaL_addlstring(&b, buff, strlen(buff));
    }
  }
  luaL_pushresult(&b);
  return 1;
}


static const struct luaL_reg strlib[] = {
{"strlen", str_len},
{"strsub", str_sub},
{"strlower", str_lower},
{"strupper", str_upper},
{"strchar", str_char},
{"strrep", str_rep},
{"ascii", str_byte},  /* for compatibility with 3.0 and earlier */
{"strbyte", str_byte},
{"format", str_format},
{"strfind", str_find},
{"gsub", str_gsub}
};


/*
** Open string library
*/
LUALIB_API void lua_strlibopen (lua_State *L) {
  luaL_openl(L, strlib);
}
