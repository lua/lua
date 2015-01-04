-- very inefficient fibonacci function

function fib(n)
	N=N+1
	if n<2 then
		return n
	else
		return fib(n-1)+fib(n-2)
	end
end

-- a much faster cached version

function cache(f)
	local c={}
	return function (x)
		local y=%c[x]
		if not y then
			y=%f(x)
			%c[x]=y
		end
		return y
	end
end

function test(s)
	N=0
	local c=clock()
	local v=fib(n)
	local t=clock()-c
	print(s,n,v,t,N)
end

n=n or 24		-- for other values, do lua -e n=XX fib.lua
n=tonumber(n)
print("","n","value","time","evals")
test("plain")
fib=cache(fib)
test("cached")
