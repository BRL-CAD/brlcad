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
	win \
	xlib

lib_LTLIBRARIES = \
	libtk.la \
	libtk@TK_VERSION@.la 

libtk_la_SOURCES =
libtk_la_LDFLAGS = -version-info 19:1 -static

# XXX tkPanedWindow.o is a hack to make the IRIX linker behave
libtk_la_LIBADD = unix/libtku.la generic/libtkg.la generic/tkPanedWindow.o ${X_LIBS}

libtk@TK_VERSION@_la_SOURCES = $(libtk_la_SOURCES)
libtk@TK_VERSION@_la_LDFLAGS = $(libtk_la_LDFLAGS)
libtk@TK_VERSION@_la_LIBADD = $(libtk_la_LIBADD)

bin_PROGRAMS = wish wish.static

# moved from unix to this dir
wish_SOURCES =
wish_LDADD = unix/libwish.la libtk.la ${TCL}

wish_static_SOURCES = $(wish_SOURCES)
wish_static_LDADD = $(wish_LDADD)
wish_static_LDFLAGS = -static

tklibdir = ${libdir}

AM_CFLAGS =
AM_CPPFLAGS = \
	-DTK_LIBRARY="\"${TK_LIBRARY}\"" \
	-I${top_srcdir}/src/other/libtcl/generic \
	-I${top_srcdir}/src/other/libtk/bitmaps \
	-I${top_srcdir}/src/other/libtk/generic \
	-I${top_srcdir}/src/other/libtk/unix \
	@X_CFLAGS@

.PHONY : test
test: wish.static
	echo 'puts "hi"' | ./wish.static

MAINTAINERCLEANFILES = Makefile.in
