-- shows how to trace assigments to global variables

-- a tostring that quotes strings. note the use of the original tostring.
local tostring=function(a)
 if tag(a)==tag("") then
  return format("%q",a)
 else
  return %tostring(a)
 end
end

local T=newtag()

local Tlog=function (name,old,new)
 local t=getinfo(3,"Sl")
 local line=t.currentline
 write(t.source)
 if line>=0 then write(":",line) end
 write(" -- ",name," is now ",%tostring(new)," (was ",%tostring(old),")","\n")
end

local Tgetnew=function (name)
 local t=settag({},%T)
 rawset(globals(),name,t)
 return nil
end

local Tsetnew=function (name,old,new)
 %Tlog(name,old,new)
 local t=settag({value=new},%T)
 rawset(globals(),name,t)
 return t
end

local Tsetglobal=function (name,old,new)
 %Tlog(name,old.value,new)
 old.value=new 
end

local Tgetglobal=function (x,value)
 return value.value
end

settagmethod(T,"getglobal",Tgetglobal)
settagmethod(T,"setglobal",Tsetglobal)

-- to trace only selected variables, use the following function
-- and comment the next two calls to settagmethod

function trace(name)
 local t=settag({value=rawget(globals(),name)},%T)
 rawset(globals(),name,t)
end
 
settagmethod(tag(nil),"getglobal",Tgetnew)
settagmethod(tag(nil),"setglobal",Tsetnew)

-- an example

trace"a"
print(a)
a=1
b=2
c=3
a=10
b=20
c=30
trace"b"
b="lua"
c={}
a=print
c=nil
c=100

print(a,b,c,d)
