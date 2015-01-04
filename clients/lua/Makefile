# makefile for lua interpreter

BIN= $(LUA)/bin
INC= $(LUA)/include
LIB= $(LUA)/lib

CC= gcc
CFLAGS= -g -Wall -O2 -I$(INC)

OBJS= lua.o
T=$(BIN)/lua

$T: $(OBJS)
	$(CC) -o $@ $(OBJS) -L$(LIB) -llua -llualib -lm

clean:
	rm -f $T $(OBJS)
