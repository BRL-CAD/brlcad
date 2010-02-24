
lib_LTLIBRARIES = libgcv.la
noinst_LTLIBRARIES = libgcv_nil.la

libgcv_nil_la_SOURCES = \
	region_end.c \
	region_end_mc.c

AM_CPPFLAGS = ${TCL_CPPFLAGS}

libgcv_la_SOURCES =
libgcv_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
libgcv_la_LIBADD = \
	libgcv_nil.la \
	${GCV_LIBS}

EXTRA_DIST = \
        CMakeLists.txt

AM_CFLAGS = ${STRICT_FLAGS}

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libgcv_nil_la_OBJECTS) \
	$(libgcv_la_OBJECTS) \
	$(lib_LTLIBRARIES)
