# makefile for lua interpreter

BIN= $(LUA)/bin
INC= $(LUA)/include
LIB= $(LUA)/lib

CC= gcc
CFLAGS= $(INCS) $(DEFS) $(WARN) -O2

# in SunOs /usr/5include contains prototypes for standard lib
INCS= -I/usr/5include -I$(INC)
WARN= -Wall -Wmissing-prototypes -Wshadow -ansi

OBJS= lua.o

T=$(BIN)/lua

$T: $(OBJS)
	$(CC) -o $@ $(OBJS) -L$(LIB) -llua -llualib -lm

dynamic:

clean:
	rm -f $T $(OBJS)

co:
	co -M lua.c
