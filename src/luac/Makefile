# makefile for Lua compiler

LUA= ../..

include $(LUA)/config

INCS= -I$(INC) -I.. $(EXTRA_INCS)
OBJS= luac.o print.o
SRCS= luac.c print.c

T= $(BIN)/luac

all:	$T

$T:	$(OBJS) $(LIB)/liblua.a ../lib/lauxlib.o
	$(CC) -o $@ $(MYLDFLAGS) $(OBJS) ../lib/lauxlib.o -L$(LIB) -llua $(EXTRA_LIBS)

$(LIB)/liblua.a:
	cd ..; $(MAKE)

../lib/lauxlib.o:
	cd ../lib; $(MAKE) lauxlib.o

clean:
	rm -f $(OBJS) $T

co:
	co -q -f -M $(SRCS)

klean:	clean
	rm -f $(SRCS)
