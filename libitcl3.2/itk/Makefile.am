# $Id$

SUBDIRS = generic unix doc library

lib_LTLIBRARIES = libitk.la
libitk_la_SOURCES = generic/libitkgeneric.la unix/libitkunix.la
libitk_la_LIBADD = $(libitk_la_SOURCES)
libitk_la_CFLAGS = -I${top_srcdir}/libtcl8.4/generic

