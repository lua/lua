-- emulate the command line behaviour of Lua 4.0
-- usage: lua doall.lua f1.lua f2.lua f3.lua ...

for i=1,table.getn(arg) do
 dofile(arg[i])
end
