# $Id$

bin_PROGRAMS = rleClock

rleClock_SOURCES = \
	font.c \
	rleClock.c

noinst_HEADERS = font.h

font.c: ${srcdir}/makeFont.sh ${srcdir}/font.src
	@SH@ ${srcdir}/makeFont.sh ${srcdir}/font.src > font.c

LDADD = ${RLE}

EXTRA_DIST = \
	README \
	font.src \
	makeFont

MAINTAINERCLEANFILES = Makefile.in
