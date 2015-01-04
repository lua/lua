local A
while 1 do
 local a,b=read("*w","*w")
 if a==nil then break end
 if a~=A then A=a write("\n",a,":\n") end
 write(b," ")
end
