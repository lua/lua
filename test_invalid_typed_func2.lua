-- Invalid: missing arrow
local fn num bad_func_no_arrow(num a):
    return a
end
print("Invalid typed func (no arrow) test parsed - THIS SHOULD NOT HAPPEN.")
