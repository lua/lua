# $Id: makefile,v 1.14 1995/10/17 14:12:45 roberto Exp $

#configuration

# define (undefine) POPEN if your system (does not) support piped I/O
CONFIG = -DPOPEN
# Compilation parameters
CC = gcc
CFLAGS = $(CONFIG) -I/usr/5include -Wall -Wmissing-prototypes -Wshadow -ansi -O2

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
	mem	\
	func

LIBMOD = 	\
	iolib	\
	strlib	\
	mathlib

LUAOBJS	= $(LUAMOD:%=%.o)

LIBOBJS	= $(LIBMOD:%=%.o)

lua : lua.o lua.a lualib.a
	$(CC) $(CFLAGS) -o $@ lua.o lua.a lualib.a -lm

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


parser.c : lua.stx
	yacc++ -d lua.stx ; mv -f y.tab.c parser.c ; mv -f y.tab.h parser.h

clear	:
	rcsclean
	rm -f *.o
	rm -f parser.c parser.h
	co lua.h lualib.h luadebug.h

% : RCS/%,v
	co $@


fallback.o : fallback.c mem.h fallback.h opcode.h lua.h types.h tree.h func.h 
func.o : func.c table.h tree.h types.h opcode.h lua.h func.h mem.h 
hash.o : hash.c mem.h opcode.h lua.h types.h tree.h func.h hash.h table.h 
inout.o : inout.c mem.h opcode.h lua.h types.h tree.h func.h hash.h inout.h \
  table.h 
iolib.o : iolib.c lua.h lualib.h luadebug.h
lex.o : lex.c mem.h tree.h types.h table.h opcode.h lua.h func.h inout.h parser.h \
  ugly.h 
lua.o : lua.c lua.h lualib.h 
mathlib.o : mathlib.c lualib.h lua.h 
mem.o : mem.c mem.h lua.h 
opcode.o : opcode.c mem.h opcode.h lua.h types.h tree.h func.h hash.h inout.h \
  table.h fallback.h luadebug.h
parser.o : parser.c mem.h opcode.h lua.h types.h tree.h func.h hash.h inout.h \
  table.h 
strlib.o : strlib.c lua.h lualib.h 
table.o : table.c mem.h opcode.h lua.h types.h tree.h func.h hash.h table.h \
  inout.h fallback.h 
tree.o : tree.c mem.h lua.h tree.h types.h table.h opcode.h func.h 
