#
## $Id: makefile,v 1.31 2001/07/24 17:25:30 roberto Exp roberto $
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
# you may need to adapat the code, too.
#

#EXTRA_H=-DLUA_USER_H='"ltests.h"'

CONFIG = -DPOPEN -D_POSIX_SOURCE  $(EXTRA_H)
#CONFIG = -DOLD_ANSI


# Compilation parameters
CC = gcc
CWARNS = -Wall -W -pedantic \
	-Waggregate-return \
	-Wcast-align \
	-Wmissing-prototypes \
	-Wnested-externs \
	-Wpointer-arith \
	-Wshadow \
	-Wwrite-strings \
#	-Wcast-qual
#	-Wtraditional

CFLAGS = $(CONFIG) $(CWARNS) -ansi -O3 -fomit-frame-pointer


# To make early versions
CO_OPTIONS =


AR = ar
ARFLAGS	= rvl


# Aplication modules
LUAOBJS = \
	lstate.o \
	lapi.o \
	lmem.o \
	lstring.o \
	ltable.o \
	ltm.o \
	lvm.o \
	ldo.o \
	lobject.o \
	lfunc.o \
	lgc.o \
	lcode.o \
	lparser.o \
	llex.o \
	lopcodes.o \
	lundump.o \
	lzio.o \
	ldebug.o \
	ltests.o 

LIBOBJS = 	\
	lauxlib.o \
	lbaselib.o \
	liolib.o \
	lmathlib.o \
	lstrlib.o \
	ldblib.o


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


lapi.o: lapi.c lua.h lapi.h lobject.h llimits.h ldo.h lstate.h \
 luadebug.h lfunc.h lgc.h lmem.h lstring.h ltable.h ltm.h lvm.h
lauxlib.o: lauxlib.c lua.h lauxlib.h luadebug.h lualib.h
lbaselib.o: lbaselib.c lua.h lauxlib.h luadebug.h lualib.h
lcode.o: lcode.c lua.h lcode.h llex.h lobject.h llimits.h lzio.h \
 lopcodes.h lparser.h ldebug.h lstate.h luadebug.h ldo.h lmem.h
ldblib.o: ldblib.c lua.h lauxlib.h luadebug.h lualib.h
ldebug.o: ldebug.c lua.h lapi.h lobject.h llimits.h lcode.h llex.h \
 lzio.h lopcodes.h lparser.h ldebug.h lstate.h luadebug.h ldo.h \
 lfunc.h lstring.h ltable.h ltm.h lvm.h
ldo.o: ldo.c lua.h ldebug.h lstate.h lobject.h llimits.h luadebug.h \
 ldo.h lgc.h lmem.h lparser.h lzio.h lstring.h ltable.h ltm.h \
 lundump.h lvm.h
lfunc.o: lfunc.c lua.h lfunc.h lobject.h llimits.h lmem.h lstate.h \
 luadebug.h
lgc.o: lgc.c lua.h ldo.h lobject.h llimits.h lstate.h luadebug.h \
 lfunc.h lgc.h lmem.h lstring.h ltable.h ltm.h
liolib.o: liolib.c lua.h lauxlib.h luadebug.h lualib.h
llex.o: llex.c lua.h llex.h lobject.h llimits.h lzio.h lparser.h \
 lstate.h luadebug.h lstring.h
lmathlib.o: lmathlib.c lua.h lauxlib.h lualib.h
lmem.o: lmem.c lua.h ldo.h lobject.h llimits.h lstate.h luadebug.h \
 lmem.h
lobject.o: lobject.c lua.h ldo.h lobject.h llimits.h lstate.h \
 luadebug.h lmem.h
lopcodes.o: lopcodes.c lua.h lopcodes.h llimits.h
lparser.o: lparser.c lua.h lcode.h llex.h lobject.h llimits.h lzio.h \
 lopcodes.h lparser.h ldebug.h lstate.h luadebug.h lfunc.h lmem.h \
 lstring.h
lstate.o: lstate.c lua.h ldo.h lobject.h llimits.h lstate.h luadebug.h \
 lgc.h llex.h lzio.h lmem.h lstring.h ltable.h ltm.h
lstring.o: lstring.c lua.h lmem.h llimits.h lobject.h lstate.h \
 luadebug.h lstring.h
lstrlib.o: lstrlib.c lua.h lauxlib.h lualib.h
ltable.o: ltable.c lua.h ldo.h lobject.h llimits.h lstate.h luadebug.h \
 lmem.h ltable.h
ltests.o: ltests.c lua.h lapi.h lobject.h llimits.h lauxlib.h lcode.h \
 llex.h lzio.h lopcodes.h lparser.h ldebug.h lstate.h luadebug.h ldo.h \
 lfunc.h lmem.h lstring.h ltable.h lualib.h
ltm.o: ltm.c lua.h ldo.h lobject.h llimits.h lstate.h luadebug.h \
 lmem.h lstring.h ltable.h ltm.h
lua.o: lua.c lua.h luadebug.h lualib.h
lundump.o: lundump.c lua.h ldebug.h lstate.h lobject.h llimits.h \
 luadebug.h lfunc.h lmem.h lopcodes.h lstring.h lundump.h lzio.h
lvm.o: lvm.c lua.h lapi.h lobject.h llimits.h ldebug.h lstate.h \
 luadebug.h ldo.h lfunc.h lgc.h lopcodes.h lstring.h ltable.h ltm.h \
 lvm.h
lzio.o: lzio.c lua.h lzio.h
