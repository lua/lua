#
## $Id: makefile,v 1.35 2002/03/18 18:18:35 roberto Exp roberto $
## Makefile
## See Copyright Notice in lua.h
#


#CONFIGURATION

# define (undefine) POPEN if your system (does not) support piped I/O
#
# define (undefine) _POSIX_SOURCE if your system is (not) POSIX compliant
#
# define LUA_NUM_TYPE if you need numbers to be different from double
# (for instance, -DLUA_NUM_TYPE=float)
# you may need to adapat the code, too.


# DEBUG = -g -DLUA_USER_H='"ltests.h"'
OPTIMIZE =  -O2 \
   -D'lua_number2int(i,d)=__asm__("fldl %1\nfistpl %0":"=m"(i):"m"(d))' \
  -fomit-frame-pointer

CONFIG = -D_POSIX_SOURCE -DPOPEN $(DEBUG) $(OPTIMIZE)



# Compilation parameters
CC = gcc
CWARNS = -Wall -pedantic \
	-Waggregate-return \
	-Wcast-align \
	-Wmissing-prototypes \
	-Wstrict-prototypes \
	-Wnested-externs \
	-Wpointer-arith \
	-Wshadow \
	-Wwrite-strings \
#	-Wtraditional \
#	-Wcast-qual

CFLAGS = $(CONFIG) $(CWARNS) -ansi


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
        ltablib.o \
	lmathlib.o \
 	liolib.o \
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


lapi.o: lapi.c lua.h lapi.h lobject.h llimits.h ldebug.h lstate.h \
 ltm.h ldo.h lzio.h lfunc.h lgc.h lmem.h lstring.h ltable.h lundump.h \
 lvm.h
lauxlib.o: lauxlib.c lua.h lauxlib.h
lbaselib.o: lbaselib.c lua.h lauxlib.h lualib.h
lcode.o: lcode.c lua.h lcode.h llex.h lobject.h llimits.h lzio.h \
 lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h ldo.h lmem.h
ldblib.o: ldblib.c lua.h lauxlib.h lualib.h
ldebug.o: ldebug.c lua.h lapi.h lobject.h llimits.h lcode.h llex.h \
 lzio.h lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h ldo.h \
 lfunc.h lstring.h lvm.h
ldo.o: ldo.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h ldo.h \
 lzio.h lfunc.h lgc.h lmem.h lopcodes.h lparser.h ltable.h lstring.h \
 lundump.h lvm.h
lfunc.o: lfunc.c lua.h lfunc.h lobject.h llimits.h lmem.h lstate.h \
 ltm.h
lgc.o: lgc.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h ldo.h \
 lzio.h lfunc.h lgc.h lmem.h lstring.h ltable.h
liolib.o: liolib.c lua.h lauxlib.h lualib.h
llex.o: llex.c lua.h ldo.h lobject.h llimits.h lstate.h ltm.h lzio.h \
 llex.h lparser.h ltable.h lstring.h
lmathlib.o: lmathlib.c lua.h lauxlib.h lualib.h
lmem.o: lmem.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h ldo.h \
 lzio.h lmem.h
lobject.o: lobject.c lua.h ldo.h lobject.h llimits.h lstate.h ltm.h \
 lzio.h lmem.h lstring.h lvm.h
lopcodes.o: lopcodes.c lua.h lobject.h llimits.h lopcodes.h
lparser.o: lparser.c lua.h lcode.h llex.h lobject.h llimits.h lzio.h \
 lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h lfunc.h lmem.h \
 lstring.h
lstate.o: lstate.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
 ldo.h lzio.h lfunc.h lgc.h llex.h lmem.h lstring.h ltable.h
lstring.o: lstring.c lua.h lmem.h llimits.h lobject.h lstate.h ltm.h \
 lstring.h
lstrlib.o: lstrlib.c lua.h lauxlib.h lualib.h
ltable.o: ltable.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
 ldo.h lzio.h lmem.h ltable.h
ltablib.o: ltablib.c lua.h lauxlib.h lualib.h
ltests.o: ltests.c lua.h lapi.h lobject.h llimits.h lauxlib.h lcode.h \
 llex.h lzio.h lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h \
 ldo.h lfunc.h lmem.h lstring.h lualib.h
ltm.o: ltm.c lua.h lobject.h llimits.h lstate.h ltm.h lstring.h \
 ltable.h
lua.o: lua.c lua.h lauxlib.h lualib.h
lundump.o: lundump.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
 lfunc.h lmem.h lopcodes.h lstring.h lundump.h lzio.h
lvm.o: lvm.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h ldo.h \
 lzio.h lfunc.h lgc.h lopcodes.h lstring.h ltable.h lvm.h
lzio.o: lzio.c lua.h llimits.h lzio.h
