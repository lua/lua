-- hilbert.c
-- Hilbert curve
-- Luiz Henrique de Figueiredo (lhf@csg.uwaterloo.ca)
-- 10 Nov 95

$debug

dofile("ps.lua")

function p()
 PS.lineto(x,y)
end

function a(n)
 if (n==0) then return else n=n-1 end
 d(n); y=y+h; p(); a(n); x=x+h; p(); a(n); y=y-h; p(); b(n);
end

function b(n)
 if (n==0) then return else n=n-1 end
 c(n); x=x-h; p(); b(n); y=y-h; p(); b(n); x=x+h; p(); a(n);
end

function c(n)
 if (n==0) then return else n=n-1 end
 b(n); y=y-h; p(); c(n); x=x-h; p(); c(n); y=y+h; p(); d(n);
end

function d(n)
 if (n==0) then return else n=n-1 end
 a(n); x=x+h; p(); d(n); y=y+h; p(); d(n); x=x-h; p(); c(n);
end

function hilbert(n)
 PS.open("hilbert curve")
 h=2^(9-n)
 x=0
 y=0
 PS.moveto(x,y)
 a(n)
 PS.close()
end

hilbert(5)
