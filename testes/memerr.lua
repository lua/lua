-- $Id: testes/memerr.lua $
-- See Copyright Notice in file lua.h


local function checkerr (msg, f, ...)
  local stat, err = pcall(f, ...)
  assert(not stat and string.find(err, msg))
end

if T==nil then
  (Message or print)
      ('\n >>> testC not active: skipping memory error tests <<<\n')
  return
end

print("testing memory-allocation errors")

local debug = require "debug"

local pack = table.pack

-- standard error message for memory errors
local MEMERRMSG = "not enough memory"


-- memory error in panic function
T.totalmem(T.totalmem()+10000)   -- set low memory limit (+10k)
assert(T.checkpanic("newuserdata 20000") == MEMERRMSG)
T.totalmem(0)          -- restore high limit



-- {==================================================================
-- Testing memory limits
-- ===================================================================

checkerr("block too big", T.newuserdata, math.maxinteger)
collectgarbage()
local f = load"local a={}; for i=1,100000 do a[i]=i end"
T.alloccount(10)
checkerr(MEMERRMSG, f)
T.alloccount()          -- remove limit


-- preallocate stack space
local function deep (n) if n > 0 then deep(n - 1) end end


-- test memory errors; increase limit for maximum memory by steps,
-- so that we get memory errors in all allocations of a given
-- task, until there is enough memory to complete the task without
-- errors.
local function testbytes (s, f)
  collectgarbage()
  local M = T.totalmem()
  local oldM = M
  local a,b = nil
  while true do
    collectgarbage(); collectgarbage()
    deep(4)
    T.totalmem(M)
    a, b = T.testC("pcall 0 1 0; pushstatus; return 2", f)
    T.totalmem(0)  -- remove limit
    if a and b == "OK" then break end       -- stop when no more errors
    if b ~= "OK" and b ~= MEMERRMSG then    -- not a memory error?
      error(a, 0)   -- propagate it
    end
    M = M + 7   -- increase memory limit
  end
  print(string.format("minimum memory for %s: %d bytes", s, M - oldM))
  return a
end

-- test memory errors; increase limit for number of allocations one
-- by one, so that we get memory errors in all allocations of a given
-- task, until there is enough allocations to complete the task without
-- errors.

local function testalloc (s, f)
  collectgarbage()
  local M = 0
  local a,b = nil
  while true do
    collectgarbage(); collectgarbage()
    deep(4)
    T.alloccount(M)
    a, b = T.testC("pcall 0 1 0; pushstatus; return 2", f)
    T.alloccount()  -- remove limit
    if a and b == "OK" then break end       -- stop when no more errors
    if b ~= "OK" and b ~= MEMERRMSG then    -- not a memory error?
      error(a, 0)   -- propagate it
    end
    M = M + 1   -- increase allocation limit
  end
  print(string.format("minimum allocations for %s: %d allocations", s, M))
  return M
end


local function testamem (s, f)
  local aloc = testalloc(s, f)
  local res = testbytes(s, f)
  return {aloc = aloc, res = res}
end


local b = testamem("function call", function () return 10 end)
assert(b.res == 10 and b.aloc == 0)

testamem("state creation", function ()
  local st = T.newstate()
  if st then T.closestate(st) end   -- close new state
  return st
end)

testamem("empty-table creation", function ()
  return {}
end)

testamem("string creation", function ()
  return "XXX" .. "YYY"
end)

testamem("coroutine creation", function()
           return coroutine.create(print)
end)

do  -- vararg tables
  local function pack (...t) return t end
  local b = testamem("vararg table", function ()
    return pack(10, 20, 30, 40, "hello")
  end)
  assert(b.aloc == 3)   -- new table uses three memory blocks
  -- table optimized away
  local function sel (n, ...arg) return arg[n] + arg.n end
  local b = testamem("optimized vararg table",
        function () return sel(2.0, 20, 30) end)
  assert(b.res == 32 and b.aloc == 0)   -- no memory needed for this case
end

-- testing to-be-closed variables
testamem("to-be-closed variables", function()
  local flag
  do
    local x <close> =
              setmetatable({}, {__close = function () flag = true end})
    flag = false
    local x = {}
  end
  return flag
end)


-- testing threads

-- get main thread from registry
local mt = T.testC("rawgeti R !M; return 1")
assert(type(mt) == "thread" and coroutine.running() == mt)



local function expand (n,s)
  if n==0 then return "" end
  local e = string.rep("=", n)
  return string.format("T.doonnewstack([%s[ %s;\n collectgarbage(); %s]%s])\n",
                              e, s, expand(n-1,s), e)
end

G=0; collectgarbage()
load(expand(20,"G=G+1"))()
assert(G==20); collectgarbage()
G = nil

testamem("running code on new thread", function ()
  return T.doonnewstack("local x=1") == 0  -- try to create thread
end)


do   -- external strings
  local str = string.rep("a", 100)
  testamem("creating external strings", function ()
    return T.externstr(str)
  end)
end


-- testing memory x compiler

testamem("loadstring", function ()
  return load("x=1")  -- try to do load a string
end)


local testprog = [[
local function foo () return end
local t = {"x"}
AA = "aaa"
for i = 1, #t do AA = AA .. t[i] end
return true
]]

-- testing memory x dofile
_G.AA = nil
local t =os.tmpname()
local f = assert(io.open(t, "w"))
f:write(testprog)
f:close()
testamem("dofile", function ()
  local a = loadfile(t)
  return a and a()
end)
assert(os.remove(t))
assert(_G.AA == "aaax")


-- other generic tests

testamem("gsub", function ()
  local a, b = string.gsub("alo alo", "(a)", function (x) return x..'b' end)
  return (a == 'ablo ablo')
end)

testamem("dump/undump", function ()
  local a = load(testprog)
  local b = a and string.dump(a)
  a = b and load(b)
  return a and a()
end)

_G.AA = nil

local t = os.tmpname()
testamem("file creation", function ()
  local f = assert(io.open(t, 'w'))
  assert (not io.open"nomenaoexistente")
  io.close(f);
  return not loadfile'nomenaoexistente'
end)
assert(os.remove(t))

testamem("table creation", function ()
  local a, lim = {}, 10
  for i=1,lim do a[i] = i; a[i..'a'] = {} end
  return (type(a[lim..'a']) == 'table' and a[lim] == lim)
end)

testamem("constructors", function ()
  local a = {10, 20, 30, 40, 50; a=1, b=2, c=3, d=4, e=5}
  return (type(a) == 'table' and a.e == 5)
end)

local a = 1
local close = nil
testamem("closure creation", function ()
  function close (b)
   return function (x) return b + x end
  end
  return (close(2)(4) == 6)
end)

testamem("using coroutines", function ()
  local a = coroutine.wrap(function ()
              coroutine.yield(string.rep("a", 10))
              return {}
            end)
  assert(string.len(a()) == 10)
  return a()
end)

do   -- auxiliary buffer
  local lim = 100
  local a = {}; for i = 1, lim do a[i] = "01234567890123456789" end
  testamem("auxiliary buffer", function ()
    return (#table.concat(a, ",") == 20*lim + lim - 1)
  end)
end

testamem("growing stack", function ()
  local function foo (n)
    if n == 0 then return 1 else return 1 + foo(n - 1) end
  end
  return foo(100)
end)

-- }==================================================================


print "Ok"


