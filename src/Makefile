# makefile for lua distribution

INC= $(LUA)/include
LIB= $(LUA)/lib

# in SunOs /usr/5include contains prototypes for standard lib
INCS= -I/usr/5include -I$(INC)
WARN= -Wall -Wmissing-prototypes -Wshadow -ansi

CC= gcc
CFLAGS= $(INCS) $(DEFS) $(WARN) -O2

OBJS= fallback.o hash.o inout.o lex.o mem.o opcode.o parser.o table.o tree.o

SLIB= $(LIB)/liblua.a

# dynamic libraries only work for SunOs
DLIB= $(LIB)/liblua.so.2.1

all: $(SLIB)

dynamic: $(DLIB)

$(SLIB): $(OBJS)
	ar rcuv $@ $(OBJS)
	ranlib $@

$(DLIB): $(OBJS)
	ld -o $@ $(OBJS)

clean:
	rm -f $(OBJS) $(SLIB) $(DLIB)

co:
	co -M fallback.c hash.c inout.c lex.c mem.c opcode.c table.c tree.c fallback.h hash.h inout.h mem.h opcode.h table.h tree.h types.h ugly.h
