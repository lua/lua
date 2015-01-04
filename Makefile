# makefile for lua hierarchy

all co clean klean:
	cd include; make $@
	cd src; make $@
	cd src/luac; make $@
	cd src/lib; make $@
	cd src/lua; make $@
