# makefile for lua compiler

LUA= ../..

include $(LUA)/config

INCS= -I$(INC) $(EXTRA_INCS) -I..
OBJS= dump.o luac.o opcode.o opt.o print.o stubs.o test.o
SRCS= dump.c luac.c opcode.c opt.c print.c stubs.c test.c luac.h opcode.h

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
