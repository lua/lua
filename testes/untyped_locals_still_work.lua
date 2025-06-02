print("untyped_locals_still_work.lua: Testing untyped locals with 'fn' syntax")
local a = 10
local b, c = "hello", true
local d
print(a,b,c,d) -- 10 hello true nil

local fn foo(x,y): -- Changed here
    local z = x + y
    return z
end
print(foo(1,2)) -- 3
print("untyped_locals_still_work.lua: Untyped declarations processed.")
