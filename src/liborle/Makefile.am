
lib_LTLIBRARIES = liborle.la
man_MANS = liborle.3

liborle_la_SOURCES = rle.c
liborle_la_LDFLAGS = -version-info 19:1 -no-undefined

AM_CFLAGS = ${STRICT_FLAGS}

EXTRA_DIST = $(man_MANS) CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs
