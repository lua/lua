$debug

function check (object, class)
 local v = next(object,nil);
 while v ~= nil do
   if class[v] = nil then print("unknown field: " .. v) 
   elseif type(object[v]) ~= class[v].type  
     then print("wrong type for field " .. v)
   end
   v = next(object,v);
 end
 v = next(class,nil);
 while v ~= nil do
   if object[v] = nil then
     if class[v].default ~= nil then
       object[v] = class[v].default
     else print("field "..v.." not initialized")
     end
   end
   v = next(class,v);
 end
end

typetrilha = @{x = @{default = 0, type = "number"},
               y = @{default = 0, type = "number"},
               name = @{type = "string"}
              }

function trilha (t)  check(t,typetrilha) end

t1 = @trilha{ x = 4, name = "3"}

a = "na".."me"

