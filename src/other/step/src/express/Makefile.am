lib_LTLIBRARIES = libexpress.la
noinst_LTLIBRARIES = libexpress_nil.la

AM_CFLAGS = -I$(top_srcdir)/include/express
AM_LFLAGS = -l

bin_PROGRAMS = \
	fedex \
	symlink

COMMON = \
	expscan.l \
	expparse.y \
	symbol.c \
	type.c \
	variable.c \
	expr.c \
	entity.c \
	caseitem.c \
	stmt.c \
	alg.c \
	scope.c \
	schema.c \
	resolve.c \
	lexact.c \
	linklist.c \
	error.c \
	dict.c \
	hash.c \
	memory.c \
	object.c \
	inithook.c \
	yyvars.c \
	express.c

fedex_SOURCES = \
	fedex.c 

symlink_SOURCES = \
	symlink.c 

fedex_LDADD = ${EXPRESS}

symlink_LDADD = ${EXPRESS} fedex.o

libexpress_nil_la_SOURCES = \
	$(COMMON)

libexpress_nil_la_LDFLAGS =
libexpress_nil_la_LIBADD = 

libexpress_la_SOURCES = 
libexpress_la_LDFLAGS =
libexpress_la_LIBADD = \
	libexpress_nil.la

noinst_HEADERS = \
	conf.h \
	exptoks.h \
	stack.h \
	y.tok.h

expparse.h: expparse.y
express.lo: expparse.h
fedex.lo: expparse.h

BUILT_SOURCES = \
	expparse.c \
	expparse.h \
	expscan.c

EXTRA_DIST = \
	README \
	Changes

DISTCLEANFILES = $(BUILT_SOURCES)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libexpress_la_OBJECTS)
