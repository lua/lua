-- $Id: testes/code.lua $
-- See Copyright Notice in file all.lua

if T==nil then
  (Message or print)('\n >>> testC not active: skipping opcode tests <<<\n')
  return
end
print "testing code generation and optimizations"


-- this code gave an error for the code checker
do
  local function f (a)
  for k,v,w in a do end
  end
end


-- testing reuse in constant table
local function checkKlist (func, list)
  local k = T.listk(func)
  assert(#k == #list)
  for i = 1, #k do
    assert(k[i] == list[i] and math.type(k[i]) == math.type(list[i]))
  end
end

local function foo ()
  local a
  a = 3;
  a = 0; a = 0.0; a = -7 + 7
  a = 3.78/4; a = 3.78/4
  a = -3.78/4; a = 3.78/4; a = -3.78/4
  a = -3.79/4; a = 0.0; a = -0;
  a = 3; a = 3.0; a = 3; a = 3.0
end

checkKlist(foo, {3.78/4, -3.78/4, -3.79/4})


-- testing opcodes

function check (f, ...)
  local arg = {...}
  local c = T.listcode(f)
  for i=1, #arg do
    local opcode = string.match(c[i], "%u%w+")
    -- print(arg[i], opcode)
    assert(arg[i] == opcode)
  end
  assert(c[#arg+2] == undef)
end


function checkequal (a, b)
  a = T.listcode(a)
  b = T.listcode(b)
  for i = 1, #a do
    a[i] = string.gsub(a[i], '%b()', '')   -- remove line number
    b[i] = string.gsub(b[i], '%b()', '')   -- remove line number
    assert(a[i] == b[i])
  end
end


-- some basic instructions
check(function ()   -- function does not create upvalues
  (function () end){f()}
end, 'CLOSURE', 'NEWTABLE', 'GETTABUP', 'CALL', 'SETLIST', 'CALL', 'RETURN0')

check(function (x)   -- function creates upvalues
  (function () return x end){f()}
end, 'CLOSURE', 'NEWTABLE', 'GETTABUP', 'CALL', 'SETLIST', 'CALL', 'RETURN')


-- sequence of LOADNILs
check(function ()
  local a,b,c
  local d; local e;
  local f,g,h;
  d = nil; d=nil; b=nil; a=nil; c=nil;
end, 'LOADNIL', 'RETURN0')

check(function ()
  local a,b,c,d = 1,1,1,1
  d=nil;c=nil;b=nil;a=nil
end, 'LOADI', 'LOADI', 'LOADI', 'LOADI', 'LOADNIL', 'RETURN0')

do
  local a,b,c,d = 1,1,1,1
  d=nil;c=nil;b=nil;a=nil
  assert(a == nil and b == nil and c == nil and d == nil)
end


-- single return
check (function (a,b,c) return a end, 'RETURN1')


-- infinite loops
check(function () while true do local a = -1 end end,
'LOADI', 'JMP', 'RETURN0')

check(function () while 1 do local a = -1 end end,
'LOADI', 'JMP', 'RETURN0')

check(function () repeat local x = 1 until true end,
'LOADI', 'RETURN0')


-- concat optimization
check(function (a,b,c,d) return a..b..c..d end,
  'MOVE', 'MOVE', 'MOVE', 'MOVE', 'CONCAT', 'RETURN1')

-- not
check(function () return not not nil end, 'LOADBOOL', 'RETURN1')
check(function () return not not false end, 'LOADBOOL', 'RETURN1')
check(function () return not not true end, 'LOADBOOL', 'RETURN1')
check(function () return not not 1 end, 'LOADBOOL', 'RETURN1')

-- direct access to locals
check(function ()
  local a,b,c,d
  a = b*a
  c.x, a[b] = -((a + d/b - a[b]) ^ a.x), b
end,
  'LOADNIL',
  'MUL',
  'DIV', 'ADD', 'GETTABLE', 'SUB', 'GETFIELD', 'POW',
    'UNM', 'SETTABLE', 'SETFIELD', 'RETURN0')


-- direct access to constants
check(function ()
  local a,b
  a.x = 3.2
  a.x = b
  a[b] = 'x'
end,
  'LOADNIL', 'SETFIELD', 'SETFIELD', 'SETTABLE', 'RETURN0')

-- "get/set table" with numeric indices
check(function (a)
  a[1] = a[100]
  a[255] = a[256]
  a[256] = 5
end,
  'GETI', 'SETI',
  'LOADI', 'GETTABLE', 'SETI',
  'LOADI', 'SETTABLE',  'RETURN0')

check(function ()
  local a,b
  a = a - a
  b = a/a
  b = 5-4
end,
  'LOADNIL', 'SUB', 'DIV', 'LOADI', 'RETURN0')

check(function ()
  local a,b
  a[true] = false
end,
  'LOADNIL', 'LOADBOOL', 'SETTABLE', 'RETURN0')


-- equalities
check(function (a) if a == 1 then return 2 end end,
  'EQI', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if -4.0 == a then return 2 end end,
  'EQI', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if a == "hi" then return 2 end end,
  'EQK', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if a == 10000 then return 2 end end,
  'EQK', 'JMP', 'LOADI', 'RETURN1')   -- number too large

check(function (a) if -10000 == a then return 2 end end,
  'EQK', 'JMP', 'LOADI', 'RETURN1')   -- number too large

-- comparisons

check(function (a) if -10 <= a then return 2 end end,
  'GEI', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if 128.0 > a then return 2 end end,
  'LTI', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if -127.0 < a then return 2 end end,
  'GTI', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if 10 < a then return 2 end end,
  'GTI', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if 129 < a then return 2 end end,
  'LOADI', 'LT', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if a >= 23.0 then return 2 end end,
  'GEI', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if a >= 23.1 then return 2 end end,
  'LOADK', 'LE', 'JMP', 'LOADI', 'RETURN1')

check(function (a) if a > 2300.0 then return 2 end end,
  'LOADF', 'LT', 'JMP', 'LOADI', 'RETURN1')


-- constant folding
local function checkK (func, val)
  check(func, 'LOADK', 'RETURN1')
  local k = T.listk(func)
  assert(#k == 1 and k[1] == val and math.type(k[1]) == math.type(val))
  assert(func() == val)
end

local function checkI (func, val)
  check(func, 'LOADI', 'RETURN1')
  assert(#T.listk(func) == 0)
  assert(func() == val)
end

local function checkF (func, val)
  check(func, 'LOADF', 'RETURN1')
  assert(#T.listk(func) == 0)
  assert(func() == val)
end

checkF(function () return 0.0 end, 0.0)
checkI(function () return 0 end, 0)
checkI(function () return -0//1 end, 0)
checkK(function () return 3^-1 end, 1/3)
checkK(function () return (1 + 1)^(50 + 50) end, 2^100)
checkK(function () return (-2)^(31 - 2) end, -0x20000000 + 0.0)
checkF(function () return (-3^0 + 5) // 3.0 end, 1.0)
checkI(function () return -3 % 5 end, 2)
checkF(function () return -((2.0^8 + -(-1)) % 8)/2 * 4 - 3 end, -5.0)
checkF(function () return -((2^8 + -(-1)) % 8)//2 * 4 - 3 end, -7.0)
checkI(function () return 0xF0.0 | 0xCC.0 ~ 0xAA & 0xFD end, 0xF4)
checkI(function () return ~(~0xFF0 | 0xFF0) end, 0)
checkI(function () return ~~-1024.0 end, -1024)
checkI(function () return ((100 << 6) << -4) >> 2 end, 100)

-- borders around MAXARG_sBx ((((1 << 17) - 1) >> 1) == 65535)
local a = 17; local sbx = ((1 << a) - 1) >> 1   -- avoid folding
checkI(function () return 65535 end, sbx)
checkI(function () return -65535 end, -sbx)
checkI(function () return 65536 end, sbx + 1)
checkK(function () return 65537 end, sbx + 2)
checkK(function () return -65536 end, -(sbx + 1))

checkF(function () return 65535.0 end, sbx + 0.0)
checkF(function () return -65535.0 end, -sbx + 0.0)
checkF(function () return 65536.0 end, (sbx + 1.0))
checkK(function () return 65537.0 end, (sbx + 2.0))
checkK(function () return -65536.0 end, -(sbx + 1.0))


-- immediate operands
check(function (x) return x + 1 end, 'ADDI', 'RETURN1')
check(function (x) return 128 + x end, 'ADDI', 'RETURN1')
check(function (x) return x * -127 end, 'MULI', 'RETURN1')
check(function (x) return 20 * x end, 'MULI', 'RETURN1')
check(function (x) return x ^ -2 end, 'POWI', 'RETURN1')
check(function (x) return x / 40 end, 'DIVI', 'RETURN1')
check(function (x) return x // 1 end, 'IDIVI', 'RETURN1')
check(function (x) return x % (100 - 10) end, 'MODI', 'RETURN1')
check(function (x) return 1 << x end, 'SHLI', 'RETURN1')
check(function (x) return x << 2 end, 'SHRI', 'RETURN1')
check(function (x) return x >> 2 end, 'SHRI', 'RETURN1')
check(function (x) return x & 1 end, 'BANDK', 'RETURN1')
check(function (x) return 10 | x end, 'BORK', 'RETURN1')
check(function (x) return -10 ~ x end, 'BXORK', 'RETURN1')

-- no foldings (and immediate operands)
check(function () return -0.0 end, 'LOADF', 'UNM', 'RETURN1')
check(function () return 3/0 end, 'LOADI', 'DIVI', 'RETURN1')
check(function () return 0%0 end, 'LOADI', 'MODI', 'RETURN1')
check(function () return -4//0 end, 'LOADI', 'IDIVI', 'RETURN1')
check(function (x) return x >> 2.0 end, 'LOADF', 'SHR', 'RETURN1')
check(function (x) return x & 2.0 end, 'LOADF', 'BAND', 'RETURN1')

-- basic 'for' loops
check(function () for i = -10, 10.5 do end end,
'LOADI', 'LOADK', 'LOADI', 'FORPREP1', 'FORLOOP1', 'RETURN0')
check(function () for i = 0xfffffff, 10.0, 1 do end end,
'LOADK', 'LOADF', 'LOADI', 'FORPREP1', 'FORLOOP1', 'RETURN0')

-- bug in constant folding for 5.1
check(function () return -nil end, 'LOADNIL', 'UNM', 'RETURN1')


check(function ()
  local a,b,c
  b[c], a = c, b
  b[a], a = c, b
  a, b = c, a
  a = a
end, 
  'LOADNIL',
  'MOVE', 'MOVE', 'SETTABLE',
  'MOVE', 'MOVE', 'MOVE', 'SETTABLE',
  'MOVE', 'MOVE', 'MOVE',
  -- no code for a = a
  'RETURN0')


-- x == nil , x ~= nil
-- checkequal(function (b) if (a==nil) then a=1 end; if a~=nil then a=1 end end,
--            function () if (a==9) then a=1 end; if a~=9 then a=1 end end)

-- check(function () if a==nil then a='a' end end,
-- 'GETTABUP', 'EQ', 'JMP', 'SETTABUP', 'RETURN')

do   -- tests for table access in upvalues
  local t
  check(function () t.x = t.y end, 'GETTABUP', 'SETTABUP')
  check(function (a) t[a()] = t[a()] end,
  'MOVE', 'CALL', 'GETUPVAL', 'MOVE', 'CALL',
  'GETUPVAL', 'GETTABLE', 'SETTABLE')
end

-- de morgan
checkequal(function () local a; if not (a or b) then b=a end end,
           function () local a; if (not a and not b) then b=a end end)

checkequal(function (l) local a; return 0 <= a and a <= l end,
           function (l) local a; return not (not(a >= 0) or not(a <= l)) end)


-- if-goto optimizations
check(function (a, b, c, d, e)
        if a == b then goto l1;
        elseif a == c then goto l2;
        elseif a == d then goto l2;
        else if a == e then goto l3;
             else goto l3
             end
        end
        ::l1:: ::l2:: ::l3:: ::l4:: 
end, 'EQ', 'JMP', 'EQ', 'JMP', 'EQ', 'JMP', 'EQ', 'JMP', 'JMP',
'CLOSE', 'CLOSE', 'CLOSE', 'CLOSE', 'RETURN0')

checkequal(
function (a) while a < 10 do a = a + 1 end end,
function (a) while true do if not(a < 10) then break end; a = a + 1; end end
)

print 'OK'

