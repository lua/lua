/*
** $Id: lstrlib.c,v 1.37 1999/11/22 13:12:07 roberto Exp roberto $
** Standard library for strings and pattern-matching
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_REENTRANT

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"



static void addnchar (lua_State *L, const char *s, int n) {
  char *b = luaL_openspace(L, n);
  memcpy(b, s, n);
  luaL_addsize(L, n);
}


static void str_len (lua_State *L) {
  long l;
  luaL_check_lstr(L, 1, &l);
  lua_pushnumber(L, l);
}


static void closeandpush (lua_State *L) {
  lua_pushlstring(L, luaL_buffer(L), luaL_getsize(L));
}


static long posrelat (long pos, long len) {
  /* relative string position: negative means back from end */
  return (pos>=0) ? pos : len+pos+1;
}


static void str_sub (lua_State *L) {
  long l;
  const char *s = luaL_check_lstr(L, 1, &l);
  long start = posrelat(luaL_check_long(L, 2), l);
  long end = posrelat(luaL_opt_long(L, 3, -1), l);
  if (start < 1) start = 1;
  if (end > l) end = l;
  if (start <= end)
    lua_pushlstring(L, s+start-1, end-start+1);
  else lua_pushstring(L, "");
}


static void str_lower (lua_State *L) {
  long l;
  int i;
  const char *s = luaL_check_lstr(L, 1, &l);
  luaL_resetbuffer(L);
  for (i=0; i<l; i++)
    luaL_addchar(L, tolower((unsigned char)(s[i])));
  closeandpush(L);
}


static void str_upper (lua_State *L) {
  long l;
  int i;
  const char *s = luaL_check_lstr(L, 1, &l);
  luaL_resetbuffer(L);
  for (i=0; i<l; i++)
    luaL_addchar(L, toupper((unsigned char)(s[i])));
  closeandpush(L);
}

static void str_rep (lua_State *L) {
  long l;
  const char *s = luaL_check_lstr(L, 1, &l);
  int n = luaL_check_int(L, 2);
  luaL_resetbuffer(L);
  while (n-- > 0)
    addnchar(L, s, l);
  closeandpush(L);
}


static void str_byte (lua_State *L) {
  long l;
  const char *s = luaL_check_lstr(L, 1, &l);
  long pos = posrelat(luaL_opt_long(L, 2, 1), l);
  luaL_arg_check(L, 0<pos && pos<=l, 2,  "out of range");
  lua_pushnumber(L, (unsigned char)s[pos-1]);
}


static void str_char (lua_State *L) {
  int i = 0;
  luaL_resetbuffer(L);
  while (lua_getparam(L, ++i) != LUA_NOOBJECT) {
    int c = luaL_check_int(L, i);
    luaL_arg_check(L, (unsigned char)c == c, i, "invalid value");
    luaL_addchar(L, (unsigned char)c);
  }
  closeandpush(L);
}



/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/

#ifndef MAX_CAPT
#define MAX_CAPT 32  /* arbitrary limit */
#endif


struct Capture {
  const char *src_end;  /* end ('\0') of source string */
  int level;  /* total number of captures (finished or unfinished) */
  struct {
    const char *init;
    int len;  /* -1 signals unfinished capture */
  } capture[MAX_CAPT];
};


#define ESC	'%'
#define SPECIALS  "^$*+?.([%-"


static void push_captures (lua_State *L, struct Capture *cap) {
  int i;
  for (i=0; i<cap->level; i++) {
    int l = cap->capture[i].len;
    if (l == -1) lua_error(L, "unfinished capture");
    lua_pushlstring(L, cap->capture[i].init, l);
  }
}


static int check_cap (lua_State *L, int l, struct Capture *cap) {
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
      if (*p == '\0') lua_error(L, "incorrect pattern (ends with `%')");
      return p+1;
    case '[':
      if (*p == '^') p++;
      do {  /* look for a ']' */
        if (*p == '\0') lua_error(L, "incorrect pattern (missing `]')");
        if (*(p++) == ESC && *p != '\0') p++;  /* skip escapes (e.g. '%]') */
      } while (*p != ']');
      return p+1;
    default:
      return p;
  }
}


