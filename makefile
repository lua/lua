#
## $Id: makefile,v 1.20 1999/08/17 20:21:52 roberto Exp roberto $
## Makefile
## See Copyright Notice in lua.h
#


#CONFIGURATION

# define (undefine) POPEN if your system (does not) support piped I/O
#
# define (undefine) _POSIX_SOURCE if your system is (not) POSIX compliant
#
# define (undefine) OLD_ANSI if your system does NOT have some new ANSI
# facilities (e.g. strerror, locale.h, memmove). SunOS does not comply;
# so, add "-DOLD_ANSI" on SunOS
#
# define LUA_NUM_TYPE if you need numbers to be different from double
# (for instance, -DLUA_NUM_TYPE=float)
#
# define LUA_COMPAT_GC if you need garbage-collect tag methods for tables
# (only for compatibility with previous versions)

CONFIG = -DPOPEN -D_POSIX_SOURCE
#CONFIG = -DOLD_ANSI -DDEBUG -DLUA_COMPAT_GC


# Compilation parameters
CC = gcc
CWARNS = -Wall -Wmissing-prototypes -Wshadow -pedantic -Wpointer-arith -Wcast-align -Waggregate-return -Wwrite-strings -Wcast-qual -Wstrict-prototypes -Wmissing-declarations -Wnested-externs -Werror
CFLAGS = $(CONFIG) $(CWARNS) -ansi -O2


# To make early versions
CO_OPTIONS =


AR = ar
ARFLAGS	= rvl


# Aplication modules
LUAOBJS = \
	lapi.o \
	lauxlib.o \
	lbuffer.o \
	lbuiltin.o \
	ldo.o \
	lfunc.o \
	lgc.o \
	llex.o \
	lmem.o \
	lobject.o \
	lparser.o \
	lref.o \
	lstate.o \
	lstring.o \
	ltable.o \
	ltm.o \
	lvm.o \
	lundump.o \
	lzio.o

LIBOBJS = 	\
	liolib.o \
	lmathlib.o \
	lstrlib.o \
	ldblib.o \
	linit.o


lua : lua.o liblua.a liblualib.a
	$(CC) $(CFLAGS) -o $@ lua.o -L. -llua -llualib -lm

liblua.a : $(LUAOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib $@

liblualib.a : $(LIBOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib $@

liblua.so.1.0 : lua.o
	ld -o liblua.so.1.0 $(LUAOBJS)


clear	:
	rcsclean
	rm -f *.o
	rm -f
	co $(CO_OPTIONS) lua.h lualib.h luadebug.h


%.h : RCS/%.h,v
	co $(CO_OPTIONS) $@

%.c : RCS/%.c,v
	co $(CO_OPTIONS) $@


lapi.o: lapi.c lapi.h lua.h lobject.h lauxlib.h ldo.h lstate.h \
 luadebug.h lfunc.h lgc.h lmem.h lref.h lstring.h ltable.h ltm.h lvm.h
lauxlib.o: lauxlib.c lauxlib.h lua.h luadebug.h
lbuffer.o: lbuffer.c lauxlib.h lua.h lmem.h lstate.h lobject.h \
 luadebug.h
lbuiltin.o: lbuiltin.c lapi.h lua.h lobject.h lauxlib.h lbuiltin.h \
 ldo.h lstate.h luadebug.h lfunc.h lmem.h lstring.h ltable.h ltm.h \
 lundump.h lzio.h lvm.h
ldblib.o: ldblib.c lauxlib.h lua.h luadebug.h lualib.h
ldo.o: ldo.c lauxlib.h lua.h ldo.h lobject.h lstate.h luadebug.h lgc.h \
 lmem.h lparser.h lzio.h lstring.h ltm.h lundump.h lvm.h
lfunc.o: lfunc.c lfunc.h lobject.h lua.h lmem.h lstate.h luadebug.h
lgc.o: lgc.c ldo.h lobject.h lua.h lstate.h luadebug.h lfunc.h lgc.h \
 lref.h lstring.h ltable.h ltm.h
linit.o: linit.c lua.h lualib.h
liolib.o: liolib.c lauxlib.h lua.h luadebug.h lualib.h
llex.o: llex.c lauxlib.h lua.h llex.h lobject.h lzio.h lmem.h \
 lparser.h lstate.h luadebug.h lstring.h
lmathlib.o: lmathlib.c lauxlib.h lua.h lualib.h
lmem.o: lmem.c lmem.h lstate.h lobject.h lua.h luadebug.h
lobject.o: lobject.c lobject.h lua.h
lparser.o: lparser.c ldo.h lobject.h lua.h lstate.h luadebug.h lfunc.h \
 llex.h lzio.h lmem.h lopcodes.h lparser.h lstring.h
lref.o: lref.c lmem.h lref.h lobject.h lua.h lstate.h luadebug.h
lstate.o: lstate.c lbuiltin.h ldo.h lobject.h lua.h lstate.h \
 luadebug.h lgc.h llex.h lzio.h lmem.h lstring.h ltm.h
lstring.o: lstring.c lmem.h lobject.h lua.h lstate.h luadebug.h \
 lstring.h
lstrlib.o: lstrlib.c lauxlib.h lua.h lualib.h
ltable.o: ltable.c lauxlib.h lua.h lmem.h lobject.h lstate.h \
 luadebug.h ltable.h
ltm.o: ltm.c lauxlib.h lua.h lmem.h lobject.h lstate.h luadebug.h \
 ltm.h
lua.o: lua.c lua.h luadebug.h lualib.h
lundump.o: lundump.c lauxlib.h lua.h lfunc.h lobject.h lmem.h \
 lopcodes.h lstring.h lundump.h lzio.h
lvm.o: lvm.c lauxlib.h lua.h ldo.h lobject.h lstate.h luadebug.h \
 lfunc.h lgc.h lopcodes.h lstring.h ltable.h ltm.h lvm.h
lzio.o: lzio.c lzio.h
