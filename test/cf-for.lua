-- temperature conversion table
-- now using the new "for" statement

-- celsius to farenheit

for c0=-20,50-1,10 do
	write("C ")
	for c=c0,c0+10-1 do
		write(format("%3.0f ",c))
	end
	write("\n")
	
	write("F ")
	for c=c0,c0+10-1 do
		f=(9/5)*c+32
		write(format("%3.0f ",f))
	end
	write("\n\n")
end
