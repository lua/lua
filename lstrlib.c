/*
** $Id: lstrlib.c,v 1.6 1998/01/09 14:44:55 roberto Exp $
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
  strncpy(b, s, n);
  luaL_addsize(n);
}


static void addstr (char *s)
{
  addnchar(s, strlen(s));
}


static void str_len (void)
{
 lua_pushnumber(strlen(luaL_check_string(1)));
}


static void closeandpush (void)
{
  luaL_addchar(0);
  lua_pushstring(luaL_buffer());
}


static void str_sub (void)
{
  char *s = luaL_check_string(1);
  long l = strlen(s);
  long start = (long)luaL_check_number(2);
  long end = (long)luaL_opt_number(3, -1);
  if (start < 0) start = l+start+1;
  if (end < 0) end = l+end+1;
  if (1 <= start && start <= end && end <= l) {
    luaL_resetbuffer();
    addnchar(s+start-1, end-start+1);
    closeandpush();
  }
  else lua_pushstring("");
}


static void str_lower (void)
{
  char *s;
  luaL_resetbuffer();
  for (s = luaL_check_string(1); *s; s++)
    luaL_addchar(tolower((unsigned char)*s));
  closeandpush();
}


static void str_upper (void)
{
  char *s;
  luaL_resetbuffer();
  for (s = luaL_check_string(1); *s; s++)
    luaL_addchar(toupper((unsigned char)*s));
  closeandpush();
}

static void str_rep (void)
{
  char *s = luaL_check_string(1);
  int n = (int)luaL_check_number(2);
  luaL_resetbuffer();
  while (n-- > 0)
    addstr(s);
  closeandpush();
}


static void str_ascii (void)
{
  char *s = luaL_check_string(1);
  long pos = (long)luaL_opt_number(2, 1) - 1;
  luaL_arg_check(0<=pos && pos<strlen(s), 2,  "out of range");
  lua_pushnumber((unsigned char)s[pos]);
}



/*
** =======================================================
** PATTERN MATCHING
** =======================================================
*/

#define MAX_CAPT 9

struct Capture {
  int level;  /* total number of captures (finished or unfinished) */
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
  for (i=0; i<cap->level; i++) {
    int l = cap->capture[i].len;
    char *buff = luaL_openspace(l+1);
    if (l == -1) lua_error("unfinished capture");
    strncpy(buff, cap->capture[i].init, l);
    buff[l] = 0;
    lua_pushstring(buff);
  }
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
  if (c == 0) return 0;
  switch (tolower((unsigned char)cl)) {
    case 'w' : res = isalnum((unsigned char)c); break;
    case 'd' : res = isdigit((unsigned char)c); break;
    case 's' : res = isspace((unsigned char)c); break;
    case 'a' : res = isalpha((unsigned char)c); break;
    case 'p' : res = ispunct((unsigned char)c); break;
    case 'l' : res = islower((unsigned char)c); break;
    case 'u' : res = isupper((unsigned char)c); break;
    case 'c' : res = iscntrl((unsigned char)c); break;
    default: return (cl == c);
  }
  return (islower((unsigned char)cl) ? res : !res);
}


int luaI_singlematch (int c, char *p, char **ep)
{
  switch (*p) {
    case '.':
      *ep = p+1;
      return (c != 0);
    case '\0':
      *ep = p;
      return 0;
    case ESC:
      if (*(++p) == '\0')
        luaL_verror("incorrect pattern (ends with `%c')", ESC);
      *ep = p+1;
      return matchclass(c, *p);
    case '[': {
      char *end = bracket_end(p+1);
      int sig = *(p+1) == '^' ? (p++, 0) : 1;
      if (end == NULL) lua_error("incorrect pattern (missing `]')");
      *ep = end+1;
      if (c == 0) return 0;
      while (++p < end) {
        if (*p == ESC) {
          if (((p+1) < end) && matchclass(c, *++p)) return sig;
        }
        else if ((*(p+1) == '-') && (p+2 < end)) {
          p+=2;
          if (*(p-2) <= c && c <= *p) return sig;
        }
        else if (*p == c) return sig;
      }
      return !sig;
    }
    default:
      *ep = p+1;
      return (*p == c);
  }
}


