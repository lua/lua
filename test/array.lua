$debug

a = {}

local i=0
while i<10 do
 a[i] = i*i
 i=i+1
end

local r,v = next(a,nil)
while r ~= nil do
 write ("array[",r,"] = ",v,"\n")
 r,v = next(a,r)
end 
