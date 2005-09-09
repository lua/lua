# makefile for Lua etc

TOP= ..
LIB= $(TOP)/src
INC= $(TOP)/src
BIN= $(TOP)/src
SRC= $(TOP)/src
TST= $(TOP)/test

CC= gcc
CFLAGS= -O2 -Wall $(MYCFLAGS)
MYCFLAGS= -ansi -I$(INC)
MYLDFLAGS= -Wl,-E
MYLIBS= -lm -ldl
RM= rm -f

all:
	@echo 'choose a target: min noparser one clean'

min:	min.c
	$(CC) $(CFLAGS) $@.c -L$(LIB) -llua $(MYLIBS)
	echo 'print"Hello there!"' | ./a.out

noparser: noparser.o
	$(CC) noparser.o $(SRC)/lua.o -L$(LIB) -llua $(MYLIBS)
	$(BIN)/luac $(TST)/hello.lua
	-./a.out luac.out
	-./a.out -e'a=1'

one:
	$(CC) $(CFLAGS) all.c $(MYLIBS)
	./a.out $(TST)/hello.lua

clean:
	$(RM) noparser.o a.out core core.* *.o luac.out
