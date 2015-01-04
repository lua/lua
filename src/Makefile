# makefile for lua basic library

LUA= ..

include $(LUA)/config

OBJS=	\
	lapi.o \
	lauxlib.o \
	lbuffer.o \
	lbuiltin.o \
	ldo.o \
	lfunc.o \
	lgc.o \
	llex.o \
	lmem.o \
	lobject.o \
	lparser.o \
	lstate.o \
	lstring.o \
	ltable.o \
	ltm.o \
	lundump.o \
	lvm.o \
	lzio.o

SRCS=	\
	lapi.c \
	lauxlib.c \
	lbuffer.c \
	lbuiltin.c \
	ldo.c \
	lfunc.c \
	lgc.c \
	llex.c \
	lmem.c \
	lobject.c \
	lparser.c \
	lstate.c \
	lstring.c \
	ltable.c \
	ltm.c \
	lundump.c \
	lvm.c \
	lzio.c \
	lapi.h \
	lbuiltin.h \
	ldo.h \
	lfunc.h \
	lgc.h \
	llex.h \
	lmem.h \
	lobject.h \
	lopcodes.h \
	lparser.h \
	lstate.h \
	lstring.h \
	ltable.h \
	ltm.h \
	lundump.h \
	lvm.h \
	lzio.h

T= $(LIB)/liblua.a

all: $T

$T: $(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f $(OBJS) $T

co:
	co -q -f -M $(SRCS)

klean:	clean
	rm -f $(SRCS)
