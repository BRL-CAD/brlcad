# $Id$
lib_LTLIBRARIES = libtclcad.la

SRCS = tclcad.c tkImgFmtPIX.c
libtclcad_la_SOURCES = $(SRCS) vers.c
libtclcad_la_LDFLAGS = -version-info 19:1
LIBADD = @TK_LIBS@ @BN@

AM_CFLAGS = -I$(top_srcdir)/h

BUILT_SOURCES = vers.c
vers.c: $(SRCS)
	$(top_srcdir)/sh/newvers.sh tclcad_version libtclcad

