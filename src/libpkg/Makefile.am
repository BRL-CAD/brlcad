# $Id$

lib_LTLIBRARIES = libpkg.la

noinst_PROGRAMS = tpkg

AM_CPPFLAGS = ${TCL_CPPFLAGS}

SRCS = \
	pkg.c \
	tcl.c

libpkg_la_SOURCES = \
	$(SRCS) \
	vers.c

libpkg_la_LDFLAGS = -version-info 19:1

tpkg_SOURCES = tpkg.c
tpkg_LDADD = \
	libpkg.la \
	${BU} ${BU_LIBS}

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh pkg_version "The BRL-CAD Network Package (PKG) Library" > $@

sample_applicationsdir=$(SAMPLE_APPLICATIONS_DIR)
sample_applications_DATA = tpkg.c

EXTRA_DIST = \
	libpkg.dsp \
	vers_win.c

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

DEPS=src/libbu

include $(top_srcdir)/misc/Makefile.defs
