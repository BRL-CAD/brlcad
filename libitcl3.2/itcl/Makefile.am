# $Id$

SUBDIRS = generic unix doc library

lib_LTLIBRARIES = libitcl.la
 
libitcl_la_SOURCES = generic/libitclgeneric.la unix/libitclunix.la
libitcl_la_LIBADD = $(libitcl_la_SOURCES)

brlcadlibdir = $(libdir)/itcl3.2
brlcadlib_DATA = pkgIndex.tcl

libitclgeneric_la_CFLAGS = -I${top_srcdir}/libtcl8.4/generic

VERSION=3.2
RELPATH=..
itcl_LIB_FILE=libitcl3.2.so
pkgIndex.tcl: pkgIndex.tcl.in Makefile.am
	sed -e 's/@VERSION/3.2/' -e 's/@RELPATH/../' -e 's/@itcl_LIB_FILE/libitcl3.2.so/' $< | sed 's/@//g' > $@

EXTRA_DIST = pkgIndex.tcl.in
