$debug
-- bisection method for solving non-linear equations

function bisect(f,a,b,fa,fb)
print(n.." a="..a.." fa="..fa.." b="..b.." fb="..fb)
 local c=(a+b)/2
 if abs(a-b)<delta then return c end
 n=n+1
 local fc=f(c)
 if fa*fc<0 then return bisect(f,a,c,fa,fc) else return bisect(f,c,b,fc,fb) end
end

-- find root of f in the inverval [a,b]. bisection needs that f(a)*f(b)<0
function solve(f,a,b)
 delta=1e-6	-- tolerance
 n=0
 local z=bisect(f,a,b,f(a),f(b))
 print(format("after %d steps, root is %.10g",n,z))
end

-- our function
function f(x)
 return x*x*x-x-1
end

solve(f,1,2)
