# makefile for installing Lua
# see INSTALL for installation instructions
# see src/Makefile and src/luaconf.h for further customization

# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

# Your platform. See PLATS for possible values.
PLAT= none

# Where to install. The installation starts in the src directory, so take care
# if INSTALL_TOP is not an absolute path. (Man pages are installed from the
# doc directory.)
#
INSTALL_TOP= /usr/local
INSTALL_BIN= $(INSTALL_TOP)/bin
INSTALL_INC= $(INSTALL_TOP)/include
INSTALL_LIB= $(INSTALL_TOP)/lib
INSTALL_MAN= $(INSTALL_TOP)/man/man1
INSTALL_LMOD= $(INSTALL_TOP)/share/lua/$V
INSTALL_CMOD= $(INSTALL_TOP)/lib/lua/$V

# How to install. You may prefer "install" instead of "cp" if you have it.
# To remove debug information from binaries, use "install -s" in INSTALL_EXEC.
#
INSTALL_EXEC= $(CP)
INSTALL_DATA= $(CP)
#INSTALL_EXEC= $(INSTALL) -m 0755
#INSTALL_DATA= $(INSTALL) -m 0644

# Utilities.
CP= cp
FIND= find
INSTALL= install
MKDIR= mkdir
RANLIB= ranlib

# == END OF USER SETTINGS. NO NEED TO CHANGE ANYTHING BELOW THIS LINE =========

# Convenience platforms targets.
PLATS= aix ansi bsd generic linux macosx mingw posix solaris

# What to install.
TO_BIN= lua luac
TO_INC= lua.h luaconf.h lualib.h lauxlib.h ../etc/lua.hpp
TO_LIB= liblua.a
TO_MAN= lua.1 luac.1

# Lua version and release.
V= 5.1
R= 5.1.1

all:	$(PLAT)

$(PLATS) clean:
	cd src && $(MAKE) $@

test:	dummy
	src/lua test/hello.lua

install: dummy
	cd src && $(MKDIR) -p $(INSTALL_BIN) $(INSTALL_INC) $(INSTALL_LIB) $(INSTALL_MAN) $(INSTALL_LMOD) $(INSTALL_CMOD)
	cd src && $(INSTALL_EXEC) $(TO_BIN) $(INSTALL_BIN)
	cd src && $(INSTALL_DATA) $(TO_INC) $(INSTALL_INC)
	cd src && $(INSTALL_DATA) $(TO_LIB) $(INSTALL_LIB)
	cd doc && $(INSTALL_DATA) $(TO_MAN) $(INSTALL_MAN)
#	$(RANLIB) $(INSTALL_LIB)/$(TO_LIB)

local:
	$(MAKE) install INSTALL_TOP=.. INSTALL_EXEC="cp -p" INSTALL_DATA="cp -p"

none:
	@echo "Please do"
	@echo "   make PLATFORM"
	@echo "where PLATFORM is one of these:"
	@echo "   $(PLATS)"
	@echo "See INSTALL for complete instructions."

# make may get confused with test/ and INSTALL in a case-insensitive OS
dummy:

# echo config parameters
echo:
	@echo ""
	@echo "These are the parameters currently set in src/Makefile to build Lua $R:"
	@echo ""
	@cd src && $(MAKE) -s echo
	@echo ""
	@echo "These are the parameters currently set in Makefile to install Lua $R:"
	@echo ""
	@echo "PLAT = $(PLAT)"
	@echo "INSTALL_TOP = $(INSTALL_TOP)"
	@echo "INSTALL_BIN = $(INSTALL_BIN)"
	@echo "INSTALL_INC = $(INSTALL_INC)"
	@echo "INSTALL_LIB = $(INSTALL_LIB)"
	@echo "INSTALL_MAN = $(INSTALL_MAN)"
	@echo "INSTALL_LMOD = $(INSTALL_LMOD)"
	@echo "INSTALL_CMOD = $(INSTALL_CMOD)"
	@echo "INSTALL_EXEC = $(INSTALL_EXEC)"
	@echo "INSTALL_DATA = $(INSTALL_DATA)"
	@echo ""
	@echo "See also src/luaconf.h ."
	@echo ""

# echo private config parameters
pecho:
	@echo "V = $(V)"
	@echo "R = $(R)"
	@echo "TO_BIN = $(TO_BIN)"
	@echo "TO_INC = $(TO_INC)"
	@echo "TO_LIB = $(TO_LIB)"
	@echo "TO_MAN = $(TO_MAN)"

# echo config parameters as Lua code
# uncomment the last sed expression if you want nil instead of empty strings
lecho:
	@echo "-- installation parameters for Lua $R"
	@echo "VERSION = '$V'"
	@echo "RELEASE = '$R'"
	@$(MAKE) echo | grep = | sed -e 's/= /= "/' -e 's/$$/"/' #-e 's/""/nil/'
	@echo "-- EOF"

# show what has changed since we unpacked
newer:
	@$(FIND) . -newer MANIFEST -type f

# list targets that do not create files (but not all makes understand .PHONY)
.PHONY: all $(PLATS) clean test install local none dummy echo pecho lecho newer

# (end of Makefile)
