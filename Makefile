OBJS= hash.o inout.o lex_yy.o opcode.o table.o y_tab.o lua.o iolib.o mathlib.o strlib.o

CFLAGS= -O2 -I.

T= lua

all:	$T

$T:	$(OBJS)
	$(CC) -o $@ $(OBJS) -lm

A=--------------------------------------------------------------------------
test:	$T
	@echo "$A"
	./$T sort.lua main
	@echo "$A"
	./$T globals.lua | sort | column
	@echo "$A"
	./$T array.lua
	@echo "$A"
	./$T save.lua
	@echo "$A"
	./$T test.lua retorno_multiplo norma

clean:
	rm -f $T $(OBJS) core core.*

diff:
	diff . fixed | grep -v ^Only
