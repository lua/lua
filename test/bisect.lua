-- bisection method for solving non-linear equations

function bisect(f,a,b,fa,fb)
 write(n," a=",a," fa=",fa," b=",b," fb=",fb,"\n")
 local c=(a+b)/2
 if c==a or c==b then return c end
 if abs(a-b)<delta then return c end
 n=n+1
 local fc=f(c)
 if fa*fc<0 then return bisect(f,a,c,fa,fc) else return bisect(f,c,b,fc,fb) end
end

-- find root of f in the inverval [a,b]. bisection needs that f(a)*f(b)<0
function solve(f,a,b)
 delta=1e-9	-- tolerance
 n=0
 local z=bisect(f,a,b,f(a),f(b))
 write(format("after %d steps, root is %.10g, f=%g\n",n,z,f(z)))
end

-- an example

-- our function
function f(x)
 return x*x*x-x-1
end

-- find zero in [1,2]
solve(f,1,2)
