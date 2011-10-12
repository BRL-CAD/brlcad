lib_LTLIBRARIES = libexpress.la
noinst_LTLIBRARIES = libexpress_nil.la

AM_LFLAGS = -l
AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	${NOWARN}

bin_PROGRAMS = \
	fedex \
	symlink

SRCS = \
	expscan.re \
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

fedex_SOURCES = fedex.c 
fedex_LDADD = libexpress.la

symlink_SOURCES = \
	fedex.c \
	symlink.c
symlink_LDADD = libexpress.la

libexpress_nil_la_SOURCES = $(SRCS)
libexpress_nil_la_LDFLAGS = -avoid-version

libexpress_la_SOURCES = 
libexpress_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
libexpress_la_LIBADD = libexpress_nil.la

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
	$(BUILT_SOURCES) \
	README \
	Changes \
	CMakeLists.txt

DISTCLEANFILES = $(BUILT_SOURCES)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libexpress_la_OBJECTS)
