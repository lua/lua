# $Id: makefile,v 1.10 1994/12/23 20:47:59 roberto Exp celes $
# Compilation parameters
CC = gcc
CFLAGS = -I/usr/5include -Wall -Wmissing-prototypes -Wshadow -ansi -O2

#CC = acc
#CFLAGS = -fast -I/usr/5include

AR = ar
ARFLAGS	= rvl

# Aplication modules
LUAMOD =	\
	parser 	\
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

lua.a : parser.c $(LUAOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib lua.a

lualib.a : $(LIBOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib $@

liblua.so.1.0 : lua.o
	ld -o liblua.so.1.0 $(LUAOBJS)

%.o	: %.c
	$(CC) $(CFLAGS) -c -o $@ $<


parser.c : lua.stx exscript
	yacc -d lua.stx ; mv -f y.tab.c parser.c ; mv -f y.tab.h parser.h ; ex parser.c <exscript

clear	:
	rcsclean
	rm -f *.o
	rm -f parser.c parser.h
	co lua.h lualib.h

% : RCS/%,v
	co $@


fallback.o : fallback.c mem.h fallback.h opcode.h lua.h types.h tree.h inout.h 
hash.o : hash.c mem.h opcode.h lua.h types.h tree.h hash.h inout.h table.h 
inout.o : inout.c mem.h opcode.h lua.h types.h tree.h hash.h inout.h table.h 
iolib.o : iolib.c mem.h lua.h lualib.h 
lex.o : lex.c tree.h types.h table.h opcode.h lua.h inout.h parser.h ugly.h 
lua.o : lua.c lua.h lualib.h 
mathlib.o : mathlib.c lualib.h lua.h 
mem.o : mem.c mem.h lua.h 
opcode.o : opcode.c mem.h opcode.h lua.h types.h tree.h hash.h inout.h table.h \
  fallback.h 
strlib.o : strlib.c mem.h lua.h lualib.h 
table.o : table.c mem.h opcode.h lua.h types.h tree.h hash.h inout.h table.h \
  fallback.h 
tree.o : tree.c mem.h lua.h tree.h types.h table.h opcode.h 
parser.o : parser.c mem.h opcode.h lua.h types.h tree.h hash.h inout.h table.h 
