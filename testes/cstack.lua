-- $Id: testes/cstack.lua $
-- See Copyright Notice in file all.lua

local debug = require "debug"

print"testing C-stack overflow detection"
print"If this test crashes, see its file ('cstack.lua')"

-- Segmentation faults in these tests probably result from a C-stack
-- overflow. To avoid these errors, you can use the function
-- 'debug.setcstacklimit' to set a smaller limit for the use of
-- C stack by Lua. After finding a reliable limit, you might want
-- to recompile Lua with this limit as the value for
-- the constant 'LUAI_MAXCCALLS', which defines the default limit.
-- (The default limit is printed by this test.)
-- Alternatively, you can ensure a larger stack for the program.

-- For Linux, a limit up to 30_000 seems Ok. Windows cannot go much
-- higher than 2_000.


-- get and print original limit
local origlimit <const> = debug.setcstacklimit(400)
print("default stack limit: " .. origlimit)


-- Do the tests using the original limit. Or else you may want to change
-- 'currentlimit' to lower values to avoid a seg. fault or to higher
-- values to check whether they are reliable.
local currentlimit <const> =  origlimit
debug.setcstacklimit(currentlimit)
print("current stack limit: " .. currentlimit)


local function checkerror (msg, f, ...)
  local s, err = pcall(f, ...)
  assert(not s and string.find(err, msg))
end

-- auxiliary function to keep 'count' on the screen even if the program
-- crashes.
local count
local back = string.rep("\b", 8)
local function progress ()
  count = count + 1
  local n = string.format("%-8d", count)
  io.stderr:write(back, n)   -- erase previous value and write new one
end


do    print("testing simple recursion:")
  count = 0
  local function foo ()
    progress()
    foo()   -- do recursive calls until a stack error (or crash)
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

do   -- bug in 5.4.0
  print("testing limits in coroutines inside deep calls")
  count = 0
  local lim = 1000
  local function stack (n)
    progress()
    if n > 0 then return stack(n - 1) + 1
    else coroutine.wrap(function ()
           stack(lim)
         end)()
    end
  end

  print(xpcall(stack, function () return "ok" end, lim))
end


do  print("testing changes in C-stack limit")

  -- Just an alternative limit, different from the current one
  -- (smaller to avoid stack overflows)
  local alterlimit <const> = currentlimit * 8 // 10

  assert(not debug.setcstacklimit(0))        -- limit too small
  assert(not debug.setcstacklimit(50000))    -- limit too large
  local co = coroutine.wrap (function ()
               return debug.setcstacklimit(alterlimit)
             end)
  assert(not co())         -- cannot change C stack inside coroutine

  local n
  local function foo () n = n + 1; foo () end

  local function check ()
    n = 0
    pcall(foo)
    return n
  end

  -- set limit to 'alterlimit'
  assert(debug.setcstacklimit(alterlimit) == currentlimit)
  local limalter <const> = check()
  -- set a very low limit (given that there are already several active
  -- calls to arrive here)
  local lowlimit <const> = 38
  assert(debug.setcstacklimit(lowlimit) == alterlimit)
  -- usable limit is much lower, due to active calls
  local actuallow = check()
  assert(actuallow < lowlimit - 30)
  -- now, add 'lowlimit' extra slots, which should all be available
  assert(debug.setcstacklimit(lowlimit + lowlimit) == lowlimit)
  local lim2 <const> = check()
  assert(lim2 == actuallow + lowlimit)


  -- 'setcstacklimit' works inside protected calls. (The new stack
  -- limit is kept when 'pcall' returns.)
  assert(pcall(function ()
    assert(debug.setcstacklimit(alterlimit) == lowlimit * 2)
    assert(check() <= limalter)
  end))

  assert(check() == limalter)
  -- restore original limit
  assert(debug.setcstacklimit(origlimit) == alterlimit)
end


print'OK'
