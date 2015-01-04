-- ps.lua
-- lua interface to postscript
-- Luiz Henrique de Figueiredo (lhf@csg.uwaterloo.ca)
-- 12 Nov 95

PS={}

function P(x)
 write(x.."\n")
end

-------------------------------------------------------------------- control --

function PS.open(title)
 local d,m,y=date()
 local H,M,S=time()
 if d<10 then d="0"..d end
 if m<10 then m="0"..m end
 if title==nil then title="(no title)" end
 P("%!PS-Adobe-2.0 EPSF-1.2")
 P("%%Title: "..title)
 P("%%Creator: ps.lua from Lua 2.2")
 P("%%CreationDate: "..y..m..d.." "..H..":"..M..":"..S)
 P("%%Pages: (atend)")
 P("%%BoundingBox: (atend)")
 P("%%EndComments")
 P("%%BeginProcSet: ps.lua")
 P("/s { stroke } bind def")
 P("/f { fill } bind def")
 P("/m { moveto } bind def")
 P("/l { lineto } bind def")
 P("/L { moveto lineto stroke } bind def")
 P("/t { show } bind def")
 P("/o { 0 360 arc stroke } bind def")
 P("/O { 0 360 arc fill } bind def")
 P("/p { 3 0 360 arc fil } bind def")
 P("/F { findfont exch scalefont setfont } bind def")
 P("/LS { 0 setdash } bind def")
 P("/LW { setlinewidth } bind def")
 P("%%EndProcSet: ps.lua")
 P("%%EndProlog")
 P("%%BeginSetup")
 P("0 setlinewidth")
 P("1 setlinejoin")
 P("1 setlinecap")
 P("10 /Times-Roman F")
 P("%%EndSetup\n")
 P("%%Page: 1 1")
-- cxmin=dv.xmin; cxmax=dv.xmax; cymin=dv.ymin; cymax=dv.ymax
 xmin=1000; xmax=-1000; ymin=1000; ymax=-1000
 page=1
end

function PS.close()
 P("stroke")
 P("showpage")
 P("%%Trailer")
 P("%%Pages: "..page)
 P("%%BoundingBox: "..xmin.." "..ymin.." "..xmax.." "..ymax)
 P("%%EOF")
end

function PS.clear()
 if (empty) then return end
 page=page+1
 P("showpage")
 P("%%Page: "..page.." "..page)
 empty=1
end

function PS.comment(s)
 P("% "..s)
end

--------------------------------------------------------------- direct color --

function PS.rgbcolor(r,g,b)
 P(r.." "..g.." "..b.." setrgbcolor")
end

function PS.gray(g)
 P(g.." setgray")
end

---------------------------------------------------------------- named color --

function PS.color(c)
 P("C"..c)
end

function PS.defrgbcolor(c,r,g,b)
 P("/C"..c.." { "..r.." "..g.." "..b.." setrgbcolor } def")
end

function PS.defgraycolor(c,g)
 P("/C"..c.." { "..g.." setgray } def")
end

----------------------------------------------------------------------- line --

function PS.line(x1,y1,x2,y2)
 P(x2.." "..y2.." "..x1.." "..y1.." L")
 PS.update(x1,y1)
 PS.update(x2,y2)
end

function PS.moveto(x,y)
 P(x.." "..y.." m")
 PS.update(x,y)
end

function PS.lineto(x,y)
 P(x.." "..y.." l")
 PS.update(x,y)
end

function PS.linewidth(w)
 P(w.." LW")
end

function PS.linestyle(s)
 P("["..s.."] LS")
end

----------------------------------------------------------------------- text --

function PS.font(name,size)
 if (size==nil) then size=10 end
 P(size.." /"..name.." F")
end

function PS.text(x,y,s)
 P(x.." "..y.."m ("..s..") t")
 PS.update(x,y)
end

--------------------------------------------------------------------- circle --

function PS.circle(x,y,r)
 P(x.." "..y.." "..r.." o")
 PS.update(x-r,y-r)
 PS.update(x+r,y+r)
end

function PS.disk(x,y,r)
 P(x.." "..y.." "..r.." O")
 PS.update(x-r,y-r)
 PS.update(x+r,y+r)
end

function PS.dot(x,y)
 P(x.." "..y.." p")
 PS.update(x-r,y-r)
 PS.update(x+r,y+r)
end

------------------------------------------------------------------------ box --

function PS.rectangle(xmin,xmax,ymin,ymax)
 P(xmin.." "..ymin.." m "..
   xmax.." "..ymin.." l "..
   xmax.." "..ymax.." l "..
   xmin.." "..ymax.." l s")
 PS.update(xmin,ymin)
 PS.update(xmax,ymax)
end

function PS.box(xmin,xmax,ymin,ymax)
 P(xmin.." "..ymin.." m "..
   xmax.." "..ymin.." l "..
   xmax.." "..ymax.." l "..
   xmin.." "..ymax.." l f")
 PS.update(xmin,ymin)
 PS.update(xmax,ymax)
end

-------------------------------------------------------- update bounding box --

function PS.update(x,y)
-- if (x>=cxmin and x<=cxmax and y>=cxmin and y<=cxmax) then
  if (x<xmin) then xmin=x elseif (x>xmax) then xmax=x end
  if (y<ymin) then ymin=y elseif (y>ymax) then ymax=y end
  empty=0
-- end
end
