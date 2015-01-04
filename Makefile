# makefile for lua hierarchy

all:
	(cd src; make)
	(cd clients/lib; make)
	(cd clients/lua; make)

clean:
	(cd src; make clean)
	(cd clients/lib; make clean)
	(cd clients/lua; make clean)
