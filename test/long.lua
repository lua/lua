function sort(a,n)                      -- selection sort
 local i=1
 while i<=n do
  local m, j = i, i+1
  while j<=n do
   if a[j]<a[m] then m=j end
   j=j+1
  end
  a[i],a[m]=a[m],a[i]                    -- swap a[i] and a[m]
  i=i+1
 end
end


v = { }

i=1
while i <= 5000 do v[i] = 5000-i i=i+1 end

sort(v,5000)

print("v512 = ".. v[512])
