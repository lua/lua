

function savevar (n,v)
 if v == nil then return end;
 if type(v) == "number" then print(n.."="..v) return end
 if type(v) == "string" then print(n.."='"..v.."'") return end
 if type(v) == "table" then
   if v.__visited__ ~= nil then
     print(n .. "=" .. v.__visited__);
   else
    print(n.."={}")
    v.__visited__ = n;
    local r,f;
    r,f = next(v,nil);
    while r ~= nil do
      if r ~= "__visited__" then
        if type(r) == 'string' then
          savevar(n.."."..r,f)
	else
          savevar(n.."["..r.."]",f)
	end
      end
      r,f = next(v,r)
    end
   end
 end
end

function save ()
 local n,v = nextvar(nil)
  while n ~= nil do 
    savevar(n,v); 
    n,v = nextvar(n)
  end
end

save()
