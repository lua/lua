/*
** $Id: lstrlib.c,v 1.60 2000/12/18 13:41:41 roberto Exp roberto $
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


typedef long sint32;	/* a "signed" version for size_t */


static int str_len (lua_State *L) {
  size_t l;
  luaL_check_lstr(L, 1, &l);
  lua_pushnumber(L, l);
  return 1;
}


static sint32 posrelat (sint32 pos, size_t len) {
  /* relative string position: negative means back from end */
  return (pos>=0) ? pos : (sint32)len+pos+1;
}


static int str_sub (lua_State *L) {
  size_t l;
  const char *s = luaL_check_lstr(L, 1, &l);
  sint32 start = posrelat(luaL_check_long(L, 2), l);
  sint32 end = posrelat(luaL_opt_long(L, 3, -1), l);
  if (start < 1) start = 1;
  if (end > (sint32)l) end = l;
  if (start <= end)
    lua_pushlstring(L, s+start-1, end-start+1);
  else lua_pushliteral(L, "");
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
  sint32 pos = posrelat(luaL_opt_long(L, 2, 1), l);
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


#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)

typedef struct MatchState {
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end ('\0') of source string */
  int level;  /* total number of captures (finished or unfinished) */
  struct {
    const char *init;
    sint32 len;
  } capture[MAX_CAPTURES];
  lua_State *L;
} MatchState;


#define ESC		'%'
#define SPECIALS	"^$*+?.([%-"


static int check_capture (MatchState *ms, int l) {
  l -= '1';
  if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
    lua_error(ms->L, "invalid capture index");
  return l;
}


static int capture_to_close (MatchState *ms) {
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  lua_error(ms->L, "invalid pattern capture");
  return 0;  /* to avoid warnings */
}


static const char *luaI_classend (MatchState *ms, const char *p) {
  switch (*p++) {
    case ESC:
      if (*p == '\0') lua_error(ms->L, "malformed pattern (ends with `%')");
      return p+1;
    case '[':
      if (*p == '^') p++;
      do {  /* look for a ']' */
        if (*p == '\0') lua_error(ms->L, "malformed pattern (missing `]')");
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


static int luaI_singlematch (int c, const char *p, const char *ep) {
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


static const char *match (MatchState *ms, const char *s, const char *p);


static const char *matchbalance (MatchState *ms, const char *s, const char *p) {
  if (*p == 0 || *(p+1) == 0)
    lua_error(ms->L, "unbalanced pattern");
  if (*s != *p) return NULL;
  else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
        if (--cont == 0) return s+1;
      }
      else if (*s == b) cont++;
    }
  }
  return NULL;  /* string ends out of balance */
}


static const char *max_expand (MatchState *ms, const char *s, const char *p,
                               const char *ep) {
  sint32 i = 0;  /* counts maximum expand for item */
  while ((s+i)<ms->src_end && luaI_singlematch((unsigned char)*(s+i), p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(ms, (s+i), ep+1);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


static const char *min_expand (MatchState *ms, const char *s, const char *p,
                               const char *ep) {
  for (;;) {
    const char *res = match(ms, s, ep+1);
    if (res != NULL)
      return res;
    else if (s<ms->src_end && luaI_singlematch((unsigned char)*s, p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


static const char *start_capture (MatchState *ms, const char *s,
                                  const char *p, int what) {
  const char *res;
  int level = ms->level;
  if (level >= MAX_CAPTURES) lua_error(ms->L, "too many captures");
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}


static const char *end_capture (MatchState *ms, const char *s, const char *p) {
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}


static const char *match_capture (MatchState *ms, const char *s, int level) {
  int l = check_capture(ms, level);
  size_t len = ms->capture[l].len;
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}


static const char *match (MatchState *ms, const char *s, const char *p) {
  init: /* using goto's to optimize tail recursion */
  switch (*p) {
    case '(':  /* start capture */
      if (*(p+1) == ')')  /* position capture? */
        return start_capture(ms, s, p+2, CAP_POSITION);
      else
        return start_capture(ms, s, p+1, CAP_UNFINISHED);
    case ')':  /* end capture */
      return end_capture(ms, s, p+1);
    case ESC:  /* may be %[0-9] or %b */
      if (isdigit((unsigned char)(*(p+1)))) {  /* capture? */
        s = match_capture(ms, s, *(p+1));
        if (s == NULL) return NULL;
        p+=2; goto init;  /* else return match(ms, s, p+2) */
      }
      else if (*(p+1) == 'b') {  /* balanced string? */
        s = matchbalance(ms, s, p+2);
        if (s == NULL) return NULL;
        p+=4; goto init;  /* else return match(ms, s, p+4); */
      }
      else goto dflt;  /* case default */
    case '\0':  /* end of pattern */
      return s;  /* match succeeded */
    case '$':
      if (*(p+1) == '\0')  /* is the '$' the last char in pattern? */
        return (s == ms->src_end) ? s : NULL;  /* check end of string */
      else goto dflt;
    default: dflt: {  /* it is a pattern item */
      const char *ep = luaI_classend(ms, p);  /* points to what is next */
      int m = s<ms->src_end && luaI_singlematch((unsigned char)*s, p, ep);
      switch (*ep) {
        case '?': {  /* optional */
          const char *res;
          if (m && ((res=match(ms, s+1, ep+1)) != NULL))
            return res;
          p=ep+1; goto init;  /* else return match(ms, s, ep+1); */
        }
        case '*':  /* 0 or more repetitions */
          return max_expand(ms, s, p, ep);
        case '+':  /* 1 or more repetitions */
          return (m ? max_expand(ms, s+1, p, ep) : NULL);
        case '-':  /* 0 or more repetitions (minimum) */
          return min_expand(ms, s, p, ep);
        default:
          if (!m) return NULL;
          s++; p=ep; goto init;  /* else return match(ms, s+1, ep); */
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


static void push_onecapture (MatchState *ms, int i) {
  int l = ms->capture[i].len;
  if (l == CAP_UNFINISHED) lua_error(ms->L, "unfinished capture");
  if (l == CAP_POSITION)
    lua_pushnumber(ms->L, ms->capture[i].init - ms->src_init + 1);
  else
    lua_pushlstring(ms->L, ms->capture[i].init, l);
}


static int push_captures (MatchState *ms) {
  int i;
  luaL_checkstack(ms->L, ms->level, "too many captures");
  for (i=0; i<ms->level; i++)
    push_onecapture(ms, i);
  return ms->level;  /* number of strings pushed */
}


static int str_find (lua_State *L) {
  size_t l1, l2;
  const char *s = luaL_check_lstr(L, 1, &l1);
  const char *p = luaL_check_lstr(L, 2, &l2);
  sint32 init = posrelat(luaL_opt_long(L, 3, 1), l1) - 1;
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
    MatchState ms;
    int anchor = (*p == '^') ? (p++, 1) : 0;
    const char *s1=s+init;
    ms.L = L;
    ms.src_init = s;
    ms.src_end = s+l1;
    do {
      const char *res;
      ms.level = 0;
      if ((res=match(&ms, s1, p)) != NULL) {
        lua_pushnumber(L, s1-s+1);  /* start */
        lua_pushnumber(L, res-s);   /* end */
        return push_captures(&ms) + 2;
      }
    } while (s1++<ms.src_end && !anchor);
  }
  lua_pushnil(L);  /* not found */
  return 1;
}


static void add_s (MatchState *ms, luaL_Buffer *b) {
  lua_State *L = ms->L;
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
          int level = check_capture(ms, news[i]);
          push_onecapture(ms, level);
          luaL_addvalue(b);  /* add capture to accumulated result */
        }
      }
    }
  }
  else {  /* is a function */
    int n;
    lua_pushvalue(L, 3);
    n = push_captures(ms);
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
  MatchState ms;
  luaL_Buffer b;
  luaL_arg_check(L,
    lua_gettop(L) >= 3 && (lua_isstring(L, 3) || lua_isfunction(L, 3)),
    3, "string or function expected");
  luaL_buffinit(L, &b);
  ms.L = L;
  ms.src_init = src;
  ms.src_end = src+srcl;
  while (n < max_s) {
    const char *e;
    ms.level = 0;
    e = match(&ms, src, p);
    if (e) {
      n++;
      add_s(&ms, &b);
    }
    if (e && e>src) /* non empty match? */
      src = e;  /* skip it */
    else if (src < ms.src_end)
      luaL_putchar(&b, *src++);
    else break;
    if (anchor) break;
  }
  luaL_addlstring(&b, src, ms.src_end-src);
  luaL_pushresult(&b);
  lua_pushnumber(L, n);  /* number of substitutions */
  return 2;
}

/* }====================================================== */


/* maximum size of each formatted item (> len(format('%99.99f', -1e308))) */
#define MAX_ITEM	512
/* maximum size of each format specification (such as '%-099.99d') */
#define MAX_FORMAT	20


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


static const char *scanformat (lua_State *L, const char *strfrmt, char *form,
                               int *hasprecision) {
  const char *p = strfrmt;
  while (strchr("-+ #0", *p)) p++;  /* skip flags */
  if (isdigit((unsigned char)*p)) p++;  /* skip width */
  if (isdigit((unsigned char)*p)) p++;  /* (2 digits at most) */
  if (*p == '.') {
    p++;
    *hasprecision = 1;
    if (isdigit((unsigned char)*p)) p++;  /* skip precision */
    if (isdigit((unsigned char)*p)) p++;  /* (2 digits at most) */
  }
  if (isdigit((unsigned char)*p))
    lua_error(L, "invalid format (width or precision too long)");
  if (p-strfrmt+2 > MAX_FORMAT)  /* +2 to include `%' and the specifier */
    lua_error(L, "invalid format (too long)");
  form[0] = '%';
  strncpy(form+1, strfrmt, p-strfrmt+1);
  form[p-strfrmt+2] = 0;
  return p;
}


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
      char form[MAX_FORMAT];  /* to store the format (`%...') */
      char buff[MAX_ITEM];  /* to store the formatted item */
      int hasprecision = 0;
      if (isdigit((unsigned char)*strfrmt) && *(strfrmt+1) == '$')
        lua_error(L, "obsolete `format' option (d$)");
      arg++;
      strfrmt = scanformat(L, strfrmt, form, &hasprecision);
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
          if (!hasprecision && l >= 100) {
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
        default:  /* also treat cases `pnLlh' */
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
