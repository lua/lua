-- reads the output of luac -d -l -p and reports global variable usage
-- lines where a global is written to are marked with "*"
-- typical usage: luac -p -l file.lua | lua globals.lua | sort

local P="^.*; "		-- pattern to extract comments

while 1 do
	local s=read()
	if s==nil then return end
	if strfind(s,"%sGETGLOBAL") then
		local g=gsub(s,P,"")
		local _,_,l=strfind(s,"(%d+)")
		write(g,"\t",l,"\n")
	elseif strfind(s,"%sSETGLOBAL") then
		local g=gsub(s,P,"")
		local _,_,l=strfind(s,"(%d+)")
		write(g,"\t",l,"*\n")
	end
end
