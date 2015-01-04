/*
** $Id: lstrlib.c,v 1.18 1998/07/01 14:21:57 roberto Exp $
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


static void closeandpush (void)
{
  lua_pushlstring(luaL_buffer(), luaL_getsize());
}


static long posrelat (long pos, long len)
{
  /* relative string position: negative means back from end */
  return (pos>=0) ? pos : len+pos+1;
}


static void str_sub (void)
{
  long l;
  char *s = luaL_check_lstr(1, &l);
  long start = posrelat(luaL_check_number(2), l);
  long end = posrelat(luaL_opt_number(3, -1), l);
  if (1 <= start && start <= end && end <= l)
    lua_pushlstring(s+start-1, end-start+1);
  else lua_pushstring("");
}


static void str_lower (void)
{
  long l;
  int i;
  char *s = luaL_check_lstr(1, &l);
  luaL_resetbuffer();
  for (i=0; i<l; i++)
    luaL_addchar(tolower((unsigned char)(s[i])));
  closeandpush();
}


static void str_upper (void)
{
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
  int n = (int)luaL_check_number(2);
  luaL_resetbuffer();
  while (n-- > 0)
    addnchar(s, l);
  closeandpush();
}


static void str_byte (void)
{
  long l;
  char *s = luaL_check_lstr(1, &l);
  long pos = posrelat(luaL_opt_number(2, 1), l);
  luaL_arg_check(0<pos && pos<=l, 2,  "out of range");
  lua_pushnumber((unsigned char)s[pos-1]);
}

static void str_char (void) {
  int i = 0;
  luaL_resetbuffer();
  while (lua_getparam(++i) != LUA_NOOBJECT) {
    double c = luaL_check_number(i);
    luaL_arg_check((unsigned char)c == c, i, "invalid value");
    luaL_addchar((int)c);
  }
  closeandpush();
}


/*
** =======================================================
** PATTERN MATCHING
** =======================================================
*/

#define MAX_CAPT 9

struct Capture {
  int level;  /* total number of captures (finished or unfinished) */
  char *src_end;  /* end ('\0') of source string */
  struct {
    char *init;
    int len;  /* -1 signals unfinished capture */
  } capture[MAX_CAPT];
};


#define ESC	'%'
#define SPECIALS  "^$*?.([%-"


static void push_captures (struct Capture *cap)
{
  int i;
  for (i=0; i<cap->level; i++)
    lua_pushlstring(cap->capture[i].init, cap->capture[i].len);
}


static int check_cap (int l, struct Capture *cap)
{
  l -= '1';
  if (!(0 <= l && l < cap->level && cap->capture[l].len != -1))
    lua_error("invalid capture index");
  return l;
}


static int capture_to_close (struct Capture *cap)
{
  int level = cap->level;
  for (level--; level>=0; level--)
    if (cap->capture[level].len == -1) return level;
  lua_error("invalid pattern capture");
  return 0;  /* to avoid warnings */
}


static char *bracket_end (char *p)
{
  return (*p == 0) ? NULL : strchr((*p=='^') ? p+2 : p+1, ']');
}


static int matchclass (int c, int cl)
{
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
    case 'z' : res = (c == '\0'); break;
    default: return (cl == c);
  }
  return (islower((unsigned char)cl) ? res : !res);
}


int luaI_singlematch (int c, char *p, char **ep)
{
  switch (*p) {
    case '.':  /* matches any char */
      *ep = p+1;
      return 1;
    case '\0':  /* end of pattern; matches nothing */
      *ep = p;
      return 0;
    case ESC:
      if (*(++p) == '\0')
        luaL_verror("incorrect pattern (ends with `%c')", ESC);
      *ep = p+1;
      return matchclass(c, (unsigned char)*p);
    case '[': {
      char *end = bracket_end(p+1);
      int sig = *(p+1) == '^' ? (p++, 0) : 1;
      if (end == NULL) lua_error("incorrect pattern (missing `]')");
      *ep = end+1;
      while (++p < end) {
        if (*p == ESC) {
          if (((p+1) < end) && matchclass(c, (unsigned char)*++p))
            return sig;
        }
        else if ((*(p+1) == '-') && (p+2 < end)) {
          p+=2;
          if ((unsigned char)*(p-2) <= c && c <= (unsigned char)*p)
            return sig;
        }
        else if ((unsigned char)*p == c) return sig;
      }
      return !sig;
    }
    default:
      *ep = p+1;
      return ((unsigned char)*p == c);
  }
}


