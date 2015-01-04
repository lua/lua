# makefile for etc

LUA= ..

include $(LUA)/config

LIBLUA=$(LIB)/liblua.a
ALL= bin2c min trace lua.def

x:
	@echo 'choose a target:' all $(ALL)

all: $(ALL)

bin2c:	bin2c.c
	$(CC) $(CFLAGS) -o $@ $@.c

min:	min.c $(LIBLUA)
	$(CC) $(CFLAGS) -o $@ $@.c -L$(LIB) -llua

trace:	trace.c $(LIBLUA)
	$(CC) $(CFLAGS) -o $@ $@.c -L$(LIB) -llua -llualib -lm

def:	lua.def

lua.def: $(INC)/lua.h
	$(BIN)/lua def.lua < $(INC)/lua.h > $@
	# cat $(INC)/l*.h | $(BIN)/lua def.lua > $@

stdcall:
	mkdir -p Stdcall
	grep -l _API $(LUA)/src/*.[ch] $(LUA)/src/*/*.[ch] | xargs -n1 -i echo $(BIN)/lua stdcall.lua '<{}' '>Stdcall/{}'

$(LIBLUA):
	cd ../src; $(MAKE)

clean:
	rm -f $(ALL)
