
lib_LTLIBRARIES = libanalyze.la
noinst_LTLIBRARIES = libanalyze_nil.la

libanalyze_nil_la_SOURCES = \
	density.c \
	overlaps.c

AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

libanalyze_la_SOURCES =
libanalyze_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
libanalyze_la_LIBADD = \
	libanalyze_nil.la \
	${ANALYZE_LIBS}

EXTRA_DIST = \
        CMakeLists.txt

AM_CFLAGS = ${STRICT_FLAGS}

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libanalyze_nil_la_OBJECTS) \
	$(libanalyze_la_OBJECTS) \
	$(lib_LTLIBRARIES)
