# $Id: makefile,v 1.3 1993/12/22 21:21:06 roberto Exp roberto $
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


exscript : RCS/exscript,v
	co $@
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
lex.c : RCS/lex.c,v
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
