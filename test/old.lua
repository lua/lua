-- implementation of old functions

function foreach(t,f)
 for i,v in t do
  local r=f(i,v)
  if r then return r end
 end
end

function foreachi(t,f)
 for i=1,getn(t) do
  local r=f(i,t[i])
  if r then return r end
 end
end

function foreachvar(f)
 return foreach(globals(),f)
end

function nextvar(n)
 return next(globals(),n)
end

function rawgetglobal(n)
 return rawget(globals(),n)
end

function rawsetglobal(n,v)
 return rawset(globals(),n,v)
end

rawsettable=rawset
rawgettable=rawget

function getglobal(n)
 return globals()[n]
end

function setglobal(n,v)
 globals()[n]=v
end
