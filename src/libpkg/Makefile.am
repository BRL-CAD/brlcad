# $Id$

lib_LTLIBRARIES = libpkg.la

SRCS = \
	pkg.c \
	tcl.c

libpkg_la_SOURCES = \
	$(SRCS) \
	vers.c

libpkg_la_LDFLAGS = -version-info 19:1

AM_CFLAGS = -I$(top_srcdir)/h

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	sh $(top_srcdir)/sh/vers.sh pkg_version "The BRL-CAD Network Package (PKG) Library" > $@
