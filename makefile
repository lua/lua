# $Id: makefile,v 1.1 1993/12/17 18:59:10 celes Exp roberto $
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


hash.c : RCS/hash.c,v
	co $@
hash.h : RCS/hash.h,v
	co $@
inout.c : RCS/inout.c,v
	co $@
inout.h : RCS/inout.h,v
	co $@
iolib.c : RCS/iolib.c,v
	co $@
lua.c : RCS/lua.c,v
	co $@
lua.h : RCS/lua.h,v
	co $@
lua.lex : RCS/lua.lex,v
	co $@
lua.stx : RCS/lua.stx,v
	co $@
lualib.h : RCS/lualib.h,v
	co $@
mathlib.c : RCS/mathlib.c,v
	co $@
mathlib.h : RCS/mathlib.h,v
	co $@
opcode.c : RCS/opcode.c,v
	co $@
opcode.h : RCS/opcode.h,v
	co $@
strlib.c : RCS/strlib.c,v
	co $@
strlib.h : RCS/strlib.h,v
	co $@
table.c : RCS/table.c,v
	co $@
table.h : RCS/table.h,v
	co $@
