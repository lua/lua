-- hex dump
-- usage: lua xd.lua < file

local N=16
local offset=0
while true do
 local s=io.read(N)
 if s==nil then return end
 io.write(string.format("%08X  ",offset))
 string.gsub(s,"(.)",
	function (c) io.write(string.format("%02X ",string.byte(c))) end)
 io.write(string.rep(" ",3*(N-string.len(s))))
 io.write(" ",string.gsub(s,"%c","."),"\n") 
 offset=offset+N
end
