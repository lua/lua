-- $Id: testes/vararg.lua $
-- See Copyright Notice in file lua.h

print('testing vararg')

local function f (a, ...t)
  local x = {n = select('#', ...), ...}
  assert(x.n == t.n)
  for i = 1, x.n do
    assert(a[i] == x[i] and x[i] == t[i])
  end
  return x.n
end

local function c12 (...)
  assert(arg == _G.arg)    -- no local 'arg'
  local x = {...}; x.n = #x
  local res = (x.n==2 and x[1] == 1 and x[2] == 2)
  if res then res = 55 end
  return res, 2
end

local function vararg (... t) return t end

local call = function (f, args) return f(table.unpack(args, 1, args.n)) end

assert(f() == 0)
assert(f({1,2,3}, 1, 2, 3) == 3)
assert(f({"alo", nil, 45, f, nil}, "alo", nil, 45, f, nil) == 5)

assert(vararg().n == 0)
assert(vararg(nil, nil).n == 2)

assert(c12(1,2)==55)
local a,b = assert(call(c12, {1,2}))
assert(a == 55 and b == 2)
a = call(c12, {1,2;n=2})
assert(a == 55 and b == 2)
a = call(c12, {1,2;n=1})
assert(not a)
assert(c12(1,2,3) == false)
local a = vararg(call(next, {_G,nil;n=2}))
local b,c = next(_G)
assert(a[1] == b and a[2] == c and a.n == 2)
a = vararg(call(call, {c12, {1,2}}))
assert(a.n == 2 and a[1] == 55 and a[2] == 2)
a = call(print, {'+'})
assert(a == nil)

local t = {1, 10}
function t:f (...) local arg = {...}; return self[...]+#arg end
assert(t:f(1,4) == 3 and t:f(2) == 11)
print('+')

local lim = 20
local i, a = 1, {}
while i <= lim do a[i] = i+0.3; i=i+1 end

function f(a, b, c, d, ...)
  local more = {...}
  assert(a == 1.3 and more[1] == 5.3 and
         more[lim-4] == lim+0.3 and not more[lim-3])
end

local function g (a,b,c)
  assert(a == 1.3 and b == 2.3 and c == 3.3)
end

call(f, a)
call(g, a)

a = {}
i = 1
while i <= lim do a[i] = i; i=i+1 end
assert(call(math.max, a) == lim)

print("+")


-- new-style varargs

local function oneless (a, ...) return ... end

function f (n, a, ...)
  local b
  assert(arg == _G.arg)   -- no local 'arg'
  if n == 0 then
    local b, c, d = ...
    return a, b, c, d, oneless(oneless(oneless(...)))
  else
    n, b, a = n-1, ..., a
    assert(b == ...)
    return f(n, a, ...)
  end
end

a,b,c,d,e = assert(f(10,5,4,3,2,1))
assert(a==5 and b==4 and c==3 and d==2 and e==1)

a,b,c,d,e = f(4)
assert(a==nil and b==nil and c==nil and d==nil and e==nil)


-- varargs for main chunks
local f = assert(load[[ return {...} ]])
local x = f(2,3)
assert(x[1] == 2 and x[2] == 3 and x[3] == undef)


f = load[[
  local x = {...}
  for i=1,select('#', ...) do assert(x[i] == select(i, ...)) end
  assert(x[select('#', ...)+1] == undef)
  return true
]]

assert(f("a", "b", nil, {}, assert))
assert(f())

a = {select(3, table.unpack{10,20,30,40})}
assert(#a == 2 and a[1] == 30 and a[2] == 40)
a = {select(1)}
assert(next(a) == nil)
a = {select(-1, 3, 5, 7)}
assert(a[1] == 7 and a[2] == undef)
a = {select(-2, 3, 5, 7)}
assert(a[1] == 5 and a[2] == 7 and a[3] == undef)
pcall(select, 10000)
pcall(select, -10000)


-- bug in 5.2.2

function f(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,
p11, p12, p13, p14, p15, p16, p17, p18, p19, p20,
p21, p22, p23, p24, p25, p26, p27, p28, p29, p30,
p31, p32, p33, p34, p35, p36, p37, p38, p39, p40,
p41, p42, p43, p44, p45, p46, p48, p49, p50, ...)
  local a1,a2,a3,a4,a5,a6,a7
  local a8,a9,a10,a11,a12,a13,a14
end

-- assertion fail here
f()

-- missing arguments in tail call
do
  local function f(a,b,c) return c, b end
  local function g() return f(1,2) end
  local a, b = g()
  assert(a == nil and b == 2)
end


do  -- vararg parameter used in nested functions
  local function foo (...tab1)
    return function (...tab2)
      return {tab1, tab2}
    end
  end
  local f = foo(10, 20, 30)
  local t = f("a", "b")
  assert(t[1].n == 3 and t[1][1] == 10)
  assert(t[2].n == 2 and t[2][1] == "a")
end

do  -- vararg parameter is read-only
  local st, msg = load("return function (... t) t = 10 end")
  assert(string.find(msg, "const variable 't'"))

  local st, msg = load[[
    local function foo (...extra)
      return function (...) extra = nil end
    end
  ]]
  assert(string.find(msg, "const variable 'extra'"))
end


do  -- _ENV as vararg parameter
  local st, msg = load[[
    local function aux (... _ENV)
      global <const> a
      a = 10
    end ]]
  assert(string.find(msg, "const variable 'a'"))

  local function aux (..._ENV)
    global a; a = 10
    return a
  end
  assert(aux() == 10)

  local function aux (... _ENV)
    global a = 10
    return a
  end
  assert(aux() == 10)
end


do   -- access to vararg parameter
  local function notab (keys, t, ...v)
    for _, k in pairs(keys) do
      assert(t[k] == v[k])
    end
    assert(t.n == v.n)
  end

  local t = table.pack(10, 20, 30)
  local keys = {-1, 0, 1, t.n, t.n + 1, 1.0, 1.1, "n", print, "k", "1"}
  notab(keys, t, 10, 20, 30)    -- ensure stack space
  local m = collectgarbage"count"
  notab(keys, t, 10, 20, 30)
  -- 'notab' does not create any table/object
  assert(m == collectgarbage"count")

  -- writing to the vararg table
  local function foo (...t)
    t[1] = t[1] + 10
    return t[1]
  end
  assert(foo(10, 30) == 20)
end

print('OK')