static char *matchbalance (char *s, int b, int e, struct Capture *cap)
{
  if (*s != b) return NULL;
  else {
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


static char *matchitem (char *s, char *p, struct Capture *cap, char **ep)
{
  if (*p == ESC) {
    p++;
    if (isdigit((unsigned char)*p)) {  /* capture */
      int l = check_cap(*p, cap);
      int len = cap->capture[l].len;
      *ep = p+1;
      if (cap->src_end-s >= len && memcmp(cap->capture[l].init, s, len) == 0)
        return s+len;
      else return NULL;
    }
    else if (*p == 'b') {  /* balanced string */
      p++;
      if (*p == 0 || *(p+1) == 0)
        lua_error("unbalanced pattern");
      *ep = p+2;
      return matchbalance(s, *p, *(p+1), cap);
    }
    else p--;  /* and go through */
  }
  /* "luaI_singlematch" sets "ep" (so must be called even when *s == 0) */
  return (luaI_singlematch((unsigned char)*s, p, ep) && s<cap->src_end) ?
                    s+1 : NULL;
}


static char *match (char *s, char *p, struct Capture *cap)
{
  init: /* using goto's to optimize tail recursion */
  switch (*p) {
    case '(': {  /* start capture */
      char *res;
      if (cap->level >= MAX_CAPT) lua_error("too many captures");
      cap->capture[cap->level].init = s;
      cap->capture[cap->level].len = -1;
      cap->level++;
      if ((res=match(s, p+1, cap)) == NULL)  /* match failed? */
        cap->level--;  /* undo capture */
      return res;
    }
    case ')': {  /* end capture */
      int l = capture_to_close(cap);
      char *res;
      cap->capture[l].len = s - cap->capture[l].init;  /* close capture */
      if ((res = match(s, p+1, cap)) == NULL)  /* match failed? */
        cap->capture[l].len = -1;  /* undo capture */
      return res;
    }
    case '\0': case '$':  /* (possibly) end of pattern */
      if (*p == 0 || (*(p+1) == 0 && s == cap->src_end))
        return s;
      /* else go through */
    default: {  /* it is a pattern item */
      char *ep;  /* get what is next */
      char *s1 = matchitem(s, p, cap, &ep);
      switch (*ep) {
        case '*': {  /* repetition */
          char *res;
          if (s1 && s1>s && ((res=match(s1, p, cap)) != NULL))
            return res;
          p=ep+1; goto init;  /* else return match(s, ep+1, cap); */
        }
        case '?': {  /* optional */
          char *res;
          if (s1 && ((res=match(s1, ep+1, cap)) != NULL))
            return res;
          p=ep+1; goto init;  /* else return match(s, ep+1, cap); */
        }
        case '-': {  /* repetition */
          char *res;
          if ((res = match(s, ep+1, cap)) != NULL)
            return res;
          else if (s1 && s1>s) {
            s = s1;
            goto init;  /* return match(s1, p, cap); */
          }
          else
            return NULL;
        }
        default:
          if (s1) { s=s1; p=ep; goto init; }  /* return match(s1, ep, cap); */
          else return NULL;
      }
    }
  }
}


static void str_find (void)
{
  long l;
  char *s = luaL_check_lstr(1, &l);
  char *p = luaL_check_string(2);
  long init = posrelat(luaL_opt_number(3, 1), l) - 1;
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


static void add_s (lua_Object newp, struct Capture *cap)
{
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


static void str_gsub (void)
{
  long srcl;
  char *src = luaL_check_lstr(1, &srcl);
  char *p = luaL_check_string(2);
  lua_Object newp = lua_getparam(3);
  int max_s = (int)luaL_opt_number(4, srcl+1);
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


static void luaI_addquoted (char *s)
{
  luaL_addchar('"');
  for (; *s; s++) {
    if (strchr("\"\\\n", *s))
      luaL_addchar('\\');
    luaL_addchar(*s);
  }
  luaL_addchar('"');
}

#define MAX_FORMAT 200

static void str_format (void)
{
  int arg = 1;
  char *strfrmt = luaL_check_string(arg);
  struct Capture cap;
  cap.src_end = strfrmt+strlen(strfrmt)+1;
  luaL_resetbuffer();
  while (*strfrmt) {
    if (*strfrmt != '%')
      luaL_addchar(*strfrmt++);
    else if (*++strfrmt == '%')
      luaL_addchar(*strfrmt++);  /* %% */
    else { /* format item */
      char form[MAX_FORMAT];      /* store the format ('%...') */
      char *buff;
      char *initf = strfrmt;
      form[0] = '%';
      cap.level = 0;
      if (isdigit((unsigned char)initf[0]) && initf[1] == '$') {
        arg = initf[0] - '0';
        initf += 2;  /* skip the 'n$' */
      }
      arg++;
      strfrmt = match(initf, "[-+ #0]*(%d*)%.?(%d*)", &cap);
      if (cap.capture[0].len > 2 || cap.capture[1].len > 2)  /* < 100? */
        lua_error("invalid format (width or precision too long)");
      strncpy(form+1, initf, strfrmt-initf+1); /* +1 to include conversion */
      form[strfrmt-initf+2] = 0;
      buff = luaL_openspace(1000);  /* to store the formatted value */
      switch (*strfrmt++) {
        case 'q':
          luaI_addquoted(luaL_check_string(arg));
          continue;
        case 's': {
          char *s = luaL_check_string(arg);
          buff = luaL_openspace(strlen(s));
          sprintf(buff, form, s);
          break;
        }
        case 'c':  case 'd':  case 'i':
          sprintf(buff, form, (int)luaL_check_number(arg));
          break;
        case 'o':  case 'u':  case 'x':  case 'X':
          sprintf(buff, form, (unsigned int)luaL_check_number(arg));
          break;
        case 'e':  case 'E': case 'f': case 'g': case 'G':
          sprintf(buff, form, luaL_check_number(arg));
          break;
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
{"ascii", str_byte},  /* for compatibility */
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
