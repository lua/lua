# makefile for lualib

INC= $(LUA)/include
LIB= $(LUA)/lib

# in SunOs /usr/5include contains prototypes for standard lib
INCS= -I/usr/5include -I$(INC)
WARN= -Wall -Wmissing-prototypes -Wshadow -ansi

CC= gcc
CFLAGS= $(INCS) $(DEFS) $(WARN) -O2

OBJS= iolib.o mathlib.o strlib.o

SLIB= $(LIB)/liblualib.a

# dynamic libraries only work for SunOs
DLIB= $(LIB)/liblualib.so.2.1

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
	co -M iolib.c mathlib.c mathlib.h strlib.c strlib.h
