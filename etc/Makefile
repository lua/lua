# makefile for etc

LUA= ..

include $(LUA)/config

ALL= bin2c min trace

all: $(ALL)

bin2c: bin2c.c
	$(CC) -o $@ $<

min: min.c $(LIB)/liblua.a
	$(CC) $(CFLAGS) -o $@ $< -L$(LIB) -llua

trace: trace.c $(LIB)/liblua.a
	$(CC) $(CFLAGS) -o $@ $< -L$(LIB) -llua

$(LIB)/liblua.a:
	cd ../src; make

clean:
	rm -f $(ALL)
