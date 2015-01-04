# makefile for lualib

INC= $(LUA)/include
LIB= $(LUA)/lib

# in SunOs /usr/5include contains prototypes for standard lib
INCS= -I/usr/5include -I$(INC)
WARN= -Wall -Wmissing-prototypes -Wshadow -ansi

# if your system does not have popen, remove -DPOPEN from the line below
DEFS= -DPOPEN

CC= gcc
CFLAGS= $(INCS) $(DEFS) $(WARN) -O2

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
