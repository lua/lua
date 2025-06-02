print("typed_locals_invalid_typecheck.lua: Testing num error")
-- Expected error: type mismatch: variable 'n1' declared as type 'num' but assigned a literal of type 'str'
local num n1 = "this should fail" 
print("FAIL: Should not have reached here if 'local num n1 = "string"' failed as expected.")
