-- $Id: heavy.lua,v 1.4 2016/11/07 13:11:28 roberto Exp $
-- See Copyright Notice in file all.lua

print("creating a string too long")
do
  local st, msg = pcall(function ()
    local a = "x"
    while true do
      a = a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       .. a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       .. a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       .. a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       .. a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       .. a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       .. a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       .. a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       .. a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       .. a .. a.. a.. a.. a.. a.. a.. a.. a.. a
       print(string.format("string with %d bytes", #a))
    end
  end)
  assert(not st and
    (string.find(msg, "string length overflow") or
     string.find(msg, "not enough memory")))
end
print('+')


local function loadrep (x, what)
  local p = 1<<20
  local s = string.rep(x, p)
  local count = 0
  local function f()
    count = count + p
    if count % (0x80*p) == 0 then
      io.stderr:write("(", string.format("0x%x", count), ")")
    end
    return s
  end
  local st, msg = load(f, "=big")
  print(string.format("\ntotal: 0x%x %s", count, what))
  return st, msg
end


print("loading chunk with too many lines")
do
  local st, msg = loadrep("\n", "lines")
  assert(not st and string.find(msg, "too many lines"))
end
print('+')


print("loading chunk with huge identifier")
do
  local st, msg = loadrep("a", "chars")
  assert(not st and 
    (string.find(msg, "lexical element too long") or
     string.find(msg, "not enough memory")))
end
print('+')


print("loading chunk with too many instructions")
do
  local st, msg = loadrep("a = 10; ", "instructions")
  print(st, msg)
end
print('+')


print "OK"
