#
## $Id: makefile,v 1.24 2000/04/14 17:52:09 roberto Exp roberto $
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
# define LUA_COMPAT_READPATTERN if you need read patterns
# (only for compatibility with previous versions)

CONFIG = -DPOPEN -D_POSIX_SOURCE
#CONFIG = -DOLD_ANSI -DDEBUG -DLUA_COMPAT_READPATTERN


# Compilation parameters
CC = gcc
CWARNS = -Wall -W -Wmissing-prototypes -Wshadow -pedantic -Wpointer-arith -Wcast-align -Waggregate-return -Wcast-qual -Wnested-externs -Wwrite-strings
CFLAGS = $(CONFIG) $(CWARNS) -ansi -O2


# To make early versions
CO_OPTIONS =


AR = ar
ARFLAGS	= rvl


# Aplication modules
LUAOBJS = \
	lstate.o \
	lref.o \
	lapi.o \
	lauxlib.o \
	lbuiltin.o \
	lmem.o \
	lstring.o \
	ltable.o \
	ltm.o \
	lvm.o \
	ldo.o \
	lobject.o \
	lbuffer.o \
	lfunc.o \
	lgc.o \
	lcode.o \
	lparser.o \
	llex.o \
	lundump.o \
	lzio.o \
	ldebug.o \
	ltests.o

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
	rm -f *.o *.a


%.h : RCS/%.h,v
	co $(CO_OPTIONS) $@

%.c : RCS/%.c,v
	co $(CO_OPTIONS) $@


lapi.o: lapi.c lapi.h lobject.h llimits.h lua.h lauxlib.h ldo.h \
 lstate.h luadebug.h lfunc.h lgc.h lmem.h lref.h lstring.h ltable.h \
 ltm.h lvm.h
lauxlib.o: lauxlib.c lauxlib.h lua.h luadebug.h
lbuffer.o: lbuffer.c lauxlib.h lua.h lmem.h lstate.h lobject.h \
 llimits.h luadebug.h
lbuiltin.o: lbuiltin.c lapi.h lobject.h llimits.h lua.h lauxlib.h \
 lbuiltin.h ldo.h lstate.h luadebug.h lfunc.h lmem.h lstring.h \
 ltable.h ltm.h lundump.h lzio.h lvm.h
lcode.o: lcode.c /usr/include/stdlib.h lcode.h llex.h lobject.h \
 llimits.h lua.h lzio.h lopcodes.h lparser.h ldo.h lstate.h luadebug.h \
 lmem.h lstring.h
ldblib.o: ldblib.c lauxlib.h lua.h luadebug.h lualib.h
ldebug.o: ldebug.c lapi.h lobject.h llimits.h lua.h lauxlib.h ldebug.h \
 luadebug.h ldo.h lstate.h lfunc.h ltable.h ltm.h
ldo.o: ldo.c lauxlib.h lua.h ldebug.h lobject.h llimits.h luadebug.h \
 ldo.h lstate.h lgc.h lmem.h lparser.h lzio.h lstring.h ltm.h \
 lundump.h lvm.h
lfunc.o: lfunc.c lfunc.h lobject.h llimits.h lua.h lmem.h lstate.h \
 luadebug.h
lgc.o: lgc.c ldo.h lobject.h llimits.h lua.h lstate.h luadebug.h \
 lfunc.h lgc.h lmem.h lref.h lstring.h ltable.h ltm.h
linit.o: linit.c lua.h lualib.h
liolib.o: liolib.c lauxlib.h lua.h luadebug.h lualib.h
llex.o: llex.c lauxlib.h lua.h llex.h lobject.h llimits.h lzio.h \
 lmem.h lparser.h lstate.h luadebug.h lstring.h
lmathlib.o: lmathlib.c lauxlib.h lua.h lualib.h
lmem.o: lmem.c lmem.h lua.h lobject.h llimits.h lstate.h luadebug.h
lobject.o: lobject.c lobject.h llimits.h lua.h
lparser.o: lparser.c lcode.h llex.h lobject.h llimits.h lua.h lzio.h \
 lopcodes.h lparser.h ldo.h lstate.h luadebug.h lfunc.h lmem.h \
 lstring.h
lref.o: lref.c lapi.h lobject.h llimits.h lua.h lmem.h lref.h lstate.h \
 luadebug.h
lstate.o: lstate.c lauxlib.h lua.h lbuiltin.h ldo.h lobject.h \
 llimits.h lstate.h luadebug.h lgc.h llex.h lzio.h lmem.h lref.h \
 lstring.h ltm.h
lstring.o: lstring.c lmem.h lua.h lobject.h llimits.h lstate.h \
 luadebug.h lstring.h
lstrlib.o: lstrlib.c lauxlib.h lua.h lualib.h
ltable.o: ltable.c lauxlib.h lua.h lmem.h lobject.h llimits.h lstate.h \
 luadebug.h ltable.h
ltests.o: ltests.c lapi.h lobject.h llimits.h lua.h lauxlib.h lmem.h \
 lopcodes.h lstate.h luadebug.h lstring.h ltable.h
ltm.o: ltm.c lauxlib.h lua.h lmem.h lobject.h llimits.h lstate.h \
 luadebug.h ltm.h
lua.o: lua.c lua.h luadebug.h lualib.h
lundump.o: lundump.c lauxlib.h lua.h lfunc.h lobject.h llimits.h \
 lmem.h lopcodes.h lstring.h lstate.h luadebug.h lundump.h lzio.h
lvm.o: lvm.c lauxlib.h lua.h ldebug.h lobject.h llimits.h luadebug.h \
 ldo.h lstate.h lfunc.h lgc.h lopcodes.h lstring.h ltable.h ltm.h \
 lvm.h
lzio.o: lzio.c lzio.h
