--------------------------------------------------------------
-- Definition of "setfallback" using tag methods
-- (for compatibility with old code)
--------------------------------------------------------------


-- default fallbacks for each event:
local defaults = {
  gettable = function () error('indexed expression not a table') end,
  settable = function () error('indexed expression not a table') end,
  index = function () return nil end,
  getglobal = function () return nil end,
  arith = function () error('number expected in arithmetic operation') end,
  order = function () error('incompatible types in comparison') end,
  concat = function () error('string expected in concatenation') end,
  gc = function () return nil end,
  ['function'] = function () error('called expression not a function') end,
  error = function (s) write(_STDERR, s, '\n') end,
}


function setfallback (name, func)

  -- set the given function as the tag method for all "standard" tags
  -- (since some combinations may cause errors, use call to avoid messages)
  local fillvalids = function (n, func)
    call(settagmethod, {0, n, func}, 'x', nil)
    call(settagmethod, {tag(0), n, func}, 'x', nil)
    call(settagmethod, {tag(''), n, func}, 'x', nil)
    call(settagmethod, {tag{}, n, func}, 'x', nil)
    call(settagmethod, {tag(function () end), n, func}, 'x', nil)
    call(settagmethod, {tag(settagmethod), n, func}, 'x', nil)
    call(settagmethod, {tag(nil), n, func}, 'x', nil)
  end

  assert(type(func) == 'function')
  local oldfunc
  if name == 'error' then
    oldfunc = seterrormethod(func)
  elseif name == 'getglobal' then
    oldfunc = settagmethod(tag(nil), 'getglobal', func)
  elseif name == 'arith' then
    oldfunc = gettagmethod(tag(0), 'pow')
    foreach({"add", "sub", "mul", "div", "unm", "pow"},
            function(_, n) %fillvalids(n, %func) end)
  elseif name == 'order' then
    oldfunc = gettagmethod(tag(nil), 'lt')
    foreach({"lt", "gt", "le", "ge"},
            function(_, n) %fillvalids(n, %func) end)
  else
    oldfunc = gettagmethod(tag(nil), name)
    fillvalids(name, func)
  end
  return oldfunc or rawgettable(%defaults, name)
end
