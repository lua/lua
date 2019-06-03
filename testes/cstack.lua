-- $Id: testes/cstack.lua $
-- See Copyright Notice in file all.lua

print"testing C-stack overflow detection"

-- Segmentation faults in these tests probably result from a C-stack
-- overflow. To avoid these errors, recompile Lua with a smaller
-- value for the constant 'LUAI_MAXCCALLS' or else ensure a larger
-- stack for the program.

local function checkerror (msg, f, ...)
  local s, err = pcall(f, ...)
  assert(not s and string.find(err, msg))
end

local count
local back = string.rep("\b", 8)
local function progress ()
  count = count + 1
  local n = string.format("%-8d", count)
  io.stderr:write(back, n)
end


do    print("testing simple recursion:")
  count = 0
  local function foo ()
    progress()
    foo()
  end
  checkerror("stack overflow", foo)
  print("\tfinal count: ", count)
end


do  print("testing stack overflow in message handling")
  count = 0
  local function loop (x, y, z)
    progress()
    return 1 + loop(x, y, z)
  end
  local res, msg = xpcall(loop, loop)
  assert(msg == "error in error handling")
  print("\tfinal count: ", count)
end


-- bug since 2.5 (C-stack overflow in recursion inside pattern matching)
do  print("testing recursion inside pattern matching")
  local function f (size)
    local s = string.rep("a", size)
    local p = string.rep(".?", size)
    return string.match(s, p)
  end
  local m = f(80)
  assert(#m == 80)
  checkerror("too complex", f, 200000)
end


do  print("testing stack-overflow in recursive 'gsub'")
  count = 0
  local function foo ()
    progress()
    string.gsub("a", ".", foo)
  end
  checkerror("stack overflow", foo)
  print("\tfinal count: ", count)

  print("testing stack-overflow in recursive 'gsub' with metatables")
  count = 0
  local t = setmetatable({}, {__index = foo})
  foo = function ()
    count = count + 1
    progress(count)
    string.gsub("a", ".", t)
  end
  checkerror("stack overflow", foo)
  print("\tfinal count: ", count)
end

print'OK'
