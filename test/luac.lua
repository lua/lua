-- bare-bones luac in Lua
-- usage: lua luac.lua file.lua

assert(arg[1]~=nil,"usage: lua luac.lua file.lua")
f=assert(io.open("luac.out","w"))
f:write(string.dump(loadfile(arg[1])))
io.close(f)
