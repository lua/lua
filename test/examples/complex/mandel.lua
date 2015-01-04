dofile("complex.lua")

xmin=-2		xmax=2		ymin=-2		ymax=2
d=.125

function level(x,y)
 local c=complex(x,y)
 local l=0
 local z=c
 repeat
  z=z*z+c
  l=l+1
 until abs(z)>2 or l>255
 return l-1
end

x=xmin
while x<xmax do
 y=ymin
 while y<ymax do
  print(level(x,y))
  y=y+d
 end
 x=x+d
end
