# makefile for building Lua
# see INSTALL for installation instructions
# see ../Makefile and luaconf.h for further customization

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

CWARNS= -pedantic -Wextra \
	-Waggregate-return \
	-Wcast-align \
        -Wdeclaration-after-statement \
        -Wdisabled-optimization \
        -Wmissing-prototypes \
        -Wnested-externs \
	-Wpointer-arith \
	-Wshadow \
        -Wsign-compare \
	-Wstrict-prototypes \
	-Wundef \
	-Wwrite-strings \
	#  -Wcast-qual \


# -DEXTERNMEMCHECK -DHARDSTACKTESTS -DHARDMEMTESTS
# -g -DLUA_USER_H='"ltests.h"'
# -fomit-frame-pointer #-pg -malign-double
TESTS= -g -DLUA_USER_H='"ltests.h"'

LOCAL = $(TESTS) $(CWARNS)

MYCFLAGS= $(LOCAL)
MYLDFLAGS=
MYLIBS=


CC= gcc
CFLAGS= -Wall -O2 $(MYCFLAGS)
# CC= ~lhf/sunstudio12/bin/cc
# CFLAGS= -xO5 -v -Xc -native -xstrconst
AR= ar rcu
RANLIB= ranlib
RM= rm -f


# enable Linux goodies
MYCFLAGS= $(LOCAL) -DLUA_USE_LINUX
MYLDFLAGS= -Wl,-E
MYLIBS= -ldl -lreadline -lhistory -lncurses



# == END OF USER SETTINGS. NO NEED TO CHANGE ANYTHING BELOW THIS LINE =========


LIBS = -lm

CORE_T=	liblua.a
CORE_O=	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o \
	lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o \
	ltm.o lundump.o lvm.o lzio.o ltests.o
AUX_O=	lauxlib.o
LIB_O=	lbaselib.o ldblib.o liolib.o lmathlib.o loslib.o ltablib.o lstrlib.o \
	lbitlib.o loadlib.o linit.o

LUA_T=	lua
LUA_O=	lua.o

LUAC_T=	luac
LUAC_O=	luac.o print.o

ALL_T= $(CORE_T) $(LUA_T) $(LUAC_T)
ALL_O= $(CORE_O) $(LUA_O) $(LUAC_O) $(AUX_O) $(LIB_O)
ALL_A= $(CORE_T)

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(ALL_A)

$(CORE_T): $(CORE_O) $(AUX_O) $(LIB_O)
	$(AR) $@ $?
	$(RANLIB) $@

$(LUA_T): $(LUA_O) $(CORE_T)
	$(CC) -o $@ $(MYLDFLAGS) $(LUA_O) $(CORE_T) $(LIBS) $(MYLIBS) $(DL)

$(LUAC_T): $(LUAC_O) $(CORE_T)
	$(CC) -o $@ $(MYLDFLAGS) $(LUAC_O) $(CORE_T) $(LIBS) $(MYLIBS)

clean:
	rcsclean -u
	$(RM) $(ALL_T) $(ALL_O)

depend:
	@$(CC) $(CFLAGS) -MM *.c

echo:
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "AR = $(AR)"
	@echo "RANLIB = $(RANLIB)"
	@echo "RM = $(RM)"
	@echo "MYCFLAGS = $(MYCFLAGS)"
	@echo "MYLDFLAGS = $(MYLDFLAGS)"
	@echo "MYLIBS = $(MYLIBS)"
	@echo "DL = $(DL)"

# DO NOT DELETE

lapi.o: lapi.c lua.h luaconf.h lapi.h llimits.h lstate.h lobject.h ltm.h \
  lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h lstring.h ltable.h lundump.h \
  lvm.h makefile
lauxlib.o: lauxlib.c lua.h luaconf.h lauxlib.h makefile
lbaselib.o: lbaselib.c lua.h luaconf.h lauxlib.h lualib.h makefile
lbitlib.o: lbitlib.c lua.h luaconf.h lauxlib.h lualib.h makefile
lcode.o: lcode.c lua.h luaconf.h lcode.h llex.h lobject.h llimits.h \
  lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h lgc.h \
  ltable.h makefile
