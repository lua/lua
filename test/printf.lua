-- an implementation of printf

function printf(...)
 io.write(string.format(unpack(arg)))
end

printf("Hello %s from %s on %s\n",os.getenv"USER" or "there",_VERSION,os.date())
