-- Invalid: missing colon
local fn -> num bad_func_no_colon(num a)
    return a
end
print("Invalid typed func (no colon) test parsed - THIS SHOULD NOT HAPPEN.")
