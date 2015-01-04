/*
* min.c -- a minimal Lua interpreter
* loads stdin only with minimal error handling.
* no interaction, and no standard library, only a "print" function.
*/

#include <stdio.h>
#include "lua.h"

static int print(lua_State *L)
{
 int n=lua_gettop(L);
 int i;
 for (i=1; i<=n; i++)
 {
  if (i>1) printf("\t");
  if (lua_isstring(L,i))
   printf("%s",lua_tostring(L,i));
  else if (lua_isnil(L,i))
   printf("%s","nil");
  else if (lua_isboolean(L,i))
   printf("%s",lua_toboolean(L,i) ? "true" : "false");
  else
   printf("%s:%p",lua_typename(L,lua_type(L,i)),lua_topointer(L,i));
 }
 printf("\n");
 return 0;
}

static const char *getF(lua_State *L, void *ud, size_t *size)
{
 FILE *f=(FILE *)ud;
 static char buff[512];
 if (feof(f)) return NULL;
 *size=fread(buff,1,sizeof(buff),f);
 return (*size>0) ? buff : NULL;
}

int main(void)
{
 lua_State *L=lua_open();
 lua_register(L,"print",print);
 if (lua_load(L,getF,stdin,"=stdin") || lua_pcall(L,0,0,0))
  fprintf(stderr,"%s\n",lua_tostring(L,-1));
 return 0;
}
