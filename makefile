# $Id: $
# Compilation parameters
CC = gcc
CFLAGS = -I/usr/5include -Wall -DMAXCODE=4096 -DMAXCONSTANT=1024 -DMAXSYMBOL=1024 

AR = ar
ARFLAGS	= rvl

# Aplication modules
LUAMOD =	\
	lex.yy	\
	y.tab 	\
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

lua.a : lex.yy.c y.tab.c $(LUAOBJS)
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

lex.yy.c : lua.lex
	lex lua.lex

y.tab.c : lua.stx
	yacc -d lua.stx ; ex y.tab.c <exscript


