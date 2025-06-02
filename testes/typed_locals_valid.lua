-- Valid typed local declarations
local num n1 = 10
local num n2 = 3.14
local num n3 = n1 + 5 -- Assignment of complex expr, should pass (no check yet)
local num n4 = nil    -- Valid: nil assignment

local str s1 = "hello"
local str s2 = "world" .. "!" -- Assignment of complex expr, should pass
local str s3 = nil           -- Valid: nil assignment

local bool b1 = true
local bool b2 = false
local bool b3 = not b1  -- Assignment of complex expr, should pass
local bool b4 = nil     -- Valid: nil assignment

-- Check that these variables can be used (e.g., print them)
-- This mainly tests parsing. Runtime behavior is standard Lua.
print(n1, n2, n3, n4)
print(s1, s2, s3)
print(b1, b2, b3, b4)

local num n5
print(n5) -- Should be nil

local str s5
print(s5) -- Should be nil

local bool b5
print(b5) -- Should be nil

print("typed_locals_valid.lua: All valid declarations processed.")