lctype.o: lctype.c lctype.h lua.h luaconf.h makefile
ldblib.o: ldblib.c lua.h luaconf.h lauxlib.h lualib.h makefile
ldebug.o: ldebug.c lua.h luaconf.h lapi.h llimits.h lstate.h lobject.h \
  ltm.h lzio.h lmem.h lcode.h llex.h lopcodes.h lparser.h ldebug.h ldo.h \
  lfunc.h lstring.h lgc.h ltable.h lvm.h makefile
ldo.o: ldo.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lparser.h lstring.h \
  ltable.h lundump.h lvm.h makefile
ldump.o: ldump.c lua.h luaconf.h lobject.h llimits.h lstate.h ltm.h \
  lzio.h lmem.h lundump.h makefile
lfunc.o: lfunc.c lua.h luaconf.h lfunc.h lobject.h llimits.h lgc.h lmem.h \
  lstate.h ltm.h lzio.h makefile
lgc.o: lgc.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lmem.h ldo.h lfunc.h lgc.h lstring.h ltable.h makefile
linit.o: linit.c lua.h luaconf.h lualib.h lauxlib.h makefile
liolib.o: liolib.c lua.h luaconf.h lauxlib.h lualib.h makefile
llex.o: llex.c lua.h luaconf.h lctype.h ldo.h lobject.h llimits.h \
  lstate.h ltm.h lzio.h lmem.h llex.h lparser.h lstring.h lgc.h ltable.h makefile
lmathlib.o: lmathlib.c lua.h luaconf.h lauxlib.h lualib.h makefile
lmem.o: lmem.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h \
  ltm.h lzio.h lmem.h ldo.h lgc.h makefile
loadlib.o: loadlib.c lua.h luaconf.h lauxlib.h lualib.h makefile
lobject.o: lobject.c lua.h luaconf.h lctype.h ldebug.h lstate.h lobject.h \
  llimits.h ltm.h lzio.h lmem.h ldo.h lstring.h lgc.h lvm.h makefile
lopcodes.o: lopcodes.c lopcodes.h llimits.h lua.h luaconf.h makefile
loslib.o: loslib.c lua.h luaconf.h lauxlib.h lualib.h makefile
lparser.o: lparser.c lua.h luaconf.h lcode.h llex.h lobject.h llimits.h \
  lzio.h lmem.h lopcodes.h lparser.h ldebug.h lstate.h ltm.h ldo.h \
  lfunc.h lstring.h lgc.h ltable.h makefile
lstate.o: lstate.c lua.h luaconf.h lapi.h llimits.h lstate.h lobject.h \
  ltm.h lzio.h lmem.h ldebug.h ldo.h lfunc.h lgc.h llex.h lstring.h \
  ltable.h makefile
lstring.o: lstring.c lua.h luaconf.h lmem.h llimits.h lobject.h lstate.h \
  ltm.h lzio.h lstring.h lgc.h makefile
lstrlib.o: lstrlib.c lua.h luaconf.h lauxlib.h lualib.h makefile
ltable.o: ltable.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h \
  ltm.h lzio.h lmem.h ldo.h lgc.h ltable.h makefile
ltablib.o: ltablib.c lua.h luaconf.h lauxlib.h lualib.h makefile
ltests.o: ltests.c lua.h luaconf.h lapi.h llimits.h lstate.h lobject.h \
  ltm.h lzio.h lmem.h lauxlib.h lcode.h llex.h lopcodes.h lparser.h \
  lctype.h ldebug.h ldo.h lfunc.h lstring.h lgc.h ltable.h lualib.h makefile
ltm.o: ltm.c lua.h luaconf.h lobject.h llimits.h lstate.h ltm.h lzio.h \
  lmem.h lstring.h lgc.h ltable.h makefile
lua.o: lua.c lua.h luaconf.h lauxlib.h lualib.h makefile
lundump.o: lundump.c lua.h luaconf.h ldebug.h lstate.h lobject.h \
  llimits.h ltm.h lzio.h lmem.h ldo.h lfunc.h lstring.h lgc.h lundump.h makefile
lvm.o: lvm.c lua.h luaconf.h ldebug.h lstate.h lobject.h llimits.h ltm.h \
  lzio.h lmem.h ldo.h lfunc.h lgc.h lopcodes.h lstring.h ltable.h lvm.h makefile
lzio.o: lzio.c lua.h luaconf.h llimits.h lmem.h lstate.h lobject.h ltm.h \
  lzio.h makefile

# (end of Makefile)

