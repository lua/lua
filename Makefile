# makefile for lua hierarchy

all co clean klean:
	cd include; $(MAKE) $@
	cd src; $(MAKE) $@
	cd src/luac; $(MAKE) $@
	cd src/lib; $(MAKE) $@
	cd src/lua; $(MAKE) $@

strip:
	strip bin/lua*
