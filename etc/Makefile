# makefile for Lua etc

LUA= ..

include $(LUA)/config

ALL= min noparser luab 1

all:
	@echo 'choose a target:' $(ALL)

min:	min.c $(LIBLUA)
	$(CC) $(CFLAGS) -o $@ $@.c -L$(LIB) -llua -llualib

noparser: noparser.c
	$(CC) $(CFLAGS) -I$(LUA)/src -o $@.o -c $@.c

luab:	noparser
	$(CC) -o $@ noparser.o $(LUA)/src/lua/lua.o -L$(LIB) -llua -llualib $(EXTRA_LIBS) $(DLLIB)
	$(BIN)/luac $(LUA)/test/hello.lua
	$@ luac.out
	-$@ -e'a=1'

1:
	$(CC) $(CFLAGS) -I../src -I../src/lib -I../src/lua all.c $(EXTRA_LIBS) $(DLLIB)
	./a.out $(LUA)/test/hello.lua

flat:
	cd ..; mkdir flat; mv include/*.h src/*.[ch] src/*/*.[ch] flat

clean:
	rm -f $(ALL) a.out core *.o luac.out
