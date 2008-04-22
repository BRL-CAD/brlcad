# $Id$

lib_LTLIBRARIES = libz.la
noinst_PROGRAMS = \
	example \
	minigzip

libz_la_SOURCES = \
	adler32.c \
	compress.c \
	crc32.c \
	deflate.c \
	gzio.c \
	infback.c \
	inffast.c \
	inflate.c \
	inftrees.c \
	trees.c \
	uncompr.c \
	zutil.c

include_HEADERS = \
	crc32.h \
	deflate.h \
	inffast.h \
	inffixed.h \
	inflate.h \
	inftrees.h \
	trees.h \
	zconf.h \
	zlib.h \
	zutil.h

libz_la_LDFLAGS = -version-info 2:4:1

man_MANS = zlib.3

example_SOURCES = example.c
example_LDADD = libz.la

minigzip_SOURCES = minigzip.c
minigzip_LDADD = libz.la

EXTRA_DIST = \
	$(man_MANS) \
        CMakeLists.txt \
	FAQ \
	INDEX \
	Makefile.in.orig \
	Makefile.orig \
	algorithm.txt \
	amiga \
	as400 \
	configure \
	contrib \
	examples \
	make_vms.com \
	minigzip.c \
	msdos \
	old \
	projects \
	qnx \
	win32 \
	zconf.in.h

include $(top_srcdir)/misc/Makefile.defs

