# $Id$

SUBDIRS = \
	doc \
	generic \
	library \
	unix

EXTRA_DIST = \
	README \
	bitmaps \
	changes \
	compat \
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
	unix/libtkunix.la \
	${TCLSTUB} \
	${X_LIBS} \
	${LIBM}

bin_PROGRAMS = wish

wish_SOURCES = unix/tkAppInit.c

wish_LDADD = \
	$(lib_LTLIBRARIES) \
	${TCL}

wish_CFLAGS = ${X_CFLAGS}