static char *matchbalance (char *s, int b, int e)
{
  if (*s != b) return NULL;
  else {
    int cont = 1;
    while (*(++s)) {
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
      *ep = p+1;
      if (strncmp(cap->capture[l].init, s, cap->capture[l].len) == 0)
        return s+cap->capture[l].len;
      else return NULL;
    }
    else if (*p == 'b') {  /* balanced string */
      p++;
      if (*p == 0 || *(p+1) == 0)
        lua_error("unbalanced pattern");
      *ep = p+2;
      return matchbalance(s, *p, *(p+1));
    }
    else p--;  /* and go through */
  }
  return (luaI_singlematch(*s, p, ep) ? s+1 : NULL);
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
      if (*p == 0 || (*(p+1) == 0 && *s == 0))
        return s;
      /* else go through */
    default: {  /* it is a pattern item */
      char *ep;  /* get what is next */
      char *s1 = matchitem(s, p, cap, &ep);
      switch (*ep) {
        case '*': {  /* repetition */
          char *res;
          if (s1 && (res = match(s1, p, cap)))
            return res;
          p=ep+1; goto init;  /* else return match(s, ep+1, cap); */
        }
        case '?': {  /* optional */
          char *res;
          if (s1 && (res = match(s1, ep+1, cap)))
            return res;
          p=ep+1; goto init;  /* else return match(s, ep+1, cap); */
        }
        case '-': {  /* repetition */
          char *res;
          if ((res = match(s, ep+1, cap)) != 0)
            return res;
          else if (s1) {
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
  char *s = luaL_check_string(1);
  char *p = luaL_check_string(2);
  long init = (long)luaL_opt_number(3, 1) - 1;
  luaL_arg_check(0 <= init && init <= strlen(s), 3, "out of range");
  if (lua_getparam(4) != LUA_NOOBJECT ||
      strpbrk(p, SPECIALS) == NULL) {  /* no special caracters? */
    char *s2 = strstr(s+init, p);
    if (s2) {
      lua_pushnumber(s2-s+1);
      lua_pushnumber(s2-s+strlen(p));
    }
  }
  else {
    int anchor = (*p == '^') ? (p++, 1) : 0;
    char *s1=s+init;
    do {
      struct Capture cap;
      char *res;
      cap.level = 0;
      if ((res=match(s1, p, &cap)) != NULL) {
        lua_pushnumber(s1-s+1);  /* start */
        lua_pushnumber(res-s);   /* end */
        push_captures(&cap);
        return;
      }
    } while (*s1++ && !anchor);
  }
}


static void add_s (lua_Object newp, struct Capture *cap)
{
  if (lua_isstring(newp)) {
    char *news = lua_getstring(newp);
    while (*news) {
      if (*news != ESC || !isdigit((unsigned char)*++news))
        luaL_addchar(*news++);
      else {
        int l = check_cap(*news++, cap);
        addnchar(cap->capture[l].init, cap->capture[l].len);
      }
    }
  }
  else if (lua_isfunction(newp)) {
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
    addstr(lua_isstring(res) ? lua_getstring(res) : "");
    lua_endblock();
  }
  else luaL_arg_check(0, 3, "string or function expected");
}


static void str_gsub (void)
{
  char *src = luaL_check_string(1);
  char *p = luaL_check_string(2);
  lua_Object newp = lua_getparam(3);
  int max_s = (int)luaL_opt_number(4, strlen(src)+1);
  int anchor = (*p == '^') ? (p++, 1) : 0;
  int n = 0;
  luaL_resetbuffer();
  while (n < max_s) {
    struct Capture cap;
    char *e;
    cap.level = 0;
    e = match(src, p, &cap);
    if (e) {
      n++;
      add_s(newp, &cap);
    }
    if (e && e>src) /* non empty match? */
      src = e;  /* skip it */
    else if (*src)
      luaL_addchar(*src++);
    else break;
    if (anchor) break;
  }
  addstr(src);
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
  luaL_resetbuffer();
  while (*strfrmt) {
    if (*strfrmt != '%')
      luaL_addchar(*strfrmt++);
    else if (*++strfrmt == '%')
      luaL_addchar(*strfrmt++);  /* %% */
    else { /* format item */
      char form[MAX_FORMAT];      /* store the format ('%...') */
      struct Capture cap;
      char *buff;
      char *initf = strfrmt;
      form[0] = '%';
      cap.level = 0;
      strfrmt = match(strfrmt, "%d?%$?[-+ #]*(%d*)%.?(%d*)", &cap);
      if (cap.capture[0].len > 3 || cap.capture[1].len > 3)  /* < 1000? */
        lua_error("invalid format (width or precision too long)");
      if (isdigit((unsigned char)initf[0]) && initf[1] == '$') {
        arg = initf[0] - '0';
        initf += 2;  /* skip the 'n$' */
      }
      arg++;
      strncpy(form+1, initf, strfrmt-initf+1); /* +1 to include convertion */
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
        case 'c':  case 'd':  case 'i': case 'o':
        case 'u':  case 'x':  case 'X':
          sprintf(buff, form, (int)luaL_check_number(arg));
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
{"strrep", str_rep},
{"ascii", str_ascii},
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
