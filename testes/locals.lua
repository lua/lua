-- $Id: testes/locals.lua $
-- See Copyright Notice in file all.lua

print('testing local variables and environments')

local debug = require"debug"


-- bug in 5.1:

local function f(x) x = nil; return x end
assert(f(10) == nil)

local function f() local x; return x end
assert(f(10) == nil)

local function f(x) x = nil; local y; return x, y end
assert(f(10) == nil and select(2, f(20)) == nil)

do
  local i = 10
  do local i = 100; assert(i==100) end
  do local i = 1000; assert(i==1000) end
  assert(i == 10)
  if i ~= 10 then
    local i = 20
  else
    local i = 30
    assert(i == 30)
  end
end



f = nil

local f
x = 1

a = nil
load('local a = {}')()
assert(a == nil)

function f (a)
  local _1, _2, _3, _4, _5
  local _6, _7, _8, _9, _10
  local x = 3
  local b = a
  local c,d = a,b
  if (d == b) then
    local x = 'q'
    x = b
    assert(x == 2)
  else
    assert(nil)
  end
  assert(x == 3)
  local f = 10
end

local b=10
local a; repeat local b; a,b=1,2; assert(a+1==b); until a+b==3


assert(x == 1)

f(2)
assert(type(f) == 'function')


local function getenv (f)
  local a,b = debug.getupvalue(f, 1)
  assert(a == '_ENV')
  return b
end

-- test for global table of loaded chunks
assert(getenv(load"a=3") == _G)
local c = {}; local f = load("a = 3", nil, nil, c)
assert(getenv(f) == c)
assert(c.a == nil)
f()
assert(c.a == 3)

-- old test for limits for special instructions (now just a generic test)
do
  local i = 2
  local p = 4    -- p == 2^i
  repeat
    for j=-3,3 do
      assert(load(string.format([[local a=%s;
                                        a=a+%s;
                                        assert(a ==2^%s)]], j, p-j, i), '')) ()
      assert(load(string.format([[local a=%s;
                                        a=a-%s;
                                        assert(a==-2^%s)]], -j, p-j, i), '')) ()
      assert(load(string.format([[local a,b=0,%s;
                                        a=b-%s;
                                        assert(a==-2^%s)]], -j, p-j, i), '')) ()
    end
    p = 2 * p;  i = i + 1
  until p <= 0
end

print'+'


if rawget(_G, "T") then
  -- testing clearing of dead elements from tables
  collectgarbage("stop")   -- stop GC
  local a = {[{}] = 4, [3] = 0, alo = 1, 
             a1234567890123456789012345678901234567890 = 10}

  local t = T.querytab(a)

  for k,_ in pairs(a) do a[k] = undef end
  collectgarbage()   -- restore GC and collect dead fiels in `a'
  for i=0,t-1 do
    local k = querytab(a, i)
    assert(k == nil or type(k) == 'number' or k == 'alo')
  end

  -- testing allocation errors during table insertions
  local a = {}
  local function additems ()
    a.x = true; a.y = true; a.z = true
    a[1] = true
    a[2] = true
  end
  for i = 1, math.huge do
    T.alloccount(i)
    local st, msg = pcall(additems)
    T.alloccount()
    local count = 0
    for k, v in pairs(a) do
      assert(a[k] == v)
      count = count + 1
    end
    if st then assert(count == 5); break end
  end
end


-- testing lexical environments

assert(_ENV == _G)

do
local dummy
local _ENV = (function (...) return ... end)(_G, dummy)   -- {

do local _ENV = {assert=assert}; assert(true) end
mt = {_G = _G}
local foo,x
A = false    -- "declare" A
do local _ENV = mt
  function foo (x)
    A = x
    do local _ENV =  _G; A = 1000 end
    return function (x) return A .. x end
  end
end
assert(getenv(foo) == mt)
x = foo('hi'); assert(mt.A == 'hi' and A == 1000)
assert(x('*') == mt.A .. '*')

do local _ENV = {assert=assert, A=10};
  do local _ENV = {assert=assert, A=20};
    assert(A==20);x=A
  end
  assert(A==10 and x==20)
end
assert(x==20)


print"testing to-be-closed variables"

do
  local a = {}
  do
    local scoped x = setmetatable({"x"}, {__close = function (self)
                                                   a[#a + 1] = self[1] end})
    local scoped y = function (x) assert(x == nil); a[#a + 1] = "y" end
    a[#a + 1] = "in"
  end
  a[#a + 1] = "out"
  assert(a[1] == "in" and a[2] == "y" and a[3] == "x" and a[4] == "out")
end


do   -- errors in __close
  local log = {}
  local function foo (err)
    local scoped x = function (msg) log[#log + 1] = msg; error(1) end
    local scoped x1 = function (msg) log[#log + 1] = msg; end
    local scoped gc = function () collectgarbage() end
    local scoped y = function (msg) log[#log + 1] = msg; error(2) end
    local scoped z = function (msg) log[#log + 1] = msg or 10; error(3) end
    if err then error(4) end
  end
  local stat, msg = pcall(foo, false)
  assert(msg == 1)
  assert(log[1] == 10 and log[2] == 3 and log[3] == 2 and log[4] == 2
         and #log == 4)

  log = {}
  local stat, msg = pcall(foo, true)
  assert(msg == 1)
  assert(log[1] == 4 and log[2] == 3 and log[3] == 2 and log[4] == 2
         and #log == 4)
end

if rawget(_G, "T") then
  local function stack(n) n = (n == 0) or stack(n - 1); end;
  -- memory error inside closing function
  local function foo ()
    local scoped y = function () T.alloccount() end
    local scoped x = setmetatable({}, {__close = function ()
      T.alloccount(0); local x = {}   -- force a memory error
    end})
    error("a")   -- common error inside the function's body
  end

  stack(5)    -- ensure a minimal number of CI structures

  -- despite memory error, 'y' will be executed and
  -- memory limit will be lifted
  local _, msg = pcall(foo)
  assert(msg == "not enough memory")

  local function close (msg)
    T.alloccount()
    assert(msg == "not enough memory")
  end

  -- set a memory limit and return a closing function to remove the limit
  local function enter (count)
    stack(10)   -- reserve some stack space
    T.alloccount(count)
    return close
  end

  local function test ()
    local scoped x = enter(0)   -- set a memory limit
    -- creation of previous upvalue will raise a memory error
    os.exit(false)    -- should not run
  end

  local _, msg = pcall(test)
  assert(msg == "not enough memory")

  -- now use metamethod for closing
  close = setmetatable({}, {__close = function ()
    T.alloccount()
  end})

  -- repeat test with extra closing upvalues
  local function test ()
    local scoped xxx = function (msg)
      assert(msg == "not enough memory");
      error(1000)   -- raise another error
    end
    local scoped xx = function (msg)
      assert(msg == "not enough memory");
    end
    local scoped x = enter(0)   -- set a memory limit
    -- creation of previous upvalue will raise a memory error
    os.exit(false)    -- should not run
  end

  local _, msg = pcall(test)
  assert(msg == 1000)

end


-- a suspended coroutine should not close its variables when collected
local co = coroutine.wrap(function()
  local scoped x = function () os.exit(false) end    -- should not run
   coroutine.yield()
end)
co()
co = nil

print('OK')

return 5,f

end   -- }

