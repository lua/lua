# makefile for Lua hierarchy
# see INSTALL for installation instructions
# see config for customization instructions

LUA= .

include $(LUA)/config

# primary targets ("co" and "klean" are used for making the distribution)
all clean co klean:	dirs
	cd include; $(MAKE) $@
	cd src; $(MAKE) $@
	cd src/lib; $(MAKE) $@
	cd src/luac; $(MAKE) $@
	cd src/lua; $(MAKE) $@

# in case they were not created during unpacking
dirs:	bin lib

bin lib:
	mkdir -p $@

# simple test to see Lua working
test:	all
	bin/lua test/hello.lua

# remove debug information from binaries
strip:
	$(STRIP) bin/*

# official installation
install: all strip
	mkdir -p $(INSTALL_BIN) $(INSTALL_INC) $(INSTALL_LIB) $(INSTALL_MAN)
	$(INSTALL_EXEC) bin/* $(INSTALL_BIN)
	$(INSTALL_DATA) include/*.h $(INSTALL_INC)
	$(INSTALL_DATA) lib/*.a $(INSTALL_LIB)
	$(INSTALL_DATA) doc/*.1 $(INSTALL_MAN)

# shared libraries (for Linux)
so:
	ld -o lib/liblua.so.$V -shared src/*.o
	ld -o lib/liblualib.so.$V -shared src/lib/*.o
	cd lib; ln -fs liblua.so.$V liblua.so; ln -fs liblualib.so.$V liblualib.so

# binaries using shared libraries
sobin:
	rm -f bin/*
	cd src/lua; $(MAKE)
	cd src/luac; $(MAKE)

# install shared libraries
soinstall:
	$(INSTALL_EXEC) lib/*.so.* $(INSTALL_LIB)
	cd $(INSTALL_LIB); ln -fs liblua.so.$V liblua.so; ln -fs liblualib.so.$V liblualib.so

# clean shared libraries
soclean:
	rm -f lib/*.so* bin/*

# echo config parameters
echo:
	@echo ""
	@echo "These are the parameters currently set in $(LUA)/config to build Lua $V:"
	@echo ""
	@echo "LOADLIB = $(LOADLIB)"
	@echo "DLLIB = $(DLLIB)"
	@echo "NUMBER = $(NUMBER)"
	@echo "POPEN = $(POPEN)"
	@echo "TMPNAM = $(TMPNAM)"
	@echo "DEGREES = $(DEGREES)"
	@echo "USERCONF = $(USERCONF)"
	@echo "CC = $(CC)"
	@echo "WARN = $(WARN)"
	@echo "MYCFLAGS = $(MYCFLAGS)"
	@echo "MYLDFLAGS = $(MYLDFLAGS)"
	@echo "EXTRA_LIBS = $(EXTRA_LIBS)"
	@echo "AR = $(AR)"
	@echo "RANLIB = $(RANLIB)"
	@echo "STRIP = $(STRIP)"
	@echo "INSTALL_ROOT = $(INSTALL_ROOT)"
	@echo "INSTALL_BIN = $(INSTALL_BIN)"
	@echo "INSTALL_INC = $(INSTALL_INC)"
	@echo "INSTALL_LIB = $(INSTALL_LIB)"
	@echo "INSTALL_MAN = $(INSTALL_MAN)"
	@echo "INSTALL_EXEC = $(INSTALL_EXEC)"
	@echo "INSTALL_DATA = $(INSTALL_DATA)"
	@echo ""
	@echo "Edit $(LUA)/config if needed to suit your platform and then run make."
	@echo ""

# turn config into Lua code
# uncomment the last sed expression if you want nil instead of empty strings
lecho:
	@echo "-- $(LUA)/config for Lua $V"
	@echo "VERSION = '$(V)'"
	@make echo | grep = | sed -e 's/= /= "/' -e 's/$$/"/' #-e 's/""/nil/'
	@echo "-- EOF"

newer:
	@find . -newer MANIFEST -type f

# (end of Makefile)
