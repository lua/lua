#
## $Id: makefile,v 2.1 2003/12/10 12:13:36 roberto Exp roberto $
## Makefile
## See Copyright Notice in lua.h
#


#CONFIGURATION

# -DEXTERNMEMCHECK -DHARDSTACKTESTS
DEBUG = -g -DLUA_USER_H='"ltests.h"'
OPTIMIZE =  -O2 \
#   -fomit-frame-pointer


# -DUSE_TMPNAME??
CONFIG = $(DEBUG) $(OPTIMIZE) -DLUA_COMPATUPSYNTAX -DUSE_DLOPEN


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


lapi.o: lapi.c lua.h luaconf.h lapi.h lobject.h llimits.h ldebug.h \
  lstate.h ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h \
  lundump.h lvm.h
lauxlib.o: lauxlib.c lua.h luaconf.h lauxlib.h
lbaselib.o: lbaselib.c lua.h luaconf.h lauxlib.h lualib.h
lcode.o: lcode.c lua.h luaconf.h lcode.h llex.h lobject.h llimits.h \
  lzio.h lmem.h lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h \
  ldo.h lgc.h
ldblib.o: ldblib.c lua.h luaconf.h lauxlib.h lualib.h
ldebug.o: ldebug.c lua.h luaconf.h lapi.h lobject.h llimits.h lcode.h \
  llex.h lzio.h lmem.h lopcodes.h lparser.h ltable.h ldebug.h lstate.h \
  ltm.h ldo.h lfunc.h lstring.h lgc.h lvm.h
ldo.o: ldo.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lparser.h ltable.h \
  lstring.h lundump.h lvm.h
ldump.o: ldump.c lua.h luaconf.h lobject.h llimits.h lopcodes.h lstate.h \
  ltm.h lzio.h lmem.h lundump.h
lfunc.o: lfunc.c lua.h luaconf.h lfunc.h lobject.h llimits.h lgc.h lmem.h \
  lstate.h ltm.h lzio.h
lgc.o: lgc.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h
liolib.o: liolib.c lua.h luaconf.h lauxlib.h lualib.h
llex.o: llex.c lua.h luaconf.h ldo.h lobject.h llimits.h lstate.h ltm.h \
  lzio.h lmem.h llex.h lparser.h ltable.h lstring.h lgc.h
lmathlib.o: lmathlib.c lua.h luaconf.h lauxlib.h lualib.h
lmem.o: lmem.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h \
  ltm.h lzio.h lmem.h ldo.h
loadlib.o: loadlib.c lua.h luaconf.h lauxlib.h lualib.h
lobject.o: lobject.c lua.h luaconf.h ldo.h lobject.h llimits.h lstate.h \
  ltm.h lzio.h lmem.h lstring.h lgc.h lvm.h
lopcodes.o: lopcodes.c lua.h luaconf.h lobject.h llimits.h lopcodes.h
lparser.o: lparser.c lua.h luaconf.h lcode.h llex.h lobject.h llimits.h \
  lzio.h lmem.h lopcodes.h lparser.h ltable.h ldebug.h lstate.h ltm.h \
  ldo.h lfunc.h lstring.h lgc.h
lstate.o: lstate.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h \
  ltm.h lzio.h lmem.h ldo.h lfunc.h lgc.h llex.h lstring.h ltable.h
lstring.o: lstring.c lua.h luaconf.h lmem.h llimits.h lobject.h lstate.h \
  ltm.h lzio.h lstring.h lgc.h
lstrlib.o: lstrlib.c lua.h luaconf.h lauxlib.h lualib.h
ltable.o: ltable.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h \
  ltm.h lzio.h lmem.h ldo.h lgc.h ltable.h
ltablib.o: ltablib.c lua.h luaconf.h lauxlib.h lualib.h
ltests.o: ltests.c lua.h luaconf.h lapi.h lobject.h llimits.h lauxlib.h \
  lcode.h llex.h lzio.h lmem.h lopcodes.h lparser.h ltable.h ldebug.h \
  lstate.h ltm.h ldo.h lfunc.h lstring.h lgc.h lualib.h
ltm.o: ltm.c lua.h luaconf.h lobject.h llimits.h lstate.h ltm.h lzio.h \
  lmem.h lstring.h lgc.h ltable.h
lua.o: lua.c lua.h luaconf.h lauxlib.h lualib.h
lundump.o: lundump.c lua.h luaconf.h ldebug.h lstate.h lobject.h \
  llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lopcodes.h lstring.h lgc.h \
  lundump.h
lvm.o: lvm.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lstring.h ltable.h lvm.h
lzio.o: lzio.c lua.h luaconf.h llimits.h lmem.h lstate.h lobject.h ltm.h \
  lzio.h
