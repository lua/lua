# makefile for lua hierarchy

all co clean klean:
	cd src; make $@
	cd src/luac; make $@
	cd include; make $@
	cd clients/lib; make $@
	cd clients/lua; make $@
