# makefile for lua interpreter

LUA= ../..

include $(LUA)/config

EXTRA_DEFS= $(POSIX)

OBJS= lua.o
SRCS= lua.c

T=$(BIN)/lua

all: $T

$T: $(OBJS)
	$(CC) -o $@ $(OBJS) -L$(LIB) -llua -llualib -lm

dynamic:
	rm -f $T
	make

clean:
	rm -f $T $(OBJS)

co:
	co -f -M $(SRCS)

klean:	clean
	rm -f $(SRCS)
