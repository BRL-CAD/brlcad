# $Id$

SUBDIRS = generic doc library

lib_LTLIBRARIES = libitk.la
libitk_la_SOURCES = generic/libitkgeneric.la
libitk_la_LIBADD = $(libitk_la_SOURCES)
libitk_la_CFLAGS = -I${top_srcdir}/libtcl8.4/generic

brlcadlibdir = $(libdir)/itk${ITCL_VERSION}
brlcadlib_DATA = pkgIndex.tcl

pkgIndex.tcl: pkgIndex.tcl.in Makefile.am
	sed -e "s/@VERSION/${ITCL_VERSION}/" -e 's/@RELPATH/../' -e "s/@itk_LIB_FILE/libitk${ITCL_VERSION}.so/" $< | sed 's/@//g' > $@

EXTRA_DIST = pkgIndex.tcl.in
