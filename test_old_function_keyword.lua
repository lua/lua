-- This should cause a syntax error
local function old_style_func(a, b):
    return a - b
end
print("Old function keyword test parsed - THIS SHOULD NOT HAPPEN.")
