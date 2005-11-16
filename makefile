# makefile for building Lua
# see INSTALL for installation instructions
# see ../Makefile and luaconf.h for further customization

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

CWARNS= -pedantic -Waggregate-return -Wcast-align \
        -Wmissing-prototypes -Wpointer-arith -Wshadow \
        -Wsign-compare -Wstrict-prototypes -Wundef -Wwrite-strings
# -Wcast-qual

# -DEXTERNMEMCHECK -DHARDSTACKTESTS
# -g -DLUA_USER_H='"ltests.h"'
# -fomit-frame-pointer #-pg -malign-double
TESTS= -g -DLUA_USER_H='"ltests.h"'

LOCAL = $(TESTS) $(CWARNS)


CC= gcc
CFLAGS= -O2 -Wall $(MYCFLAGS)
AR= ar rcu
RANLIB= ranlib
RM= rm -f

MYCFLAGS= $(LOCAL)
MYLDFLAGS=
MYLIBS=


# enable Linux goodies
MYCFLAGS= $(LOCAL) -DLUA_USE_LINUX
MYLDFLAGS= -Wl,-E
MYLIBS= -ldl -lreadline -lhistory -lncurses



# == END OF USER SETTINGS. NO NEED TO CHANGE ANYTHING BELOW THIS LINE =========


LIBS = -lm

CORE_T=	liblua.a
CORE_O=	lapi.o lcode.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o lmem.o \
	lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o  \
	lundump.o lvm.o lzio.o ltests.o
AUX_O=	lauxlib.o
LIB_O=	lbaselib.o ldblib.o liolib.o lmathlib.o loslib.o ltablib.o lstrlib.o \
	loadlib.o linit.o

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
linit.o: linit.c lua.h luaconf.h lualib.h lauxlib.h
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
loslib.o: loslib.c lua.h luaconf.h lauxlib.h lualib.h
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

# (end of Makefile)
