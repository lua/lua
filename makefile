# $Id: makefile,v 1.4 1993/12/22 21:52:26 roberto Exp roberto $
# Compilation parameters
CC = gcc
CFLAGS = -I/usr/5include -Wall -O2 -DMAXCODE=4096 -DMAXCONSTANT=1024 -DMAXSYMBOL=1024 

AR = ar
ARFLAGS	= rvl

# Aplication modules
LUAMOD =	\
	y.tab 	\
	lex	\
	opcode	\
	hash	\
	table	\
	inout

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

% : RCS/%,v
	co $@

