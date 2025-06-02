local num global_num = 100

local fn -> num get_global_plus(num x):
    return global_num + x
end

local str message = "The result is: "
local num result = get_global_plus(50)

print(message .. result) -- Should be "The result is: 150"
