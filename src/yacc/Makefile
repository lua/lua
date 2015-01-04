# makefile for lua

LIB= $(LUA)/lib
INC= $(LUA)/inc

CC= gcc
CFLAGS= -g -Wall -O2 -I$(INC) $(DEFS)
DEFS= -DMAXCODE=64000 -DMAXCONSTANT=1024 -DMAXSYMBOL=1024 -DMAXARRAY=1024

OBJS= hash.o inout.o lex.o opcode.o table.o y.tab.o
SLIB= $(LIB)/lua.a
DLIB= $(LIB)/liblua.so.1.1

libs: $(SLIB) $(DLIB)

$(SLIB): y.tab.c $(OBJS)
	ar ruvl $@ $(OBJS)
	ranlib $(SLIB)

$(DLIB): $(OBJS)
	ld -o $@ $(OBJS)

y.tab.c: lua.stx exscript
	yacc -d lua.stx ; ex y.tab.c <exscript

clean:
	rm -f $(OBJS) $(SLIB) $(DLIB)
