# $Id$

lib_LTLIBRARIES = libz.la

libz_la_SOURCES = \
	adler32.c \
	compress.c \
	crc32.c \
	deflate.c \
	example.c \
	gzio.c \
	infblock.c \
	infcodes.c \
	inffast.c \
	inflate.c \
	inftrees.c \
	infutil.c \
	trees.c \
	uncompr.c \
	zutil.c

libz_la_LDFLAGS = -version-info 2:4:1

include_HEADERS = \
	deflate.h \
	infblock.h \
	infcodes.h \
	inffast.h \
	inffixed.h \
	inftrees.h \
	infutil.h \
	trees.h \
	zconf.h \
	zlib.h \
	zutil.h

man_MANS = zlib.3

EXTRA_DIST = \
	$(man_MANS) \
	FAQ \
	INDEX \
	Make_vms.com \
	Makefile.riscos \
	afile \
	algorithm.txt \
	amiga \
	contrib \
	descrip.mms \
	maketree.c \
	minigzip.c \
	msdos \
	nt \
	sfile \
	zlib.html	

MAINTAINERCLEANFILES = Makefile.in
