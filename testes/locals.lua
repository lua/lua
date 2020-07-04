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

-- old test for limits for special instructions
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
  collectgarbage()   -- restore GC and collect dead fields in 'a'
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


do   -- constants
  local a<const>, b, c<const> = 10, 20, 30
  b = a + c + b    -- 'b' is not constant
  assert(a == 10 and b == 60 and c == 30)
  local function checkro (name, code)
    local st, msg = load(code)
    local gab = string.format("attempt to assign to const variable '%s'", name)
    assert(not st and string.find(msg, gab))
  end
  checkro("y", "local x, y <const>, z = 10, 20, 30; x = 11; y = 12")
  checkro("x", "local x <const>, y, z <const> = 10, 20, 30; x = 11")
  checkro("z", "local x <const>, y, z <const> = 10, 20, 30; y = 10; z = 11")

  checkro("z", [[
    local a, z <const>, b = 10;
    function foo() a = 20; z = 32; end
  ]])

  checkro("var1", [[
    local a, var1 <const> = 10;
    function foo() a = 20; z = function () var1 = 12; end  end
  ]])
end


print"testing to-be-closed variables"

local function stack(n) n = ((n == 0) or stack(n - 1)) end

local function func2close (f, x, y)
  local obj = setmetatable({}, {__close = f})
  if x then
    return x, obj, y
  else
    return obj
  end
end


