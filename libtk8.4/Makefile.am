# $Id$

SUBDIRS = xlib bitmaps generic unix doc library

bin_PROGRAMS = wish
#lib_LTLIBRARIES = libtk.la
 
#libtk_la_SOURCES = generic/libtkgeneric.la unix/libtkunix.la
#libtk_la_LIBADD = $(libtk_la_SOURCES)
#libtk_la_LDFLAGS = -version-info 8:4

lib_LTLIBRARIES = libtk8.4.la
 
libtk8_4_la_SOURCES = generic/libtkgeneric.la unix/libtkunix.la
libtk8_4_la_LIBADD = $(libtk8_4_la_SOURCES)
libtk8_4_la_LDFLAGS = -version-info 1

wish_SOURCES = unix/tkAppInit.c
# lib_LTLIBRARIES is a redundancy to help the dependancy graph
wish_LDADD = $(lib_LTLIBRARIES) @TK_LIBS@
wish_CFLAGS = @X_CFLAGS@
