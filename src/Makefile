# makefile for lua distribution (main library)

LUA= ..

include $(LUA)/config

OBJS=	auxlib.o\
	fallback.o\
	func.o\
	hash.o\
	inout.o\
	lex.o\
	luamem.o\
	opcode.o\
	parser.o\
	table.o\
	tree.o\
	undump.o\
	zio.o

SRCS=	auxlib.c\
	auxlib.h\
	fallback.c\
	fallback.h\
	func.c\
	func.h\
	hash.c\
	hash.h\
	inout.c\
	inout.h\
	lex.c\
	lex.h\
	luamem.c\
	luamem.h\
	opcode.c\
	opcode.h\
	parser.c\
	parser.h\
	table.c\
	table.h\
	tree.c\
	tree.h\
	types.h\
	undump.c\
	undump.h\
	zio.c\
	zio.h\
	lua.stx

SLIB= $(LIB)/liblua.a

all: $(SLIB)

$(SLIB): $(OBJS)
	ar rcu $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f $(OBJS) $(SLIB)

co:
	co -f -M $(SRCS)

parser:
	yacc -d lua.stx
	rm -f parser.c parser.h
	sed -e 's/yy/luaY_/g' y.tab.c > parser.c
	sed -e 's/yy/luaY_/g' y.tab.h > parser.h
	rm y.tab.c y.tab.h

klean:	clean
	rm -f $(SRCS)
