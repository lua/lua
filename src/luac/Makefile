# makefile for Lua compiler

LUA= ../..

include $(LUA)/config

INCS= -I$(INC) $(EXTRA_INCS) -I..
OBJS= dump.o luac.o opt.o print.o stubs.o
SRCS= dump.c luac.c opt.c print.c stubs.c luac.h print.h

T= $(BIN)/luac

all: $T

$T: $(OBJS) $(LIB)/liblua.a
	$(CC) -o $@ $(OBJS) -L$(LIB) -llua

$(LIB)/liblua.a:
	cd ..; $(MAKE)

clean:
	rm -f $(OBJS) $T

co:
	co -q -f -M $(SRCS)

klean:	clean
	rm -f $(SRCS)
