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

ALL= min noparser.o 1

all:
	@echo 'choose a target:' $(ALL)

min:	min.c
	$(CC) $(CFLAGS) -o $@ $@.c -L$(LIB) -llua -llualib

noparser: noparser.o
	$(CC) noparser.o $(SRC)/lua.o -L$(LIB) -llua -llualib $(MYLIBS)
	$(BIN)/luac $(TST)/hello.lua
	-./a.out luac.out
	-./a.out -e'a=1'

1:
	$(CC) $(CFLAGS) all.c $(MYLIBS)
	./a.out $(TST)/hello.lua

clean:
	$(RM) $(ALL) a.out core core.* *.o luac.out
