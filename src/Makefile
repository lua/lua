# makefile for lua

LIB= $(LUA)/lib
INC= $(LUA)/include

CC= gcc
CFLAGS= -g -Wall -O2 -I$(INC) $(DEFS)
DEFS= -DMAXCODE=64000 -DMAXCONSTANT=1024 -DMAXSYMBOL=1024 -DMAXARRAY=1024

OBJS= hash.o inout.o lex.o opcode.o table.o y.tab.o
SLIB= $(LIB)/liblua.a
DLIB= $(LIB)/liblua.so.1.1

libs: $(SLIB) $(DLIB)

$(SLIB): $(OBJS)
	ar ruvl $@ $(OBJS)
	ranlib $(SLIB)

$(DLIB): $(OBJS)
	ld -o $@ $(OBJS)

clean:
	rm -f $(OBJS) $(SLIB) $(DLIB)
