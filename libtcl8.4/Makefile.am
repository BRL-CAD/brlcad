# $Id$

SUBDIRS = generic unix doc library

bin_PROGRAMS = tclsh
#lib_LTLIBRARIES = libtcl.la
 
#libtcl_la_SOURCES = generic/libtclgeneric.la unix/libtclunix.la
#libtcl_la_LIBADD = $(libtcl_la_SOURCES)
#libtcl_la_LDFLAGS = -version-info 1 -release 8.4

# Ugh.
lib_LTLIBRARIES = libtcl8.4.la
libtcl8_4_la_SOURCES = generic/libtclgeneric.la unix/libtclunix.la
libtcl8_4_la_LIBADD = $(libtcl8_4_la_SOURCES)
libtcl8_4_la_LDFLAGS = -version-info 1

tclsh_SOURCES = unix/tclAppInit.c
tclsh_LDADD = libtcl.la -lm @DL@
