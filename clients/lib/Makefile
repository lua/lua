# makefile for lualib

LUA= ../..

include $(LUA)/config

EXTRA_DEFS= $(POPEN)

OBJS= iolib.o mathlib.o strlib.o
SRCS= iolib.c mathlib.c mathlib.h strlib.c strlib.h

SLIB= $(LIB)/liblualib.a

# dynamic libraries only work for SunOs
DLIB= $(LIB)/liblualib.so.$(VERSION)

all: $(SLIB)

dynamic: $(DLIB)

$(SLIB): $(OBJS)
	ar rcuv $@ $(OBJS)
	ranlib $(SLIB)

$(DLIB): $(OBJS)
	ld -o $@ $(OBJS)

clean:
	rm -f $(OBJS) $(SLIB) $(DLIB)

co:
	co -f -M $(SRCS)

klean:	clean
	rm -f $(SRCS)
