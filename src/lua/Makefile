# makefile for lua interpreter

LUA= ../..

include $(LUA)/config

EXTRA_DEFS= $(POSIX)

OBJS= lua.o
SRCS= lua.c

T= $(BIN)/lua

all: $T

$T: $(OBJS) $(LIB)/liblua.a $(LIB)/liblualib.a
	$(CC) -o $@ $(OBJS) -L$(LIB) -llua -llualib -lm

$(LIB)/liblua.a:
	cd ..; $(MAKE)

$(LIB)/liblualib.a:
	cd ../lib; $(MAKE)

clean:
	rm -f $(OBJS) $T

co:
	co -q -f -M $(SRCS)

klean:	clean
	rm -f $(SRCS)
