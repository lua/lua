-- stdcall.lua
-- add __stdcall where appropriate
-- usage: lua stdcall.lua <lua.h >s_lua.h
-- usage: lua stdcall.lua <lapi.c >s_lapi.c

T=read"*a"
T=gsub(T,"(lua_%w+%s+%()","__stdcall %1")
T=gsub(T,"(%*lua_CFunction)","__stdcall %1")

write(T)
