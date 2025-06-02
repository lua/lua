-- Correct variable assignments
local num n1 = 10
local str s1 = "hello"
local bool b1 = true
local num n2 = n1 + 5
local str s2 = s1 .. " world"
local bool b2 = not b1
local num n3 = nil -- Allowed
local str s3 = nil -- Allowed

print("Correct assignments parsed.")

-- Correct function definitions and calls
local fn -> num add(num a, num b):
    return a + b
end

-- Function with string and boolean types
local fn -> str concat_nil(str s, nil n): -- Assuming DEC_NIL can be a param type
    if n == nil then
        return s .. "_nil"
    end
    return s
end

local fn -> void print_num(num val):
    print(val)
    return -- or return nil
end

local num r1 = add(5, 3)
print_num(r1)
print_num(nil) -- Allowed if param 'val' allows nil

local str r2 = concat_nil("text", nil)
print(r2)

-- Function returning nil for a typed return
local fn -> num get_nil_num():
    return nil
end
local num n4 = get_nil_num()
print(n4) -- Should print nil

print("Correct functions parsed and called.")
