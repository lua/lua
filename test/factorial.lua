-- function closures are powerful

-- traditional fixed-point operator from functional programming

Y = function (g)
      local a = function (f) return f(f) end
      return a(function (f)
                 return %g(function (x)
                             local c=%f(%f)
                             return c(x)
                           end)
               end)
end


-- factorial without recursion

F = function (f)
      return function (n)
               if n == 0 then return 1
               else return n*%f(n-1) end
             end
    end

factorial = Y(F)   -- factorial is the fixed point of F

-- now test it

function test(x)
	write(x,"! = ",factorial(x),"\n")
end

test(3)
test(4)
test(5)
test(6)
test(7)
