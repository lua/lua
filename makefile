#
## $Id: makefile,v 1.39 2002/12/06 17:20:45 roberto Exp roberto $
## Makefile
## See Copyright Notice in lua.h
#


#CONFIGURATION

# -DEXTERNMEMCHECK -DHARDSTACKTESTS
# DEBUG = -g -DLUA_USER_H='"ltests.h"'
OPTIMIZE =  -O2 \
  -D'lua_number2int(i,d)=__asm__("fldl %1\nfistpl %0":"=m"(i):"m"(d))' \
#   -fomit-frame-pointer


CONFIG = $(DEBUG) $(OPTIMIZE) -DLUA_COMPATUPSYNTAX -DUSE_TMPNAME


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

CFLAGS = $(CONFIG) $(CWARNS)  # -ansi


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
	ldump.o \
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
	ldblib.o \
	loadlib.o


lua : lua.o liblua.a liblualib.a
	$(CC) $(CFLAGS) -o $@ lua.o -Wl,-E -L. -llua -llualib -lm -ldl 

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


lapi.o: lapi.c lua.h lapi.h lobject.h llimits.h ldebug.h lstate.h ltm.h \
  lzio.h ldo.h lfunc.h lgc.h lmem.h lstring.h ltable.h lundump.h lvm.h
lauxlib.o: lauxlib.c lua.h lauxlib.h
lbaselib.o: lbaselib.c lua.h lauxlib.h lualib.h
lcode.o: lcode.c lua.h lcode.h llex.h lobject.h llimits.h lzio.h \
  lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h ldo.h lmem.h
ldblib.o: ldblib.c lua.h lauxlib.h lualib.h
ldebug.o: ldebug.c lua.h lapi.h lobject.h llimits.h lcode.h llex.h lzio.h \
  lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h ldo.h lfunc.h \
  lstring.h lvm.h
ldo.o: ldo.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h \
  ldo.h lfunc.h lgc.h lmem.h lopcodes.h lparser.h ltable.h lstring.h \
  lundump.h lvm.h
ldump.o: ldump.c lua.h lobject.h llimits.h lopcodes.h lstate.h ltm.h \
  lzio.h lundump.h
lfunc.o: lfunc.c lua.h lfunc.h lobject.h llimits.h lgc.h lmem.h lstate.h \
  ltm.h lzio.h
lgc.o: lgc.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h \
  ldo.h lfunc.h lgc.h lmem.h lstring.h ltable.h
liolib.o: liolib.c lua.h lauxlib.h lualib.h
llex.o: llex.c lua.h ldo.h lobject.h llimits.h lstate.h ltm.h lzio.h \
  llex.h lparser.h ltable.h lstring.h
lmathlib.o: lmathlib.c lua.h lauxlib.h lualib.h
lmem.o: lmem.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h \
  ldo.h lmem.h
lobject.o: lobject.c lua.h ldo.h lobject.h llimits.h lstate.h ltm.h \
  lzio.h lmem.h lstring.h lvm.h
loadlib.o: loadlib.c lua.h lauxlib.h lualib.h
lopcodes.o: lopcodes.c lua.h lobject.h llimits.h lopcodes.h
lparser.o: lparser.c lua.h lcode.h llex.h lobject.h llimits.h lzio.h \
  lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h lfunc.h lmem.h \
  lstring.h
lstate.o: lstate.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h ldo.h lfunc.h lgc.h llex.h lmem.h lstring.h ltable.h
lstring.o: lstring.c lua.h lmem.h llimits.h lobject.h lstate.h ltm.h \
  lzio.h lstring.h
lstrlib.o: lstrlib.c lua.h lauxlib.h lualib.h
ltable.o: ltable.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h ldo.h lgc.h lmem.h ltable.h
ltablib.o: ltablib.c lua.h lauxlib.h lualib.h
ltests.o: ltests.c lua.h lapi.h lobject.h llimits.h lauxlib.h lcode.h \
  llex.h lzio.h lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h \
  ldo.h lfunc.h lmem.h lstring.h lualib.h
ltm.o: ltm.c lua.h lobject.h llimits.h lstate.h ltm.h lzio.h lstring.h \
  ltable.h
lua.o: lua.c lua.h lauxlib.h lualib.h
lundump.o: lundump.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lfunc.h lmem.h lopcodes.h lstring.h lundump.h
lvm.o: lvm.c lua.h ldebug.h lstate.h lobject.h llimits.h ltm.h lzio.h \
  ldo.h lfunc.h lgc.h lopcodes.h lstring.h ltable.h lvm.h
lzio.o: lzio.c lua.h llimits.h lmem.h lzio.h
