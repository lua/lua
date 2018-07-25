-- $Id: testes/gengc.lua $
-- See Copyright Notice in file all.lua

print('testing generational garbage collection')

local debug = require"debug"

assert(collectgarbage("isrunning"))

collectgarbage()

local oldmode = collectgarbage("generational")


-- ensure that table barrier evolves correctly
do
  local U = {}
  -- full collection makes 'U' old
  collectgarbage()
  assert(not T or T.gcage(U) == "old")

  -- U refers to a new table, so it becomes 'touched1'
  U[1] = {x = {234}}
  assert(not T or (T.gcage(U) == "touched1" and T.gcage(U[1]) == "new"))

  -- both U and the table survive one more collection
  collectgarbage("step", 0)
  assert(not T or (T.gcage(U) == "touched2" and T.gcage(U[1]) == "survival"))

  -- both U and the table survive yet another collection
  -- now everything is old
  collectgarbage("step", 0)
  assert(not T or (T.gcage(U) == "old" and T.gcage(U[1]) == "old1"))

  -- data was not corrupted
  assert(U[1].x[1] == 234)
end


if T == nil then
  (Message or print)('\n >>> testC not active: \z
                             skipping some generational tests <<<\n')
  print 'OK'
  return
end


-- ensure that userdata barrier evolves correctly
do
  local U = T.newuserdata(0, 1)
  -- full collection makes 'U' old
  collectgarbage()
  assert(T.gcage(U) == "old")

  -- U refers to a new table, so it becomes 'touched1'
  debug.setuservalue(U, {x = {234}})
  assert(T.gcage(U) == "touched1" and
         T.gcage(debug.getuservalue(U)) == "new")

  -- both U and the table survive one more collection
  collectgarbage("step", 0)
  assert(T.gcage(U) == "touched2" and
         T.gcage(debug.getuservalue(U)) == "survival")

  -- both U and the table survive yet another collection
  -- now everything is old
  collectgarbage("step", 0)
  assert(T.gcage(U) == "old" and
         T.gcage(debug.getuservalue(U)) == "old1")

  -- data was not corrupted
  assert(debug.getuservalue(U).x[1] == 234)
end



-- just to make sure
assert(collectgarbage'isrunning')

collectgarbage(oldmode)

print('OK')