static int matchclass (int c, int cl) {
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
      if (matchclass(c, (unsigned char)*p))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < endclass)) {
      p+=2;
      if ((int)(unsigned char)*(p-2) <= c && c <= (int)(unsigned char)*p)
        return sig;
    }
    else if ((unsigned char)*p == c) return sig;
  }
  return !sig;
}



int luaI_singlematch (int c, const char *p, const char *ep) {
  switch (*p) {
    case '.':  /* matches any char */
      return 1;
    case ESC:
      return matchclass(c, (unsigned char)*(p+1));
    case '[':
      return matchbracketclass(c, p, ep-1);
    default:
      return ((unsigned char)*p == c);
  }
}


static const char *match (lua_State *L, const char *s, const char *p, struct Capture *cap);


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


static const char *max_expand (lua_State *L, const char *s, const char *p, const char *ep,
                         struct Capture *cap) {
  int i = 0;  /* counts maximum expand for item */
  while ((s+i)<cap->src_end && luaI_singlematch((unsigned char)*(s+i), p, ep))
    i++;
  /* keeps trying to match mith the maximum repetitions */
  while (i>=0) {
    const char *res = match(L, (s+i), ep+1, cap);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


static const char *min_expand (lua_State *L, const char *s, const char *p, const char *ep,
                         struct Capture *cap) {
  for (;;) {
    const char *res = match(L, s, ep+1, cap);
    if (res != NULL)
      return res;
    else if (s<cap->src_end && luaI_singlematch((unsigned char)*s, p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


static const char *start_capt (lua_State *L, const char *s, const char *p,
                               struct Capture *cap) {
  const char *res;
  int level = cap->level;
  if (level >= MAX_CAPT) lua_error(L, "too many captures");
  cap->capture[level].init = s;
  cap->capture[level].len = -1;
  cap->level = level+1;
  if ((res=match(L, s, p+1, cap)) == NULL)  /* match failed? */
    cap->level--;  /* undo capture */
  return res;
}


static const char *end_capt (lua_State *L, const char *s, const char *p,
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
  int l = check_cap(L, level, cap);
  int len = cap->capture[l].len;
  if (cap->src_end-s >= len &&
      memcmp(cap->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}


static const char *match (lua_State *L, const char *s, const char *p, struct Capture *cap) {
  init: /* using goto's to optimize tail recursion */
  switch (*p) {
    case '(':  /* start capture */
      return start_capt(L, s, p, cap);
    case ')':  /* end capture */
      return end_capt(L, s, p, cap);
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



static const char *memfind (const char *s1, long l1, const char *s2, long l2) {
  if (l2 == 0) return s1;  /* empty strings are everywhere ;-) */
  else {
    const char *init;  /* to search for a `*s2' inside `s1' */
    l2--;  /* 1st char will be checked by `memchr' */
    l1 = l1-l2;  /* `s2' cannot be found after that */
    while (l1 > 0 && (init = memchr(s1, *s2, l1)) != NULL) {
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


static void str_find (lua_State *L) {
  long l1, l2;
  const char *s = luaL_check_lstr(L, 1, &l1);
  const char *p = luaL_check_lstr(L, 2, &l2);
  long init = posrelat(luaL_opt_long(L, 3, 1), l1) - 1;
  struct Capture cap;
  luaL_arg_check(L, 0 <= init && init <= l1, 3, "out of range");
  if (lua_getparam(L, 4) != LUA_NOOBJECT ||
      strpbrk(p, SPECIALS) == NULL) {  /* no special characters? */
    const char *s2 = memfind(s+init, l1, p, l2);
    if (s2) {
      lua_pushnumber(L, s2-s+1);
      lua_pushnumber(L, s2-s+l2);
      return;
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
        push_captures(L, &cap);
        return;
      }
    } while (s1++<cap.src_end && !anchor);
  }
  lua_pushnil(L);  /* not found */
}


static void add_s (lua_State *L, lua_Object newp, struct Capture *cap) {
  if (lua_isstring(L, newp)) {
    const char *news = lua_getstring(L, newp);
    int l = lua_strlen(L, newp);
    int i;
    for (i=0; i<l; i++) {
      if (news[i] != ESC)
        luaL_addchar(L, news[i]);
      else {
        i++;  /* skip ESC */
        if (!isdigit((unsigned char)news[i]))
          luaL_addchar(L, news[i]);
        else {
          int level = check_cap(L, news[i], cap);
          addnchar(L, cap->capture[level].init, cap->capture[level].len);
        }
      }
    }
  }
  else {  /* is a function */
    lua_Object res;
    int status;
    int oldbuff;
    lua_beginblock(L);
    push_captures(L, cap);
    /* function may use buffer, so save it and create a new one */
    oldbuff = luaL_newbuffer(L, 0);
    status = lua_callfunction(L, newp);
    /* restore old buffer */
    luaL_oldbuffer(L, oldbuff);
    if (status != 0) {
      lua_endblock(L);
      lua_error(L, NULL);
    }
    res = lua_getresult(L, 1);
    if (lua_isstring(L, res))
      addnchar(L, lua_getstring(L, res), lua_strlen(L, res));
    lua_endblock(L);
  }
}


static void str_gsub (lua_State *L) {
  long srcl;
  const char *src = luaL_check_lstr(L, 1, &srcl);
  const char *p = luaL_check_string(L, 2);
  lua_Object newp = lua_getparam(L, 3);
  int max_s = luaL_opt_int(L, 4, srcl+1);
  int anchor = (*p == '^') ? (p++, 1) : 0;
  int n = 0;
  struct Capture cap;
  luaL_arg_check(L, lua_isstring(L, newp) || lua_isfunction(L, newp), 3,
                 "string or function expected");
  luaL_resetbuffer(L);
  cap.src_end = src+srcl;
  while (n < max_s) {
    const char *e;
    cap.level = 0;
    e = match(L, src, p, &cap);
    if (e) {
      n++;
      add_s(L, newp, &cap);
    }
    if (e && e>src) /* non empty match? */
      src = e;  /* skip it */
    else if (src < cap.src_end)
      luaL_addchar(L, *src++);
    else break;
    if (anchor) break;
  }
  addnchar(L, src, cap.src_end-src);
  closeandpush(L);
  lua_pushnumber(L, n);  /* number of substitutions */
}

/* }====================================================== */


static void luaI_addquoted (lua_State *L, int arg) {
  long l;
  const char *s = luaL_check_lstr(L, arg, &l);
  luaL_addchar(L, '"');
  while (l--) {
    switch (*s) {
      case '"':  case '\\':  case '\n':
        luaL_addchar(L, '\\');
        luaL_addchar(L, *s);
        break;
      case '\0': addnchar(L, "\\000", 4); break;
      default: luaL_addchar(L, *s);
    }
    s++;
  }
  luaL_addchar(L, '"');
}

/* maximum size of each format specification (such as '%-099.99d') */
#define MAX_FORMAT 20  /* arbitrary limit */

static void str_format (lua_State *L) {
  int arg = 1;
  const char *strfrmt = luaL_check_string(L, arg);
  luaL_resetbuffer(L);
  while (*strfrmt) {
    if (*strfrmt != '%')
      luaL_addchar(L, *strfrmt++);
    else if (*++strfrmt == '%')
      luaL_addchar(L, *strfrmt++);  /* %% */
    else { /* format item */
      struct Capture cap;
      char form[MAX_FORMAT];  /* to store the format ('%...') */
      char *buff;  /* to store the formatted item */
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
      buff = luaL_openspace(L, 512);  /* 512 > len(format('%99.99f', -1e308)) */
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
          luaI_addquoted(L, arg);
          continue;  /* skip the "addsize" at the end */
        case 's': {
          long l;
          const char *s = luaL_check_lstr(L, arg, &l);
          if (cap.capture[1].len == 0 && l >= 100) {
            /* no precision and string is too big to be formatted;
               keep original string */
            addnchar(L, s, l);
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
      luaL_addsize(L, strlen(buff));
    }
  }
  closeandpush(L);  /* push the result */
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
void lua_strlibopen (lua_State *L) {
  luaL_openl(L, strlib);
}
