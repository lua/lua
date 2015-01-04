-- shows how to trace assigments to global variables

T=newtag()				-- tag for tracing

function Ttrace(name)			-- trace a global variable
  local t={}
  settag(t,T)
  rawsetglobal(name,t)
end

function Tsetglobal(name,old,new)
  write("tracing: ",name," now is ",new,"\n")
  old.value=new 
end

function Tgetglobal(x,value)		-- get the actual value
  return value.value
end

settagmethod(T,"getglobal",Tgetglobal)
settagmethod(T,"setglobal",Tsetglobal)

-- now show it working

Ttrace("a")
Ttrace("c")

a=1
b=2
c=3
a=10
b=20
c=30
