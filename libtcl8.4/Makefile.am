# $Id$

SUBDIRS = generic unix doc library

bin_PROGRAMS = tclsh
lib_LTLIBRARIES = libtcl.la
 
libtcl_la_SOURCES = generic/libtclgeneric.la unix/libtclunix.la
libtcl_la_LIBADD = $(libtcl_la_SOURCES)

tclsh_SOURCES = unix/tclAppInit.c
tclsh_LDADD = libtcl.la -lm @DL@
