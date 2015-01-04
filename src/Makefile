# makefile for lua distribution

INC= $(LUA)/include
LIB= $(LUA)/lib

# in SunOs /usr/5include contains prototypes for standard lib
INCS= -I/usr/5include -I$(INC)
WARN= -Wall -Wmissing-prototypes -Wshadow -ansi

CC= gcc
CFLAGS= $(INCS) $(DEFS) $(WARN) -O2

OBJS= fallback.o func.o hash.o inout.o lex.o mem.o opcode.o parser.o table.o tree.o
SRCS= fallback.c func.c hash.c inout.c lex.c mem.c opcode.c table.c tree.c fallback.h func.h hash.h inout.h mem.h opcode.h table.h tree.h types.h ugly.h lua.stx

SLIB= $(LIB)/liblua.a

# dynamic libraries only work for SunOs
DLIB= $(LIB)/liblua.so.$(VERSION)

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
	co -f -M $(SRCS)
	yacc -d lua.stx ; mv -f y.tab.c parser.c ; mv -f y.tab.h parser.h

klean:	clean
	rm -f $(SRCS) parser.c parser.h
