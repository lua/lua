print("typed_locals_invalid_bool_typecheck.lua: Testing bool error")
-- Expected error: type mismatch: variable 'b1' declared as type 'bool' but assigned a literal of type 'num'
local bool b1 = 0
print("FAIL: Should not have reached here if 'local bool b1 = 0' failed as expected.")
