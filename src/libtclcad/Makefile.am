# $Id$

lib_LTLIBRARIES = libtclcad.la

AM_CFLAGS = ${X_CFLAGS}

SRCS = \
	tclcad.c \
	tkImgFmtPIX.c

libtclcad_la_LDFLAGS = -version-info 19:1
libtclcad_la_LIBADD = ${BN} ${BU} ${TCL} ${TK}
libtclcad_la_SOURCES = \
	$(SRCS) \
	vers.c

LIBADD = ${TK} ${BN}

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh tclcad_version "BRL-CAD Routines for Tcl/Tk" > $@

DISTCLEANFILES = version

MAINTAINERCLEANFILES = Makefile.in
