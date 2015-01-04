/*
** $Id: lstrlib.c,v 1.32 1999/06/17 17:04:03 roberto Exp $
** Standard library for strings and pattern-matching
** See Copyright Notice in lua.h
*/


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"



static void addnchar (char *s, int n)
{
  char *b = luaL_openspace(n);
  memcpy(b, s, n);
  luaL_addsize(n);
}


static void str_len (void)
{
  long l;
  luaL_check_lstr(1, &l);
  lua_pushnumber(l);
}


static void closeandpush (void) {
  lua_pushlstring(luaL_buffer(), luaL_getsize());
}


static long posrelat (long pos, long len) {
  /* relative string position: negative means back from end */
  return (pos>=0) ? pos : len+pos+1;
}


static void str_sub (void) {
  long l;
  char *s = luaL_check_lstr(1, &l);
  long start = posrelat(luaL_check_long(2), l);
  long end = posrelat(luaL_opt_long(3, -1), l);
  if (start < 1) start = 1;
  if (end > l) end = l;
  if (start <= end)
    lua_pushlstring(s+start-1, end-start+1);
  else lua_pushstring("");
}


static void str_lower (void) {
  long l;
  int i;
  char *s = luaL_check_lstr(1, &l);
  luaL_resetbuffer();
  for (i=0; i<l; i++)
    luaL_addchar(tolower((unsigned char)(s[i])));
  closeandpush();
}


static void str_upper (void) {
  long l;
  int i;
  char *s = luaL_check_lstr(1, &l);
  luaL_resetbuffer();
  for (i=0; i<l; i++)
    luaL_addchar(toupper((unsigned char)(s[i])));
  closeandpush();
}

static void str_rep (void)
{
  long l;
  char *s = luaL_check_lstr(1, &l);
  int n = luaL_check_int(2);
  luaL_resetbuffer();
  while (n-- > 0)
    addnchar(s, l);
  closeandpush();
}


static void str_byte (void) {
  long l;
  char *s = luaL_check_lstr(1, &l);
  long pos = posrelat(luaL_opt_long(2, 1), l);
  luaL_arg_check(0<pos && pos<=l, 2,  "out of range");
  lua_pushnumber((unsigned char)s[pos-1]);
}


static void str_char (void) {
  int i = 0;
  luaL_resetbuffer();
  while (lua_getparam(++i) != LUA_NOOBJECT) {
    double c = luaL_check_number(i);
    luaL_arg_check((unsigned char)c == c, i, "invalid value");
    luaL_addchar((unsigned char)c);
  }
  closeandpush();
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
  char *src_end;  /* end ('\0') of source string */
  int level;  /* total number of captures (finished or unfinished) */
  struct {
    char *init;
    int len;  /* -1 signals unfinished capture */
  } capture[MAX_CAPT];
};


#define ESC	'%'
#define SPECIALS  "^$*+?.([%-"


static void push_captures (struct Capture *cap) {
  int i;
  for (i=0; i<cap->level; i++) {
    int l = cap->capture[i].len;
    if (l == -1) lua_error("unfinished capture");
    lua_pushlstring(cap->capture[i].init, l);
  }
}


static int check_cap (int l, struct Capture *cap) {
  l -= '1';
  if (!(0 <= l && l < cap->level && cap->capture[l].len != -1))
    lua_error("invalid capture index");
  return l;
}


static int capture_to_close (struct Capture *cap) {
  int level = cap->level;
  for (level--; level>=0; level--)
    if (cap->capture[level].len == -1) return level;
  lua_error("invalid pattern capture");
  return 0;  /* to avoid warnings */
}


char *luaI_classend (char *p) {
  switch (*p++) {
    case ESC:
      if (*p == '\0')
        luaL_verror("incorrect pattern (ends with `%c')", ESC);
      return p+1;
    case '[':
      if (*p == '^') p++;
      if (*p == ']') p++;
      p = strchr(p, ']');
      if (!p) lua_error("incorrect pattern (missing `]')");
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



static int matchbracketclass (int c, char *p, char *end) {
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the '^' */
  }
  while (++p < end) {
    if (*p == ESC) {
      p++;
      if ((p < end) && matchclass(c, (unsigned char)*p))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < end)) {
      p+=2;
      if ((int)(unsigned char)*(p-2) <= c && c <= (int)(unsigned char)*p)
        return sig;
    }
    else if ((unsigned char)*p == c) return sig;
  }
  return !sig;
}



