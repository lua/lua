/* saconfig.c -- configuration for stand-alone Lua interpreter
*
* #define LUA_USERCONFIG to this file
*
* Here are the features that can be customized using #define:
*
*** Line editing and history:
*   #define USE_READLINE to use the GNU readline library.
*   Add "-lreadline -ltermcap" to MYLIBS.
*
*   To use another library for this, use the code below as a start.
*   Make sure you #define lua_readline and lua_saveline accordingly.
*
*   If you do not #define lua_readline, you'll get a version based on fgets
*   that uses a static buffer of size MAXINPUT.
*/

#define USE_READLINE

#ifdef USE_READLINE
/*
* This section implements of lua_readline and lua_saveline  for lua.c using
* the GNU readline and history libraries.  It should also work with drop-in
* replacements  such as  editline  and  libedit (you  may  have to  include
* different headers, though).
*
*/

#define lua_readline	myreadline
#define lua_saveline	mysaveline

#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

static int myreadline (lua_State *L, const char *prompt) {
  char *s=readline(prompt);
  if (s==NULL)
    return 0;
  else {
    lua_pushstring(L,s);
    lua_pushliteral(L,"\n");
    lua_concat(L,2);
    free(s);
    return 1;
  }
}

static void mysaveline (lua_State *L, const char *s) {
  const char *p;
  for (p=s; isspace(*p); p++)
    ;
  if (*p!=0) {
    size_t n=strlen(s)-1;
    if (s[n]!='\n')
      add_history(s);
    else {
      lua_pushlstring(L,s,n);
      s=lua_tostring(L,-1);
      add_history(s);
      lua_remove(L,-1);
    }
  }
}
#endif
