# $Id: makefile,v 1.9 1994/11/23 20:12:11 roberto Exp $
# Compilation parameters
CC = gcc
CFLAGS = -I/usr/5include -Wall -Wmissing-prototypes -Wshadow -ansi -O2

#CC = acc
#CFLAGS = -fast -I/usr/5include

AR = ar
ARFLAGS	= rvl

# Aplication modules
LUAMOD =	\
	y.tab 	\
	lex	\
	opcode	\
	hash	\
	table	\
	inout	\
	tree    \
	fallback\
	mem

LIBMOD = 	\
	iolib	\
	strlib	\
	mathlib

LUAOBJS	= $(LUAMOD:%=%.o)

LIBOBJS	= $(LIBMOD:%=%.o)

lua : lua.o lua.a lualib.a
	$(CC) $(CFLAGS) -o $@ lua.c lua.a lualib.a -lm

lua.a : y.tab.c $(LUAOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib lua.a

lualib.a : $(LIBOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib $@

liblua.so.1.0 : lua.o
	ld -o liblua.so.1.0 $(LUAOBJS)

%.o	: %.c
	$(CC) $(CFLAGS) -c -o $@ $<


y.tab.c : lua.stx exscript
	yacc -d lua.stx ; ex y.tab.c <exscript

clear	:
	rcsclean
	rm -f *.o
	rm -f y.tab.c y.tab.h

% : RCS/%,v
	co $@


fallback.o : fallback.c mem.h fallback.h opcode.h lua.h tree.h inout.h 
hash.o : hash.c mem.h opcode.h lua.h tree.h hash.h inout.h table.h 
inout.o : inout.c mem.h opcode.h lua.h tree.h hash.h inout.h table.h 
iolib.o : iolib.c mem.h lua.h lualib.h 
lex.o : lex.c tree.h table.h opcode.h lua.h inout.h y.tab.h ugly.h 
lua.o : lua.c lua.h lualib.h 
make.o : make.c lua.h lualib.h 
mathlib.o : mathlib.c lualib.h lua.h 
mem.o : mem.c mem.h lua.h 
newlua.o : newlua.c lua.h lualib.h 
opcode.o : opcode.c mem.h opcode.h lua.h tree.h hash.h inout.h table.h fallback.h 
strlib.o : strlib.c mem.h lua.h lualib.h 
table.o : table.c mem.h opcode.h lua.h tree.h hash.h inout.h table.h fallback.h 
temp1.o : temp1.c lua.h 
temp2.o : temp2.c lua.h 
tree.o : tree.c mem.h lua.h tree.h table.h opcode.h 
y.tab.o : y.tab.c mem.h opcode.h lua.h tree.h hash.h inout.h table.h 
