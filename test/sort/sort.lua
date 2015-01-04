$debug
function quicksort(r,s)
	if s<=r then return end		-- caso basico da recursao
	local v=x[r]
	local i=r
	local j=s+1
		i=i+1; while x[i]<v do i=i+1 end
		j=j-1; while x[j]>v do j=j-1 end
		x[i],x[j]=x[j],x[i]
	while j>i do			-- separacao
		i=i+1; while x[i]<v do i=i+1 end
		j=j+1; while x[j]>v do j=j-1 end
		x[i],x[j]=x[j],x[i]
	end
	x[i],x[j]=x[j],x[i]		-- undo last swap
	x[j],x[r]=x[r],x[j]
	quicksort(r,j-1)		-- recursao
	quicksort(j+1,s)
end

function sort(a,n)			-- selection sort
 local i=1
 while i<=n do
  local m=i
  local j=i+1
  while j<=n do
   if a[j]<a[m] then m=j end
   j=j+1
  end
 a[i],a[m]=a[m],a[i]			-- swap a[i] and a[m]
 print (i, a[i])
 i=i+1
 end
end

x=@(20)
n=-1
n=n+1;	x[n]="a"
n=n+1;	x[n]="waldemar"
n=n+1;	x[n]="luiz"
n=n+1;	x[n]="lula"
n=n+1;	x[n]="peter"
n=n+1;	x[n]="raquel"
n=n+1;	x[n]="camilo"
n=n+1;	x[n]="andre"
n=n+1;	x[n]="marcelo"
n=n+1;	x[n]="sedrez"
n=n+1;	x[n]="z"
-- sort(x,n)
quicksort(1,n-1)
print(x[0]..","..x[1]..","..x[2]..","..x[3]..","..x[4]..","..x[5]..","..x[6]..","..x[7]..","..x[8]..","..x[9]..","..x[10])
