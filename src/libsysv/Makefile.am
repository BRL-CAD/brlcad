# $Id$

lib_LTLIBRARIES = libsysv.la

libsysv_la_SOURCES = \
	redata.c \
	regex.c \
	vers.c

libsysv_la_LDFLAGS = -version-info 19:1

noinst_HEADERS = \
	cclass.h \
	cname.h \
	regex2.h \
	utils.h

BUILT_SOURCES = vers.c
vers.c:
	@SH@ $(top_srcdir)/sh/vers.sh libsysv_version "BRL-CAD System V Compatibility Library" > $@
