-- life.lua
-- original by Dave Bollinger <DBollinger@compuserve.com> posted to lua-l
-- modified to use ANSI terminal escape sequences

ALIVE="O"
DEAD="-"

function delay() -- NOTE: SYSTEM-DEPENDENT, adjust as necessary
  local i=10000
  while i>0 do i=i-1 end
  -- local i=clock()+1 while(clock()<i) do end
end

function ARRAY2D(w,h)
  local t = {}
  t.w=w
  t.h=h
  while h>0 do
    t[h] = {}
    local x=w
    while x>0 do
      t[h][x]=0
      x=x-1
    end
    h=h-1
  end
  return t
end

_CELLS = {}

-- give birth to a "shape" within the cell array
function _CELLS:spawn(shape,left,top)
  local y=0
  while y<shape.h do
    local x=0
    while x<shape.w do
      self[top+y][left+x] = shape[y*shape.w+x+1]
      x=x+1
    end
    y=y+1
  end
end

-- run the CA and produce the next generation
function _CELLS:evolve(next)
  local ym1,y,yp1,yi=self.h-1,self.h,1,self.h
  while yi > 0 do
    local xm1,x,xp1,xi=self.w-1,self.w,1,self.w
    while xi > 0 do
      local sum = self[ym1][xm1] + self[ym1][x] + self[ym1][xp1] +
                  self[y][xm1] + self[y][xp1] +
                  self[yp1][xm1] + self[yp1][x] + self[yp1][xp1]
      next[y][x] = ((sum==2) and self[y][x]) or ((sum==3) and 1) or 0
      xm1,x,xp1,xi = x,xp1,xp1+1,xi-1
    end
    ym1,y,yp1,yi = y,yp1,yp1+1,yi-1
  end
end

-- output the array to screen
function _CELLS:draw()
  local out="" -- accumulate to reduce flicker
  local y=1
  while y <= self.h do
    local x=1
    while x <= self.w do
      out=out..(((self[y][x]>0) and ALIVE) or DEAD)
      x=x+1
    end
    out=out.."\n"
    y=y+1
  end
  write(out)
end

-- constructor
function CELLS(w,h)
  local c = ARRAY2D(w,h)
  c.spawn = _CELLS.spawn
  c.evolve = _CELLS.evolve
  c.draw = _CELLS.draw
  return c
end

--
-- shapes suitable for use with spawn() above
--
HEART = { 1,0,1,1,0,1,1,1,1; w=3,h=3 }
GLIDER = { 0,0,1,1,0,1,0,1,1; w=3,h=3 }
EXPLODE = { 0,1,0,1,1,1,1,0,1,0,1,0; w=3,h=4 }
FISH = { 0,1,1,1,1,1,0,0,0,1,0,0,0,0,1,1,0,0,1,0; w=5,h=4 }
BUTTERFLY = { 1,0,0,0,1,0,1,1,1,0,1,0,0,0,1,1,0,1,0,1,1,0,0,0,1; w=5,h=5 }

-- the main routine
function LIFE(w,h)
  -- create two arrays
  local thisgen = CELLS(w,h)
  local nextgen = CELLS(w,h)

  -- create some life
  -- about 1000 generations of fun, then a glider steady-state
  thisgen:spawn(GLIDER,5,4)
  thisgen:spawn(EXPLODE,25,10)
  thisgen:spawn(FISH,4,12)

  -- run until break
  local gen=1
  write("\027[2J")	-- ANSI clear screen
  while 1 do
    thisgen:evolve(nextgen)
    thisgen,nextgen = nextgen,thisgen
    write("\027[H")	-- ANSI home cursor
    thisgen:draw()
    write("Generation: "..gen.."\n")
    gen=gen+1
    --delay()		-- no delay
  end
end

LIFE(40,20)
