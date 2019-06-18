-- $Id: testes/cstack.lua $
-- See Copyright Notice in file all.lua

local debug = require "debug"

print"testing C-stack overflow detection"

local origlimit = debug.setCstacklimit(400)
print("current stack limit: " .. origlimit)
debug.setCstacklimit(origlimit)

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


do  print("testing changes in C-stack limit")

  assert(not debug.setCstacklimit(0))        -- limit too small
  assert(not debug.setCstacklimit(50000))    -- limit too large
  local co = coroutine.wrap (function ()
               return debug.setCstacklimit(400)
             end)
  assert(co() == false)         -- cannot change C stack inside coroutine

  local n
  local function foo () n = n + 1; foo () end

  local function check ()
    n = 0
    pcall(foo)
    return n
  end

  assert(debug.setCstacklimit(400) == origlimit)
  local lim400 = check()
  -- a very low limit (given that the several calls to arive here)
  local lowlimit = 38
  assert(debug.setCstacklimit(lowlimit) == 400)
  assert(check() < lowlimit - 30)
  assert(debug.setCstacklimit(600) == lowlimit)
  local lim600 = check()
  assert(lim600 == lim400 + 200)


  -- 'setCstacklimit' works inside protected calls. (The new stack
  -- limit is kept when 'pcall' returns.)
  assert(pcall(function ()
    assert(debug.setCstacklimit(400) == 600)
    assert(check() <= lim400)
  end))

  assert(check() == lim400)
  assert(debug.setCstacklimit(origlimit) == 400)   -- restore original limit
end


print'OK'
