# $Id$

lib_LTLIBRARIES = libpkg.la

SRCS = \
	pkg.c \
	tcl.c

libpkg_la_SOURCES = \
	$(SRCS) \
	vers.c

libpkg_la_LDFLAGS = -version-info 19:1

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh pkg_version "The BRL-CAD Network Package (PKG) Library" > $@

EXTRA_DIST = \
	libpkg.dsp \
	vers_win.c

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

