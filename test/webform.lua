-- convert POST data to Lua table

T=read"*a"	-- for GET, use T=getenv"QUERY_STRING"
T=gsub(T,"=","=[[")
T=gsub(T,"&","]],\n")
T=gsub(T,"+"," ")
T=gsub(T,"%%(%x%x)",function (x) return strchar(tonumber(x,16)) end)
write("form{\n",T,"]]}\n")
