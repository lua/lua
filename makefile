# $Id: makefile,v 1.21 1996/03/05 15:57:53 roberto Exp roberto $

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
	func.o \
	undump.o

LIBOBJS = 	\
	iolib.o \
	mathlib.o \
	strlib.o


lua : lua.o lua.a lualib.a
	$(CC) $(CFLAGS) -o $@ lua.o lua.a lualib.a -lm

lua.a : $(LUAOBJS)
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
inout.o : inout.c lex.h opcode.h lua.h types.h tree.h func.h inout.h table.h \
  mem.h 
iolib.o : iolib.c lua.h luadebug.h lualib.h 
lex.o : lex.c mem.h tree.h types.h table.h opcode.h lua.h func.h lex.h inout.h \
  luadebug.h parser.h 
lua.o : lua.c lua.h lualib.h 
mathlib.o : mathlib.c lualib.h lua.h 
mem.o : mem.c mem.h lua.h table.h tree.h types.h opcode.h func.h 
opcode.o : opcode.c luadebug.h lua.h mem.h opcode.h types.h tree.h func.h hash.h \
  inout.h table.h fallback.h undump.h 
parser.o : parser.c luadebug.h lua.h mem.h lex.h opcode.h types.h tree.h func.h \
  hash.h inout.h table.h 
strlib.o : strlib.c lua.h lualib.h 
table.o : table.c mem.h opcode.h lua.h types.h tree.h func.h hash.h table.h \
  inout.h fallback.h luadebug.h 
tree.o : tree.c mem.h lua.h tree.h types.h lex.h hash.h opcode.h func.h table.h 
undump.o : undump.c opcode.h lua.h types.h tree.h func.h mem.h table.h undump.h 
