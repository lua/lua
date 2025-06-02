-- Variable assignment errors
local num n_err1 = "not a number" -- E: assign: expected num, got str
local str s_err1 = 123           -- E: assign: expected str, got num
local bool b_err1 = "true"       -- E: assign: expected bool, got str

local num n_decl
n_decl = "oops"                  -- E: assign: expected num, got str

print("Variable assignment errors tested - THIS SHOULD NOT BE REACHED IF ERRORS ARE FATAL.")

-- Function return type errors
local fn -> num ret_err_str():
    return "should be num"       -- E: return: expected num, got str
end

local fn -> str ret_err_num():
    return 123                   -- E: return: expected str, got num
end

local fn -> void ret_err_val():
    return 100                   -- E: return: void function returns value (unless it's nil)
end

local fn -> num ret_err_void():
    return                       -- E: return: function expected num, returns nothing
end

print("Function return type errors tested - THIS SHOULD NOT BE REACHED IF ERRORS ARE FATAL.")

-- Function argument type errors
local fn -> void arg_expect_num(num x):
    print(x)
end

arg_expect_num("not a num")      -- E: argument: expected num, got str

local fn -> void arg_expect_str_bool(str s, bool b):
    print(s, b)
end

arg_expect_str_bool(true, "false") -- E: argument 1: expected str, got bool
                                   -- E: argument 2: expected bool, got str (or errors one by one)

print("Function argument type errors tested - THIS SHOULD NOT BE REACHED IF ERRORS ARE FATAL.")
