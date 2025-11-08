-- $Id: testes/goto.lua $
-- See Copyright Notice in file lua.h

global<const> require
global<const> print, load, assert, string, setmetatable
global<const> collectgarbage, error

print("testing goto and global declarations")

collectgarbage()

local function errmsg (code, m)
  local st, msg = load(code)
  assert(not st and string.find(msg, m))
end

-- cannot see label inside block
errmsg([[ goto l1; do ::l1:: end ]], "label 'l1'")
errmsg([[ do ::l1:: end goto l1; ]], "label 'l1'")

-- repeated label
errmsg([[ ::l1:: ::l1:: ]], "label 'l1'")
errmsg([[ ::l1:: do ::l1:: end]], "label 'l1'")



-- jumping over variable declaration
errmsg([[ goto l1; local aa ::l1:: ::l2:: print(3) ]], "scope of 'aa'")

errmsg([[ goto l2; global *; ::l1:: ::l2:: print(3) ]], "scope of '*'")

errmsg([[
do local bb, cc; goto l1; end
local aa
::l1:: print(3)
]], "scope of 'aa'")


-- jumping into a block
errmsg([[ do ::l1:: end goto l1 ]], "label 'l1'")
errmsg([[ goto l1 do ::l1:: end ]], "label 'l1'")

-- cannot continue a repeat-until with variables
errmsg([[
  repeat
    if x then goto cont end
    local xuxu = 10
    ::cont::
  until xuxu < x
]], "scope of 'xuxu'")

-- simple gotos
local x
do
  local y = 12
  goto l1
  ::l2:: x = x + 1; goto l3
  ::l1:: x = y; goto l2
end
::l3:: ::l3_1:: assert(x == 13)


-- long labels
do
  local prog = [[
  do
    local a = 1
    goto l%sa; a = a + 1
   ::l%sa:: a = a + 10
    goto l%sb; a = a + 2
   ::l%sb:: a = a + 20
    return a
  end
  ]]
  local label = string.rep("0123456789", 40)
  prog = string.format(prog, label, label, label, label)
  assert(assert(load(prog))() == 31)
end


-- ok to jump over local dec. to end of block
do
  goto l1
  local a = 23
  x = a
  ::l1::;
end

while true do
  goto l4
  goto l1  -- ok to jump over local dec. to end of block
  goto l1  -- multiple uses of same label
  local x = 45
  ::l1:: ;;;
end
::l4:: assert(x == 13)

if print then
  goto l1   -- ok to jump over local dec. to end of block
  error("should not be here")
  goto l2   -- ok to jump over local dec. to end of block
  local x
  ::l1:: ; ::l2:: ;;
else end

