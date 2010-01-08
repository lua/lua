function run(x)
	print("\n-------------------------------------------------------------")
	print(x)
	os.execute("../src/lua "..x)
	print("DONE",x)
end

run"bisect.lua"
run"cf.lua"
run"echo.lua jan feb mar apr may jun jul aug sep oct nov dec"
run"env.lua"
run"factorial.lua"
run"fib.lua"
run"fibfor.lua"
--run"globals.lua"
run"hello.lua"
run"luac.lua"
run"printf.lua"
run"readonly.lua"
run"sieve.lua"
run"sort.lua"
run"table.lua"
run"trace-calls.lua"
run"trace-globals.lua"
run"xd.lua < hello.lua"
run"life.lua"
