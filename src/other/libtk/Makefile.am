# $Id$

SUBDIRS = \
	doc \
	generic \
	unix

EXTRA_DIST = \
	README \
	bitmaps \
	changes \
	compat \
	library \
	license.terms \
	mac \
	macosx \
	tests \
	unix \
	win \
	xlib

MAINTAINERCLEANFILES = \
	Makefile.in

lib_LTLIBRARIES = libtk@TK_VERSION@.la

libtk@TK_VERSION@_la_SOURCES =
libtk@TK_VERSION@_la_LDFLAGS = -version-info 19:1

libtk@TK_VERSION@_la_LIBADD = \
	generic/libtkgeneric.la \
	unix/libtkunix.la
