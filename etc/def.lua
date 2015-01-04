-- def.lua
-- make .DEF file from lua.h
-- usage: lua def.lua <lua.h >lua.def

T=read"*a"
write("LIBRARY LUA\nVERSION ")
gsub(T,"LUA_VERSION.-(%d+%.%d+)",write)
write("\nEXPORTS\n")
gsub(T,"(lua_%w+)%s+%(",function (f) write("   ",f,"\n") end)
