function split (s)
 local n = 1
 local f = strfind(s,"/")
 while f do
  n = n+f
  f = strfind(strsub(s,n),"/")
 end
 return strsub(s,1,n-1), strsub(s,n)
end


function test(s)
 local path, filename = split(s)
 print(s .. "=[".. path.."]+["..filename.."]")
end

test("a:/lua/obj/lua.c")
test("lua.lib")
