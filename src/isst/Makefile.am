pkglibdir = $(libdir)/isst
pkglib_LTLIBRARIES = issttcltk.la

issttcltk_la_SOURCES = \
	isst_tcltk.c

issttcltk_la_LIBADD = ${TIE} ${RENDER} ${TCL} ${TK} -L$(top_builddir)/src/other/togl -lToglstub2.1
issttcltk_la_CFLAGS = -I$(top_srcdir)/src/adrt -I$(top_srcdir)/src/adrt/libtie -I$(top_srcdir)/src/adrt/librender  -DTIE_PRECISION=0 -DHAX=1  ${TCL_CPPFLAGS} ${TK_CPPFLAGS} -I$(top_srcdir)/src/other/togl -I$(top_builddir)/src/other/togl ${GL_CPPFLAGS}
issttcltk_la_LDFLAGS = -module -shrext .plugin

pkgIndex.tcl:
	echo 'package ifneeded isst 0.1 [list load [file join $(pkglibdir) issttcltk.plugin] isst]' > pkgIndex.tcl

issttcltkscriptdir = $(pkglibdir)

issttcltkscript_SCRIPTS = pkgIndex.tcl

EXTRA_DIST = \
	gears.tcl

DEPENDS = src/adrt

include $(top_srcdir)/misc/Makefile.defs
