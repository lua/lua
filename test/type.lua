$debug

function check (object, class)
 local v = next(object,nil);
 while v ~= nil do
   if class[v] == nil then
    print("unknown field: " .. v) 
   elseif type(object[v]) ~= class[v].type then
    print("wrong type for field " .. v)
   end
   v = next(object,v);
 end
 v = next(class,nil);
 while v ~= nil do
   if object[v] == nil then
     if class[v].default ~= nil then
      object[v] = class[v].default
     else
      print("field "..v.." not initialized")
     end
   end
   v = next(class,v);
 end
end

typeblock = {x = {default = 0, type = "number"},
               y = {default = 0, type = "number"},
               name = {type = "string"}
              }

function block(t) check(t,typeblock) end

block{ x = 7, name = "3"}
block{ x = "7", name = "3"}
block{ x = 7, name = 3}
block{ x = 7}
block{ x = 7, name = "3", bogus=3.14}
