# $Id$

SUBDIRS = \
	doc \
	generic \
	library \
	unix

EXTRA_DIST = \
	compat \
	mac \
	macosx \
	tests \
	tools \
	win

MAINTAINERCLEANFILES = \
	Makefile.in

lib_LTLIBRARIES = libtcl@TCL_VERSION@.la libtcl.la

libtcl@TCL_VERSION@_la_SOURCES = 

libtcl@TCL_VERSION@_la_LDFLAGS = -version-info 19:1

libtcl@TCL_VERSION@_la_LIBADD = \
	generic/libtclgeneric.la \
	unix/libtclunix.la \
	${LIBZ} \
	${LIBM}

libtcl_la_SOURCES = 

libtcl_la_LDFLAGS = -version-info 19:1

libtcl_la_LIBADD = \
	generic/libtclgeneric.la \
	unix/libtclunix.la \
	${LIBZ} \
	${LIBM}

bin_PROGRAMS = tclsh

tclsh_SOURCES = unix/tclAppInit.c

tclsh_LDADD = $(lib_LTLIBRARIES) ${TCL}
