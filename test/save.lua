-- dump global environment

function savevar (n,v)
 if v == nil then return end
 if type(v)=="userdata" or type(v)=="function" then return end
 -- if type(v)=="userdata" or type(v)=="function" then write("\t-- ") end
 write(n,"=")
 if type(v) == "string" then write(format("%q",v))
 elseif type(v) == "table" then
   if v.__visited__ ~= nil then
     write(v.__visited__)
   else
    write("{}\n")
    v.__visited__ = n
    local r,f
    r,f = next(v,nil)
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
 else write(tostring(v)) end
 write("\n")
end

function save ()
 write("\n-- global environment\n")
 local n,v = nextvar(nil)
  while n ~= nil do 
    savevar(n,v) 
    n,v = nextvar(n)
  end
end

-- ow some examples

a = 3
x = {a = 4, b = "name", l={4,5,67}}

b = {t=5}
x.next = b

save()
