# makefile for Lua core library

LUA= ..

include $(LUA)/config

OBJS=	\
	lapi.o \
	lcode.o \
	ldebug.o \
	ldo.o \
	ldump.o \
	lfunc.o \
	lgc.o \
	llex.o \
	lmem.o \
	lobject.o \
	lopcodes.o \
	lparser.o \
	lstate.o \
	lstring.o \
	ltable.o \
	ltests.o \
	ltm.o \
	lundump.o \
	lvm.o \
	lzio.o

SRCS=	\
	lapi.c \
	lcode.c \
	ldebug.c \
	ldo.c \
	ldump.c \
	lfunc.c \
	lgc.c \
	llex.c \
	lmem.c \
	lobject.c \
	lopcodes.c \
	lparser.c \
	lstate.c \
	lstring.c \
	ltable.c \
	ltests.c \
	ltm.c \
	lundump.c \
	lvm.c \
	lzio.c \
	lapi.h \
	lcode.h \
	ldebug.h \
	ldo.h \
	lfunc.h \
	lgc.h \
	llex.h \
	llimits.h \
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

all:	$T

$T:	$(OBJS)
	$(AR) $@ $(OBJS)
	$(RANLIB) $@

clean:
	rm -f $(OBJS) $T

co:
	co -q -f -M $(SRCS)

klean:	clean
	rm -f $(SRCS)
