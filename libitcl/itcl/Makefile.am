# $Id$

SUBDIRS = generic doc library

lib_LTLIBRARIES = libitcl.la
 
libitcl_la_SOURCES = generic/libitclgeneric.la
libitcl_la_LIBADD = $(libitcl_la_SOURCES)

brlcadlibdir = $(libdir)/itcl${ITCL_VERSION}
brlcadlib_DATA = pkgIndex.tcl

libitclgeneric_la_CFLAGS = -I${top_srcdir}/libtcl8.4/generic

VERSION=${ITCL_VERSION}
RELPATH=..
itcl_LIB_FILE=libitcl${ITCL_VERSION}.so
pkgIndex.tcl: pkgIndex.tcl.in Makefile.am
	sed -e "s/@VERSION/${ITCL_VERSION}/" -e 's/@RELPATH/../' -e "s/@itcl_LIB_FILE/libitcl${ITCL_VERSION}.so/" $< | sed 's/@//g' > $@

EXTRA_DIST = pkgIndex.tcl.in
