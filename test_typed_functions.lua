-- Valid typed function definition
local fn -> num add(num a, num b):
    return a + b
end

-- Function with string and boolean types
local fn -> str concat(str s1, str s2):
    return s1 .. s2
end

local fn -> bool is_equal(num x, num y):
    return x == y
end

-- Function with mixed typed and untyped parameters
local fn -> num process(num val, untyped_param):
    if untyped_param == "double" then -- Changed colon to then
        return val * 2
    end
    return val
end

-- Function with no parameters, explicit return type
local fn -> str get_hello():
    return "hello"
end

-- Anonymous function (should default to untyped return)
local my_anon_fn = fn(): print("anon") end

-- Global function (should default to untyped return)
fn global_printer(str message):
    print(message)
end

print("Typed function test parsed.")
