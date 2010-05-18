# makefile for Lua etc

TOP= ..
LIB= $(TOP)/src
INC= $(TOP)/src
BIN= $(TOP)/src
SRC= $(TOP)/src
TST= $(TOP)/test

T= test
CC= gcc
CFLAGS= -O2 -Wall -I$(INC) $(MYCFLAGS)
MYCFLAGS=
MYLIBS= -lm # -ldl -lreadline -lncurses -Wl,-E
RM= rm -f


default:
	@echo 'Please choose a target: min noparser one one-lua one-lib one-luac so strict clean'

min:	min.c
	$(CC) $(CFLAGS) -o $T min.c -L$(LIB) -llua $(MYLIBS)
	echo 'print"Hello there!"' | ./$T
	./$T $(TST)/hello.lua
	echo 'print("Hello world, from",_VERSION,"!")' | ./$T

noparser: noparser.o
	$(CC) -o $T noparser.o $(SRC)/lua.o -L$(LIB) -llua $(MYLIBS)
	$(BIN)/luac $(TST)/hello.lua
	-./$T luac.out
	-./$T -e'a=1'

one:	one-lua one-lib one-luac

one-lua:
	$(CC) $(CFLAGS) -o $T one.c $(MYLIBS)
	./$T $(TST)/hello.lua

one-lib:
	$(CC) $(CFLAGS) -DMAKE_LIB -o liblua.o -c one.c
	$(CC) $(CFLAGS) -o $T min.c liblua.o $(MYLIBS)
	./$T $(TST)/hello.lua

one-luac:
	$(CC) $(CFLAGS) -o $T -DMAKE_LUAC one.c $(MYLIBS)
	./$T -l $(TST)/hello.lua

so:
	$(CC) $(CFLAGS) -shared -o dummy.so dummy.c
	@#env MACOSX_DEPLOYMENT_TARGET=10.3 $(CC) $(CFLAGS) -bundle -undefined dynamic_lookup -o dummy.so dummy.c
	$(BIN)/lua -v -ldummy

strict:
	-$(BIN)/lua -e 'print(a);b=2'
	-$(BIN)/lua -lstrict -e 'print(a)'
	-$(BIN)/lua -e 'function f() b=2 end f()'
	-$(BIN)/lua -lstrict -e 'function f() b=2 end f()'

clean:
	$(RM) $T a.out core core.* *.o luac.out dummy.so

.PHONY:	default min noparser one one-lua one-lib one-luac so strict clean