-- to repeat a label in a different function is OK
local function foo ()
  local a = {}
  goto l3
  ::l1:: a[#a + 1] = 1; goto l2;
  ::l2:: a[#a + 1] = 2; goto l5;
  ::l3::
  ::l3a:: a[#a + 1] = 3; goto l1;
  ::l4:: a[#a + 1] = 4; goto l6;
  ::l5:: a[#a + 1] = 5; goto l4;
  ::l6:: assert(a[1] == 3 and a[2] == 1 and a[3] == 2 and
              a[4] == 5 and a[5] == 4)
  if not a[6] then a[6] = true; goto l3a end   -- do it twice
end

::l6:: foo()


do   -- bug in 5.2 -> 5.3.2
  local x
  ::L1::
  local y             -- cannot join this SETNIL with previous one
  assert(y == nil)
  y = true
  if x == nil then
    x = 1
    goto L1
  else
    x = x + 1
  end
  assert(x == 2 and y == true)
end

-- bug in 5.3
do
  local first = true
  local a = false
  if true then
    goto LBL
    ::loop::
    a = true
    ::LBL::
    if first then
      first = false
      goto loop
    end
  end
  assert(a)
end

do   -- compiling infinite loops
  goto escape   -- do not run the infinite loops
  ::a:: goto a
  ::b:: goto c
  ::c:: goto b
end
::escape::
--------------------------------------------------------------------------------
-- testing closing of upvalues

local debug = require 'debug'

local function foo ()
  local t = {}
  do
  local i = 1
  local a, b, c, d
  t[1] = function () return a, b, c, d end
  ::l1::
  local b
  do
    local c
    t[#t + 1] = function () return a, b, c, d end    -- t[2], t[4], t[6]
    if i > 2 then goto l2 end
    do
      local d
      t[#t + 1] = function () return a, b, c, d end   -- t[3], t[5]
      i = i + 1
      local a
      goto l1
    end
  end
  end
  ::l2:: return t
end

local a = foo()
assert(#a == 6)

-- all functions share same 'a'
for i = 2, 6 do
  assert(debug.upvalueid(a[1], 1) == debug.upvalueid(a[i], 1))
end

-- 'b' and 'c' are shared among some of them
for i = 2, 6 do
  -- only a[1] uses external 'b'/'b'
  assert(debug.upvalueid(a[1], 2) ~= debug.upvalueid(a[i], 2))
  assert(debug.upvalueid(a[1], 3) ~= debug.upvalueid(a[i], 3))
end

for i = 3, 5, 2 do
  -- inner functions share 'b'/'c' with previous ones
  assert(debug.upvalueid(a[i], 2) == debug.upvalueid(a[i - 1], 2))
  assert(debug.upvalueid(a[i], 3) == debug.upvalueid(a[i - 1], 3))
  -- but not with next ones
  assert(debug.upvalueid(a[i], 2) ~= debug.upvalueid(a[i + 1], 2))
  assert(debug.upvalueid(a[i], 3) ~= debug.upvalueid(a[i + 1], 3))
end

-- only external 'd' is shared
for i = 2, 6, 2 do
  assert(debug.upvalueid(a[1], 4) == debug.upvalueid(a[i], 4))
end

-- internal 'd's are all different
for i = 3, 5, 2 do
  for j = 1, 6 do
    assert((debug.upvalueid(a[i], 4) == debug.upvalueid(a[j], 4))
      == (i == j))
  end
end

--------------------------------------------------------------------------------
-- testing if x goto optimizations

local function testG (a)
  if a == 1 then
    goto l1
    error("should never be here!")
  elseif a == 2 then goto l2
  elseif a == 3 then goto l3
  elseif a == 4 then
    goto l1  -- go to inside the block
    error("should never be here!")
    ::l1:: a = a + 1   -- must go to 'if' end
  else
    goto l4
    ::l4a:: a = a * 2; goto l4b
    error("should never be here!")
    ::l4:: goto l4a
    error("should never be here!")
    ::l4b::
  end
  do return a end
  ::l2:: do return "2" end
  ::l3:: do return "3" end
  ::l1:: return "1"
end

assert(testG(1) == "1")
assert(testG(2) == "2")
assert(testG(3) == "3")
assert(testG(4) == 5)
assert(testG(5) == 10)

do   -- test goto's around to-be-closed variable

  global *

  -- set 'var' and return an object that will reset 'var' when
  -- it goes out of scope
  local function newobj (var)
    _ENV[var] = true
    return setmetatable({}, {__close = function ()
      _ENV[var] = nil
    end})
  end

  goto L1

  ::L4:: assert(not varX); goto L5   -- varX dead here

  ::L1::
  local varX <close> = newobj("X")
  assert(varX); goto L2   -- varX alive here

  ::L3::
  assert(varX); goto L4   -- varX alive here

  ::L2:: assert(varX); goto L3  -- varX alive here

  ::L5::   -- return
end



foo()
--------------------------------------------------------------------------

-- check for compilation errors
local function checkerr (code, err)
  local st, msg = load(code)
  assert(not st and string.find(msg, err))
end

do
  global T<const>

  -- globals must be declared, after a global declaration
  checkerr("global none; X = 1", "variable 'X'")
  checkerr("global none; function XX() end", "variable 'XX'")

  -- global variables cannot be to-be-closed
  checkerr("global X<close>", "cannot be")
  checkerr("global <close> *", "cannot be")

  do
    local X = 10
    do global X; X = 20 end
    assert(X == 10)   -- local X
  end
  assert(_ENV.X == 20)  -- global X

  -- '_ENV' cannot be global
  checkerr("global _ENV, a; a = 10", "variable 'a'")

  -- global declarations inside functions
  checkerr([[
    global none
    local function foo () XXX = 1 end   --< ERROR]], "variable 'XXX'")

  if not T then  -- when not in "test mode", "global" isn't reserved
    assert(load("global = 1; return global")() == 1)
    print "  ('global' is not a reserved word)"
  else
    -- "global" reserved, cannot be used as a variable
    assert(not load("global = 1; return global"))
  end

  local foo = 20
  do
    global function foo (x)
      if x == 0 then return 1 else return 2 * foo(x - 1) end
    end
    assert(foo == _ENV.foo and foo(4) == 16)
  end
  assert(_ENV.foo(4) == 16)
  assert(foo == 20)   -- local one is in context here

  do
    global foo;
    function foo (x) return end   -- Ok after declaration
  end

  checkerr([[
    global<const> foo;
    function foo (x) return end   -- ERROR: foo is read-only
  ]], "assign to const variable 'foo'")

  checkerr([[
    global foo <const>;
    function foo (x)    -- ERROR: foo is read-only
      return
    end
  ]], "%:2%:")   -- correct line in error message

  checkerr([[
    global<const> *;
    print(X)    -- Ok to use
    Y = 1   -- ERROR
  ]], "assign to const variable 'Y'")

  checkerr([[
    global *;
    Y = X    -- Ok to use
    global<const> *;
    Y = 1   -- ERROR
  ]], "assign to const variable 'Y'")

  global *
  Y = 10
  assert(_ENV.Y == 10)
  global<const> *
  local x = Y
  global *
  Y = x + Y
  assert(_ENV.Y == 20)
  Y = nil
end


do   -- Ok to declare hundreds of globals
  global table
  local code = {}
  for i = 1, 1000 do
    code[#code + 1] = ";global x" .. i
  end
  code[#code + 1] = "; return x990"
  code = table.concat(code)
  _ENV.x990 = 11
  assert(load(code)() == 11)
  _ENV.x990 = nil
end

do   -- mixing lots of global/local declarations
  global table
  local code = {}
  for i = 1, 200 do
    code[#code + 1] = ";global x" .. i
    code[#code + 1] = ";local y" .. i .. "=" .. (2*i)
  end
  code[#code + 1] = "; return x200 + y200"
  code = table.concat(code)
  _ENV.x200 = 11
  assert(assert(load(code))() == 2*200 + 11)
  _ENV.x200 = nil
end

do  print "testing initialization in global declarations"
  global<const> a, b, c = 10, 20, 30
  assert(_ENV.a == 10 and b == 20 and c == 30)
  _ENV.a = nil; _ENV.b = nil; _ENV.c = nil;

  global<const> a, b, c = 10
  assert(_ENV.a == 10 and b == nil and c == nil)
  _ENV.a = nil; _ENV.b = nil; _ENV.c = nil;

  global table
  global a, b, c, d = table.unpack{1, 2, 3, 6, 5}
  assert(_ENV.a == 1 and b == 2 and c == 3 and d == 6)
  a = nil; b = nil; c = nil; d = nil

  local a, b = 100, 200
  do
    global a, b = a, b
  end
  assert(_ENV.a == 100 and _ENV.b == 200)
  _ENV.a = nil; _ENV.b = nil


  assert(_ENV.a == nil and _ENV.b == nil and _ENV.c == nil and _ENV.d == nil)
end

do
  global table, string
  -- global initialization when names don't fit in K

  -- to fill constant table
  local code = {}
  for i = 1, 300 do code[i] = "'" .. i .. "'" end
  code = table.concat(code, ",")
  code = string.format([[
    return function (_ENV)
      local dummy = {%s}  -- fill initial positions in constant table,
      -- so that initialization must use registers for global names
      global a, b, c = 10, 20, 30
    end]], code)

  local fun = assert(load(code))()

  local env = {}
  fun(env)
  assert(env.a == 10 and env.b == 20 and env.c == 30)
end


do  -- testing global redefinitions
  -- cannot use 'checkerr' as errors are not compile time
  global pcall
  local f = assert(load("global print = 10"))
  local st, msg = pcall(f)
  assert(string.find(msg, "global 'print' already defined"))

  local f = assert(load("local _ENV = {AA = false}; global AA = 10"))
  local st, msg = pcall(f)
  assert(string.find(msg, "global 'AA' already defined"))

end

print'OK'

