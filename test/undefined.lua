-- catch "undefined" global variables

local f=function (t,i) error("undefined global variable `"..i.."'",2) end
setmetatable(getfenv(),{__index=f})

-- an example
a=1
c=3
print(a,b,c)	-- `b' is undefined