do
  local a = {}
  do
    local b <close> = false   -- not to be closed
    local x <close> = setmetatable({"x"}, {__close = function (self)
                                                   a[#a + 1] = self[1] end})
    local w, y <close>, z = func2close(function (self, err)
                                assert(err == nil); a[#a + 1] = "y"
                              end, 10, 20)
    local c <close> = nil  -- not to be closed
    a[#a + 1] = "in"
    assert(w == 10 and z == 20)
  end
  a[#a + 1] = "out"
  assert(a[1] == "in" and a[2] == "y" and a[3] == "x" and a[4] == "out")
end

do
  local X = false

  local x, closescope = func2close(function () stack(10); X = true end, 100)
  assert(x == 100);  x = 101;   -- 'x' is not read-only

  -- closing functions do not corrupt returning values
  local function foo (x)
    local _ <close> = closescope
    return x, X, 23
  end

  local a, b, c = foo(1.5)
  assert(a == 1.5 and b == false and c == 23 and X == true)

  X = false
  foo = function (x)
    local  _<close> = closescope
    local y = 15
    return y
  end

  assert(foo() == 15 and X == true)

  X = false
  foo = function ()
    local x <close> = closescope
    return x
  end

  assert(foo() == closescope and X == true)

end


-- testing to-be-closed x compile-time constants
-- (there were some bugs here in Lua 5.4-rc3, due to a confusion
-- between compile levels and stack levels of variables)
do
  local flag = false
  local x = setmetatable({},
    {__close = function() assert(flag == false); flag = true end})
  local y <const> = nil
  local z <const> = nil
  do
      local a <close> = x
  end
  assert(flag)   -- 'x' must be closed here
end

do
  -- similar problem, but with implicit close in for loops
  local flag = false
  local x = setmetatable({},
    {__close = function () assert(flag == false); flag = true end})
  -- return an empty iterator, nil, nil, and 'x' to be closed
  local function a ()
    return (function () return nil end), nil, nil, x
  end
  local v <const> = 1
  local w <const> = 1
  local x <const> = 1
  local y <const> = 1
  local z <const> = 1
  for k in a() do
      a = k
  end    -- ending the loop must close 'x'
  assert(flag)   -- 'x' must be closed here
end



do
  -- calls cannot be tail in the scope of to-be-closed variables
  local X, Y
  local function foo ()
    local _ <close> = func2close(function () Y = 10 end)
    assert(X == true and Y == nil)    -- 'X' not closed yet
    return 1,2,3
  end

  local function bar ()
    local _ <close> = func2close(function () X = false end)
    X = true
    do
      return foo()    -- not a tail call!
    end
  end

  local a, b, c, d = bar()
  assert(a == 1 and b == 2 and c == 3 and X == false and Y == 10 and d == nil)
end


-- auxiliary functions for testing warnings in '__close'
local function prepwarn ()
  if not T then   -- no test library?
    warn("@off")      -- do not show (lots of) warnings
  else
    warn("@store")    -- to test the warnings
  end
end


local function endwarn ()
  if not T then
    warn("@on")          -- back to normal
  else
    assert(_WARN == false)
    warn("@normal")
  end
end


local function checkwarn (msg)
  if T then
    assert(string.find(_WARN, msg))
    _WARN = false    -- reset variable to check next warning
  end
end

warn("@on")

do print("testing errors in __close")

  prepwarn()

  -- original error is in __close
  local function foo ()

    local x <close> =
      func2close(function (self, msg)
        assert(string.find(msg, "@z"))
        error("@x")
      end)

    local x1 <close> =
      func2close(function (self, msg)
        checkwarn("@y")
        assert(string.find(msg, "@z"))
      end)

    local gc <close> = func2close(function () collectgarbage() end)

    local y <close> =
      func2close(function (self, msg)
        assert(string.find(msg, "@z"))  -- first error in 'z'
        checkwarn("@z")   -- second error in 'z' generated a warning
        error("@y")
      end)

    local first = true
    local z <close> =
      -- 'z' close is called twice
      func2close(function (self, msg)
        if first then
          assert(msg == nil)
          first = false
        else
         assert(string.find(msg, "@z"))   -- own error
        end
        error("@z")
      end)

    return 200
  end

  local stat, msg = pcall(foo, false)
  assert(string.find(msg, "@z"))
  checkwarn("@x")


  -- original error not in __close
  local function foo ()

    local x <close> =
      func2close(function (self, msg)
        assert(msg == 4)
      end)

    local x1 <close> =
      func2close(function (self, msg)
        checkwarn("@y")
        assert(msg == 4)
        error("@x1")
      end)

    local gc <close> = func2close(function () collectgarbage() end)

    local y <close> =
      func2close(function (self, msg)
        assert(msg == 4)   -- error in body
        checkwarn("@z")
        error("@y")
      end)

    local first = true
    local z <close> =
      func2close(function (self, msg)
        -- 'z' close is called once
        assert(first and msg == 4)
        first = false
        error("@z")
      end)

    error(4)    -- original error
  end

  local stat, msg = pcall(foo, true)
  assert(msg == 4)
  checkwarn("@x1")   -- last error

  -- error leaving a block
  local function foo (...)
    do
      local x1 <close> =
        func2close(function ()
          checkwarn("@X")
          error("@Y")
        end)

      local x123 <close> =
        func2close(function ()
          error("@X")
        end)
    end
    os.exit(false)    -- should not run
  end

  local st, msg = xpcall(foo, debug.traceback)
  assert(string.match(msg, "^[^ ]* @X"))
  assert(string.find(msg, "in metamethod 'close'"))
  checkwarn("@Y")

  -- error in toclose in vararg function
  local function foo (...)
    local x123 <close> = func2close(function () error("@x123") end)
  end

  local st, msg = xpcall(foo, debug.traceback)
  assert(string.match(msg, "^[^ ]* @x123"))
  assert(string.find(msg, "in metamethod 'close'"))
  checkwarn("@x123")   -- from second call to close 'x123'

  endwarn()
end


do   -- errors due to non-closable values
  local function foo ()
    local x <close> = {}
    os.exit(false)    -- should not run
  end
  local stat, msg = pcall(foo)
  assert(not stat and
    string.find(msg, "variable 'x' got a non%-closable value"))

  local function foo ()
    local xyz <close> = setmetatable({}, {__close = print})
    getmetatable(xyz).__close = nil   -- remove metamethod
  end
  local stat, msg = pcall(foo)
  assert(not stat and
    string.find(msg, "attempt to close non%-closable variable 'xyz'"))
end


if rawget(_G, "T") then

  warn("@off")

  -- memory error inside closing function
  local function foo ()
    local y <close> = func2close(function () T.alloccount() end)
    local x <close> = setmetatable({}, {__close = function ()
      T.alloccount(0); local x = {}   -- force a memory error
    end})
    error(1000)   -- common error inside the function's body
  end

  stack(5)    -- ensure a minimal number of CI structures

  -- despite memory error, 'y' will be executed and
  -- memory limit will be lifted
  local _, msg = pcall(foo)
  assert(msg == 1000)

  local close = func2close(function (self, msg)
    T.alloccount()
    assert(msg == "not enough memory")
  end)

  -- set a memory limit and return a closing object to remove the limit
  local function enter (count)
    stack(10)   -- reserve some stack space
    T.alloccount(count)
    return close
  end

  local function test ()
    local x <close> = enter(0)   -- set a memory limit
    -- creation of previous upvalue will raise a memory error
    assert(false)    -- should not run
  end

  local _, msg = pcall(test)
  assert(msg == "not enough memory")

  -- now use metamethod for closing
  close = setmetatable({}, {__close = function ()
    T.alloccount()
  end})

  -- repeat test with extra closing upvalues
  local function test ()
    local xxx <close> = func2close(function (self, msg)
      assert(msg == "not enough memory");
      error(1000)   -- raise another error
    end)
    local xx <close> = func2close(function (self, msg)
      assert(msg == "not enough memory");
    end)
    local x <close> = enter(0)   -- set a memory limit
    -- creation of previous upvalue will raise a memory error
    os.exit(false)    -- should not run
  end

  local _, msg = pcall(test)
  assert(msg == "not enough memory")   -- reported error is the first one

  do    -- testing 'toclose' in C string buffer
    collectgarbage()
    local s = string.rep('a', 10000)    -- large string
    local m = T.totalmem()
    collectgarbage("stop")
    s = string.upper(s)    -- allocate buffer + new string (10K each)
    -- ensure buffer was deallocated
    assert(T.totalmem() - m <= 11000)
    collectgarbage("restart")
  end

  do   -- now some tests for freeing buffer in case of errors
    local lim = 10000           -- some size larger than the static buffer
    local extra = 2000          -- some extra memory (for callinfo, etc.)

    local s = string.rep("a", lim)

    -- concat this table needs two buffer resizes (one for each 's')
    local a = {s, s}

    collectgarbage()

    m = T.totalmem()
    collectgarbage("stop")

    -- error in the first buffer allocation
    T. totalmem(m + extra)
    assert(not pcall(table.concat, a))
    -- first buffer was not even allocated
    assert(T.totalmem() - m <= extra)

    -- error in the second buffer allocation
    T. totalmem(m + lim + extra)
    assert(not pcall(table.concat, a))
    -- first buffer was released by 'toclose'
    assert(T.totalmem() - m <= extra)

    -- error in creation of final string
    T.totalmem(m + 2 * lim + extra)
    assert(not pcall(table.concat, a))
    -- second buffer was released by 'toclose'
    assert(T.totalmem() - m <= extra)

    -- userdata, upvalue, buffer, buffer, final string
    T.totalmem(m + 4*lim + extra)
    assert(#table.concat(a) == 2*lim)

    T.totalmem(0)     -- remove memory limit
    collectgarbage("restart")

    print'+'
  end

  warn("@on")
end


print "to-be-closed variables in coroutines"

do
  -- an error in a wrapped coroutine closes variables
  local x = false
  local y = false
  local co = coroutine.wrap(function ()
    local xv <close> = func2close(function () x = true end)
    do
      local yv <close> = func2close(function () y = true end)
      coroutine.yield(100)   -- yield doesn't close variable
    end
    coroutine.yield(200)   -- yield doesn't close variable
    error(23)              -- error does
  end)

  local b = co()
  assert(b == 100 and not x and not y)
  b = co()
  assert(b == 200 and not x and y)
  local a, b = pcall(co)
  assert(not a and b == 23 and x and y)
end


do
  prepwarn()

  -- error in a wrapped coroutine raising errors when closing a variable
  local x = 0
  local co = coroutine.wrap(function ()
    local xx <close> = func2close(function () x = x + 1; error("@YYY") end)
    local xv <close> = func2close(function () x = x + 1; error("@XXX") end)
      coroutine.yield(100)
      error(200)
  end)
  assert(co() == 100); assert(x == 0)
  local st, msg = pcall(co); assert(x == 2)
  assert(not st and msg == 200)   -- should get first error raised
  checkwarn("@YYY")

  local x = 0
  local y = 0
  co = coroutine.wrap(function ()
    local xx <close> = func2close(function () y = y + 1; error("YYY") end)
    local xv <close> = func2close(function () x = x + 1; error("XXX") end)
      coroutine.yield(100)
      return 200
  end)
  assert(co() == 100); assert(x == 0)
  local st, msg = pcall(co)
  assert(x == 2 and y == 1)   -- first close is called twice
  -- should get first error raised
  assert(not st and string.find(msg, "%w+%.%w+:%d+: XXX"))
  checkwarn("YYY")

  endwarn()
end


-- a suspended coroutine should not close its variables when collected
local co
co = coroutine.wrap(function()
  -- should not run
  local x <close> = func2close(function () os.exit(false) end)
  co = nil
  coroutine.yield()
end)
co()                 -- start coroutine
assert(co == nil)    -- eventually it will be collected
collectgarbage()


-- to-be-closed variables in generic for loops
do
  local numopen = 0
  local function open (x)
    numopen = numopen + 1
    return
      function ()   -- iteraction function
        x = x - 1
        if x > 0 then return x end
      end,
      nil,   -- state
      nil,   -- control variable
      func2close(function () numopen = numopen - 1 end)   -- closing function
  end

  local s = 0
  for i in open(10) do
     s = s + i
  end
  assert(s == 45 and numopen == 0)

  local s = 0
  for i in open(10) do
     if i < 5 then break end
     s = s + i
  end
  assert(s == 35 and numopen == 0)

  local s = 0
  for i in open(10) do
    for j in open(10) do
       if i + j < 5 then goto endloop end
       s = s + i
    end
  end
  ::endloop::
  assert(s == 375 and numopen == 0)
end

print('OK')

return 5,f

end   -- }

