-- complex.lua
-- complex arithmetic package for lua
-- Luiz Henrique de Figueiredo (lhf@csg.uwaterloo.ca)
-- 24 Oct 95
$debug

Complex={type="package"}

function complex(x,y)
 return { re=x, im=y, type="complex" }
end

function Complex.conj(x,y)
 return complex(x.re,-x.im)
end

function Complex.norm2(x)
 local n=Complex.mul(x,Complex.conj(x))
 return n.re
end

function Complex.abs(x)
 return sqrt(Complex.norm2(x))
end

function Complex.add(x,y)
 return complex(x.re+y.re,x.im+y.im)
end

function Complex.sub(x,y)
 return complex(x.re-y.re,x.im-y.im)
end

function Complex.mul(x,y)
 return complex(x.re*y.re-x.im*y.im,x.re*y.im+x.im*y.re)
end

function Complex.div(x,y)
 local z=x*Complex.conj(y)
 local t=Complex.norm2(y)
 z.re=z.re/t
 z.im=z.im/t
 return z
end

function Complex.fallback(x,y,op)
 if type(x)=="number" then x=complex(x,0) end
 if type(y)=="number" then y=complex(y,0) end
 if type(x)=="complex" and type(y)=="complex" then
  return Complex[op](x,y)
 else
  return Complex.oldfallback(x,y)
 end
end

function Complex.newtype(x)
 local t=Complex.oldtype(x)
 if t=="table" and x.type then return x.type else return t end
end

function Complex.print(x)
 if type(x)~="complex" then
  Complex.oldprint(x)
 else
  local s
  if x.im>=0 then s="+" else s="" end
  Complex.oldprint(x.re..s..x.im.."I")
 end
end

function Complex.newabs(x)
 if type(x)~="complex" then
  return Complex.oldabs(x)
 else
  return Complex.abs(x)
 end
end

Complex.oldfallback=setfallback("arith",Complex.fallback)
Complex.oldtype=type	type=Complex.newtype
Complex.oldprint=print	print=Complex.print
Complex.oldabs=abs	abs=Complex.abs
