# $Id$

SUBDIRS = \
	doc \
	generic \
	unix

EXTRA_DIST = \
	compat \
	library \
	mac \
	macosx \
	tests \
	tools \
	win

MAINTAINERCLEANFILES = \
	Makefile.in

lib_LTLIBRARIES = libtcl@TCL_VERSION@.la

libtcl@TCL_VERSION@_la_SOURCES = 
libtcl@TCL_VERSION@_la_LDFLAGS = -version-info 19:1

libtcl@TCL_VERSION@_la_LIBADD = \
	generic/libtclgeneric.la \
	unix/libtclunix.la \
	${LIBZ} \
	${LIBM}
