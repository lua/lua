$debug

function quicksort(a,r,s)
 if s<=r then return end
 local v, i, j = a[r], r, s+1
 repeat
  i=i+1; while a[i]<v do i=i+1 end
  j=j-1; while a[j]>v do j=j-1 end
  a[i],a[j]=a[j],a[i]
 until j<=i
 a[i],a[j]=a[j],a[i]			-- undo last swap
 a[j],a[r]=a[r],a[j]
 quicksort(a,r,j-1)
 quicksort(a,j+1,s)
end

function selectionsort(a,n)
 local i=1
 while i<=n do
  local m, j = i, i+1
  while j<=n do
   if a[j]>a[m] then m=j end		-- reverse sort
   j=j+1
  end
 a[i],a[m]=a[m],a[i]			-- swap a[i] and a[m]
 i=i+1
 end
end

function show(m,x)
 write(m,"\n\t")
 local i=0
 while x[i] do
  write(x[i])
  i=i+1
  if x[i] then write(",") end
 end
 write("\n")
end

x={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"}

n=1	while x[n] do n=n+1 end		-- count elements
x[0]="A"	x[n]="Z"		-- quicksort need sentinels

show("original",x)

quicksort(x,1,n-1)
show("after quicksort",x)

selectionsort(x, n-1)
show("after reverse selection sort",x)
