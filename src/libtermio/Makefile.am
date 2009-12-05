
lib_LTLIBRARIES = libtermio.la

libtermio_la_SOURCES = termio.c
libtermio_la_LDFLAGS = -version-info 19:1 -no-undefined
libtermio_la_LIBADD = ${TERMIO_LIBS}

AM_CFLAGS = ${STRICT_FLAGS}

EXTRA_DIST = \
	libtermio.dsp \
	termio_win32.c

include $(top_srcdir)/misc/Makefile.defs
