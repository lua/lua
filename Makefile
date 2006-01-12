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

# How to install. You may prefer "install" instead of "cp" if you have it.
# To remove debug information from binaries, use "install -s" in INSTALL_EXEC.
#
INSTALL_EXEC= cp
INSTALL_DATA= cp
#INSTALL_EXEC= install -m 0755
#INSTALL_DATA= install -m 0644

# == END OF USER SETTINGS. NO NEED TO CHANGE ANYTHING BELOW THIS LINE =========

# Convenience platforms targets.
PLATS= ansi bsd generic linux macosx mingw posix

# What to install.
TO_BIN= lua luac
TO_INC= lua.h luaconf.h lualib.h lauxlib.h ../etc/lua.hpp
TO_LIB= liblua.a
TO_MAN= lua.1 luac.1

# Lua version. Currently used only for messages.
V= 5.1

all: $(PLAT)

$(PLATS) clean:
	cd src; $(MAKE) $@

test:	all
	src/lua test/hello.lua

install: all
	cd src; mkdir -p $(INSTALL_BIN) $(INSTALL_INC) $(INSTALL_LIB) $(INSTALL_MAN)
	cd src; $(INSTALL_EXEC) $(TO_BIN) $(INSTALL_BIN)
	cd src; $(INSTALL_DATA) $(TO_INC) $(INSTALL_INC)
	cd src; $(INSTALL_DATA) $(TO_LIB) $(INSTALL_LIB)
	cd doc; $(INSTALL_DATA) $(TO_MAN) $(INSTALL_MAN)

local:
	$(MAKE) install INSTALL_TOP=.. INSTALL_EXEC="cp -p" INSTALL_DATA="cp -p"

none:
	@echo "Please choose a platform: $(PLATS)"

# echo config parameters
echo:
	@echo ""
	@echo "These are the parameters currently set in src/Makefile to build Lua $V:"
	@echo ""
	@cd src; $(MAKE) -s echo
	@echo ""
	@echo "These are the parameters currently set in Makefile to install Lua $V:"
	@echo ""
	@echo "PLAT = $(PLAT)"
	@echo "INSTALL_TOP = $(INSTALL_TOP)"
	@echo "INSTALL_BIN = $(INSTALL_BIN)"
	@echo "INSTALL_INC = $(INSTALL_INC)"
	@echo "INSTALL_LIB = $(INSTALL_LIB)"
	@echo "INSTALL_MAN = $(INSTALL_MAN)"
	@echo "INSTALL_EXEC = $(INSTALL_EXEC)"
	@echo "INSTALL_DATA = $(INSTALL_DATA)"
	@echo ""
	@echo "See also src/luaconf.h ."
	@echo ""

# echo private config parameters
pecho:
	@echo "V = $(V)"
	@echo "TO_BIN = $(TO_BIN)"
	@echo "TO_INC = $(TO_INC)"
	@echo "TO_LIB = $(TO_LIB)"
	@echo "TO_MAN = $(TO_MAN)"

# echo config parameters as Lua code
# uncomment the last sed expression if you want nil instead of empty strings
lecho:
	@echo "-- installation parameters for Lua $V"
	@echo "VERSION = '$V'"
	@$(MAKE) echo | grep = | sed -e 's/= /= "/' -e 's/$$/"/' #-e 's/""/nil/'
	@echo "-- EOF"

# show what has changed since we unpacked
newer:
	@find . -newer MANIFEST -type f

# (end of Makefile)