int luaI_singlematch (int c, char *p, char *ep) {
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


static char *match (char *s, char *p, struct Capture *cap);


static char *matchbalance (char *s, char *p, struct Capture *cap) {
  if (*p == 0 || *(p+1) == 0)
    lua_error("unbalanced pattern");
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


static char *max_expand (char *s, char *p, char *ep, struct Capture *cap) {
  int i = 0;  /* counts maximum expand for item */
  while ((s+i)<cap->src_end && luaI_singlematch((unsigned char)*(s+i), p, ep))
    i++;
  /* keeps trying to match mith the maximum repetitions */
  while (i>=0) {
    char *res = match((s+i), ep+1, cap);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


static char *min_expand (char *s, char *p, char *ep, struct Capture *cap) {
  for (;;) {
    char *res = match(s, ep+1, cap);
    if (res != NULL)
      return res;
    else if (s<cap->src_end && luaI_singlematch((unsigned char)*s, p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


static char *start_capt (char *s, char *p, struct Capture *cap) {
  char *res;
  int level = cap->level;
  if (level >= MAX_CAPT) lua_error("too many captures");
  cap->capture[level].init = s;
  cap->capture[level].len = -1;
  cap->level = level+1;
  if ((res=match(s, p+1, cap)) == NULL)  /* match failed? */
    cap->level--;  /* undo capture */
  return res;
}


static char *end_capt (char *s, char *p, struct Capture *cap) {
  int l = capture_to_close(cap);
  char *res;
  cap->capture[l].len = s - cap->capture[l].init;  /* close capture */
  if ((res = match(s, p+1, cap)) == NULL)  /* match failed? */
    cap->capture[l].len = -1;  /* undo capture */
  return res;
}


static char *match_capture (char *s, int level, struct Capture *cap) {
  int l = check_cap(level, cap);
  int len = cap->capture[l].len;
  if (cap->src_end-s >= len &&
      memcmp(cap->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}


static char *match (char *s, char *p, struct Capture *cap) {
  init: /* using goto's to optimize tail recursion */
  switch (*p) {
    case '(':  /* start capture */
      return start_capt(s, p, cap);
    case ')':  /* end capture */
      return end_capt(s, p, cap);
    case ESC:  /* may be %[0-9] or %b */
      if (isdigit((unsigned char)(*(p+1)))) {  /* capture? */
        s = match_capture(s, *(p+1), cap);
        if (s == NULL) return NULL;
        p+=2; goto init;  /* else return match(p+2, s, cap) */
      }
      else if (*(p+1) == 'b') {  /* balanced string? */
        s = matchbalance(s, p+2, cap);
        if (s == NULL) return NULL;
        p+=4; goto init;  /* else return match(p+4, s, cap); */
      }
      else goto dflt;  /* case default */
    case '\0':  /* end of pattern */
      return s;  /* match succeeded */
    case '$':
      if (*(p+1) == '\0')  /* is the '$' the last char in pattern? */
        return (s == cap->src_end) ? s : NULL;  /* check end of string */
      else goto dflt;
    default: dflt: {  /* it is a pattern item */
      char *ep = luaI_classend(p);  /* points to what is next */
      int m = s<cap->src_end && luaI_singlematch((unsigned char)*s, p, ep);
      switch (*ep) {
        case '?': {  /* optional */
          char *res;
          if (m && ((res=match(s+1, ep+1, cap)) != NULL))
            return res;
          p=ep+1; goto init;  /* else return match(s, ep+1, cap); */
        }
        case '*':  /* 0 or more repetitions */
          return max_expand(s, p, ep, cap);
        case '+':  /* 1 or more repetitions */
          return (m ? max_expand(s+1, p, ep, cap) : NULL);
        case '-':  /* 0 or more repetitions (minimum) */
          return min_expand(s, p, ep, cap);
        default:
          if (!m) return NULL;
          s++; p=ep; goto init;  /* else return match(s+1, ep, cap); */
      }
    }
  }
}


static void str_find (void) {
  long l;
  char *s = luaL_check_lstr(1, &l);
  char *p = luaL_check_string(2);
  long init = posrelat(luaL_opt_long(3, 1), l) - 1;
  struct Capture cap;
  luaL_arg_check(0 <= init && init <= l, 3, "out of range");
  if (lua_getparam(4) != LUA_NOOBJECT ||
      strpbrk(p, SPECIALS) == NULL) {  /* no special characters? */
    char *s2 = strstr(s+init, p);
    if (s2) {
      lua_pushnumber(s2-s+1);
      lua_pushnumber(s2-s+strlen(p));
      return;
    }
  }
  else {
    int anchor = (*p == '^') ? (p++, 1) : 0;
    char *s1=s+init;
    cap.src_end = s+l;
    do {
      char *res;
      cap.level = 0;
      if ((res=match(s1, p, &cap)) != NULL) {
        lua_pushnumber(s1-s+1);  /* start */
        lua_pushnumber(res-s);   /* end */
        push_captures(&cap);
        return;
      }
    } while (s1++<cap.src_end && !anchor);
  }
  lua_pushnil();  /* if arrives here, it didn't find */
}


static void add_s (lua_Object newp, struct Capture *cap) {
  if (lua_isstring(newp)) {
    char *news = lua_getstring(newp);
    int l = lua_strlen(newp);
    int i;
    for (i=0; i<l; i++) {
      if (news[i] != ESC)
        luaL_addchar(news[i]);
      else {
        i++;  /* skip ESC */
        if (!isdigit((unsigned char)news[i]))
          luaL_addchar(news[i]);
        else {
          int level = check_cap(news[i], cap);
          addnchar(cap->capture[level].init, cap->capture[level].len);
        }
      }
    }
  }
  else {  /* is a function */
    lua_Object res;
    int status;
    int oldbuff;
    lua_beginblock();
    push_captures(cap);
    /* function may use buffer, so save it and create a new one */
    oldbuff = luaL_newbuffer(0);
    status = lua_callfunction(newp);
    /* restore old buffer */
    luaL_oldbuffer(oldbuff);
    if (status != 0) {
      lua_endblock();
      lua_error(NULL);
    }
    res = lua_getresult(1);
    if (lua_isstring(res))
      addnchar(lua_getstring(res), lua_strlen(res));
    lua_endblock();
  }
}


static void str_gsub (void) {
  long srcl;
  char *src = luaL_check_lstr(1, &srcl);
  char *p = luaL_check_string(2);
  lua_Object newp = lua_getparam(3);
  int max_s = luaL_opt_int(4, srcl+1);
  int anchor = (*p == '^') ? (p++, 1) : 0;
  int n = 0;
  struct Capture cap;
  luaL_arg_check(lua_isstring(newp) || lua_isfunction(newp), 3,
                 "string or function expected");
  luaL_resetbuffer();
  cap.src_end = src+srcl;
  while (n < max_s) {
    char *e;
    cap.level = 0;
    e = match(src, p, &cap);
    if (e) {
      n++;
      add_s(newp, &cap);
    }
    if (e && e>src) /* non empty match? */
      src = e;  /* skip it */
    else if (src < cap.src_end)
      luaL_addchar(*src++);
    else break;
    if (anchor) break;
  }
  addnchar(src, cap.src_end-src);
  closeandpush();
  lua_pushnumber(n);  /* number of substitutions */
}

/* }====================================================== */


static void luaI_addquoted (int arg) {
  long l;
  char *s = luaL_check_lstr(arg, &l);
  luaL_addchar('"');
  while (l--) {
    switch (*s) {
      case '"':  case '\\':  case '\n':
        luaL_addchar('\\');
        luaL_addchar(*s);
        break;
      case '\0': addnchar("\\000", 4); break;
      default: luaL_addchar(*s);
    }
    s++;
  }
  luaL_addchar('"');
}

/* maximum size of each format specification (such as '%-099.99d') */
#define MAX_FORMAT 20  /* arbitrary limit */

static void str_format (void) {
  int arg = 1;
  char *strfrmt = luaL_check_string(arg);
  luaL_resetbuffer();
  while (*strfrmt) {
    if (*strfrmt != '%')
      luaL_addchar(*strfrmt++);
    else if (*++strfrmt == '%')
      luaL_addchar(*strfrmt++);  /* %% */
    else { /* format item */
      struct Capture cap;
      char form[MAX_FORMAT];  /* to store the format ('%...') */
      char *buff;  /* to store the formatted item */
      char *initf = strfrmt;
      form[0] = '%';
      if (isdigit((unsigned char)*initf) && *(initf+1) == '$') {
        arg = *initf - '0';
        initf += 2;  /* skip the 'n$' */
      }
      arg++;
      cap.src_end = strfrmt+strlen(strfrmt)+1;
      cap.level = 0;
      strfrmt = match(initf, "[-+ #0]*(%d*)%.?(%d*)", &cap);
      if (cap.capture[0].len > 2 || cap.capture[1].len > 2 ||  /* < 100? */
          strfrmt-initf > MAX_FORMAT-2)
        lua_error("invalid format (width or precision too long)");
      strncpy(form+1, initf, strfrmt-initf+1); /* +1 to include conversion */
      form[strfrmt-initf+2] = 0;
      buff = luaL_openspace(512);  /* 512 > size of format('%99.99f', -1e308) */
      switch (*strfrmt++) {
        case 'c':  case 'd':  case 'i':
          sprintf(buff, form, luaL_check_int(arg));
          break;
        case 'o':  case 'u':  case 'x':  case 'X':
          sprintf(buff, form, (unsigned int)luaL_check_number(arg));
          break;
        case 'e':  case 'E': case 'f': case 'g': case 'G':
          sprintf(buff, form, luaL_check_number(arg));
          break;
        case 'q':
          luaI_addquoted(arg);
          continue;  /* skip the "addsize" at the end */
        case 's': {
          long l;
          char *s = luaL_check_lstr(arg, &l);
          if (cap.capture[1].len == 0 && l >= 100) {
            /* no precision and string is too big to be formatted;
               keep original string */
            addnchar(s, l);
            continue;  /* skip the "addsize" at the end */
          }
          else {
            sprintf(buff, form, s);
            break;
          }
        }
        default:  /* also treat cases 'pnLlh' */
          lua_error("invalid option in `format'");
      }
      luaL_addsize(strlen(buff));
    }
  }
  closeandpush();  /* push the result */
}


static struct luaL_reg strlib[] = {
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
void strlib_open (void)
{
  luaL_openlib(strlib, (sizeof(strlib)/sizeof(strlib[0])));
}
