# $Id: makefile,v 1.7 1994/07/19 22:04:51 celes Exp $
# Compilation parameters
CC = gcc
CFLAGS = -I/usr/5include -Wall -Wmissing-prototypes -ansi -O2

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
	tree

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

.KEEP_STATE:

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

