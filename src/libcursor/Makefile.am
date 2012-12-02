
lib_LTLIBRARIES = libcursor.la

libcursor_la_SOURCES = cursor.c

libcursor_la_LDFLAGS = -version-info 20:1 -no-undefined
libcursor_la_LIBADD = ${CURSOR_LIBS}

AM_CPPFLAGS = ${TERMLIB_CPPFLAGS}
AM_CFLAGS = ${STRICT_FLAGS}

EXTRA_DIST = CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs
