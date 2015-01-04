$debug

a = {}

i=0
while i<10 do
 a[i] = i*i
 i=i+1
end

r,v = next(a,nil)
while r ~= nil do
 print ("array["..r.."] = "..v)
 r,v = next(a,r)
end 
