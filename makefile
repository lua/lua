# $Id: makefile,v 1.18 1996/01/09 20:22:08 roberto Exp roberto $

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
LUAOBJS = \
	parser.o \
	lex.o \
	opcode.o \
	hash.o \
	table.o \
	inout.o \
	tree.o \
	fallback.o \
	mem.o \
	func.o

LIBOBJS = 	\
	iolib.o \
	mathlib.o \
	strlib.o


lua : lua.o lua.a lualib.a
	$(CC) $(CFLAGS) -o $@ lua.o lua.a lualib.a -lm

lua.a : parser.o $(LUAOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib lua.a

lualib.a : $(LIBOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib $@

liblua.so.1.0 : lua.o
	ld -o liblua.so.1.0 $(LUAOBJS)

y.tab.c y.tab.h  : lua.stx
	yacc++ -d lua.stx

parser.c : y.tab.c
	sed -e 's/yy/luaY_/g' y.tab.c > parser.c

parser.h : y.tab.h
	sed -e 's/yy/luaY_/g' y.tab.h > parser.h

clear	:
	rcsclean
	rm -f *.o
	rm -f parser.c parser.h y.tab.c y.tab.h
	co lua.h lualib.h luadebug.h

% : RCS/%,v
	co $@


fallback.o : fallback.c mem.h fallback.h opcode.h lua.h types.h tree.h func.h 
func.o : func.c luadebug.h lua.h table.h tree.h types.h opcode.h func.h mem.h 
hash.o : hash.c mem.h opcode.h lua.h types.h tree.h func.h hash.h table.h 
inout.o : inout.c mem.h opcode.h lua.h types.h tree.h func.h hash.h inout.h \
  table.h 
iolib.o : iolib.c lua.h luadebug.h lualib.h 
lex.o : lex.c mem.h tree.h types.h table.h opcode.h lua.h func.h inout.h luadebug.h \
  parser.h ugly.h 
lua.o : lua.c lua.h lualib.h 
mathlib.o : mathlib.c lualib.h lua.h 
mem.o : mem.c mem.h lua.h table.h tree.h types.h opcode.h func.h 
opcode.o : opcode.c luadebug.h lua.h mem.h opcode.h types.h tree.h func.h hash.h \
  inout.h table.h fallback.h 
parser.o : parser.c luadebug.h lua.h mem.h opcode.h types.h tree.h func.h hash.h \
  inout.h table.h 
strlib.o : strlib.c lua.h lualib.h 
table.o : table.c mem.h opcode.h lua.h types.h tree.h func.h hash.h table.h \
  inout.h fallback.h luadebug.h 
tree.o : tree.c mem.h lua.h tree.h types.h table.h opcode.h func.h 
