-- catch "undefined" global variables. see FAQ.

do
 local f=function(name)
  local v=rawget(globals(),name)
  if v then
   return v
  else
   error("undefined global variable `"..name.."'")
  end
 end

 settagmethod(tag(nil),"getglobal",f)
end

-- an example

a=1
c=3
print(a,b,c)	-- 'b' is undefined
