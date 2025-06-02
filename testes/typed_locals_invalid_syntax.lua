print("typed_locals_invalid_syntax.lua: Testing comma syntax error")
-- Expected error: comma-separated typed local declarations not supported
local num n1 = 10, num n2 = 20 
print("FAIL: Should not have reached here if comma-separated typed locals syntax error was caught.")
