dofile("dump.lua")

a = 3
x = {a = 4, b = "name", l={4,5,67}}

b = {t=5}
x.next = b


save()

