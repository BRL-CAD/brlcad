# $Id$

lib_LTLIBRARIES = libtclcad.la

SRCS = \
	tclcad.c \
	tkImgFmtPIX.c

libtclcad_la_SOURCES = \
	$(SRCS) \
	vers.c

libtclcad_la_LDFLAGS = -version-info 19:1

LIBADD = @TK_LIBS@ @BN@

AM_CFLAGS = @X_CFLAGS@

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh tclcad_version "BRL-CAD Routines for Tcl/Tk" > $@
