# makefile for lua hierarchy

all co clean klean:
	cd include; make $@
	cd src; make $@
	cd src/luac; make $@
	cd clients/lib; make $@
	cd clients/lib/old; make $@
	cd clients/lua; make $@

old:
	cd clients/lib/old; make $@
