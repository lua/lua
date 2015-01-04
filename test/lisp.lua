-- a simple LISP evaluator

function eval(x)
	if type(x)=="table" then
		return eval(x[1])(eval(x[2]),eval(x[3]))
	else
		return x
	end
end

function add(x,y) return x+y end
function sub(x,y) return x-y end
function mul(x,y) return x*y end
function div(x,y) return x/y end
function pow(x,y) return x^y end

-- an example

function E(x) print(eval(x)) end

E{add,1,{mul,2,3}}
E{sin,60}
