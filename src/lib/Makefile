# makefile for Lua standard library

LUA= ../..

include $(LUA)/config

EXTRA_DEFS= $(POPEN) $(TMPNAM) $(DEGREES) $(LOADLIB)

OBJS= lauxlib.o lbaselib.o ldblib.o liolib.o lmathlib.o ltablib.o lstrlib.o loadlib.o
SRCS= lauxlib.c lbaselib.c ldblib.c liolib.c lmathlib.c ltablib.c lstrlib.c loadlib.c

T= $(LIB)/liblualib.a

all:	$T

$T:	$(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f $(OBJS) $T

co:
	co -q -f -M $(SRCS)

klean:	clean
	rm -f $(SRCS)
