print("typed_locals_invalid_str_typecheck.lua: Testing str error")
-- Expected error: type mismatch: variable 's1' declared as type 'str' but assigned a literal of type 'num'
local str s1 = 12345
print("FAIL: Should not have reached here if 'local str s1 = 12345' failed as expected.")
