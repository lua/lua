-- Valid typed variable declarations
local num a = 10
local str s = "hello"
local bool b = true
local num f = 1.23

-- Valid typed variable declarations with assignments
local num x = a + 20
local str greeting = s .. " world"
local bool flag = not b

-- Untyped should still work
local untyped_var = a
local another_untyped = "some string"

print("Typed variable test parsed.")
