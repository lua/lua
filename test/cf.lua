-- temperature conversion table

-- celsius to farenheit

c0=-20
while c0<50 do

	c1=c0+10
	write("C ")
	c=c0
	while c<c1 do
		write(format("%3.0f ",c))
		c=c+1
	end
	write("\n")
	
	c=c0
	write("F ")
	while c<c1 do
		f=(9/5)*c+32
		write(format("%3.0f ",f))
		c=c+1
	end
	write("\n\n")

	c0=c1
end
