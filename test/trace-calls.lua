-- trace calls
-- example: lua trace-calls.lua hello.lua bisect.lua

function callhook(func)
 local t=getinfo(2)
 write(">>> ")
--foreach(t,print)
 if t.what=="main" then
  if func=="call" then
   write("begin ",t.source)
  else
   write("end ",t.source)
  end
 elseif t.what=="Lua" then
  write(func," ",t.name," <",t.linedefined,":",t.source,">")
 else
  write(func," ",t.name," [",t.what,"] ")
 end
 if t.currentline>=0 then write(":",t.currentline) end
 write("\n")
end

setcallhook(callhook)
