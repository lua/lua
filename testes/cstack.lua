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


do    -- simple recursion
  local count = 0
  local function foo ()
    count = count + 1
    foo()
  end
  checkerror("stack overflow", foo)
  print("  maximum recursion: " .. count)
end


-- bug since 2.5 (C-stack overflow in recursion inside pattern matching)
do
  local function f (size)
    local s = string.rep("a", size)
    local p = string.rep(".?", size)
    return string.match(s, p)
  end
  local m = f(80)
  assert(#m == 80)
  checkerror("too complex", f, 200000)
end


-- testing stack-overflow in recursive 'gsub'
do
  local count = 0
  local function foo ()
    count = count + 1
    string.gsub("a", ".", foo)
  end
  checkerror("stack overflow", foo)
  print("  maximum 'gsub' nest (calls): " .. count)

  -- can be done with metamethods, too
  count = 0
  local t = setmetatable({}, {__index = foo})
  foo = function ()
    count = count + 1
    string.gsub("a", ".", t)
  end
  checkerror("stack overflow", foo)
  print("  maximum 'gsub' nest (metamethods): " .. count)
end

print'OK'
