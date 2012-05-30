lib_LTLIBRARIES = libgcv.la
noinst_LTLIBRARIES = libgcv_nil.la

AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

libgcv_nil_la_SOURCES = \
	region_end.c \
	region_end_mc.c \
	bottess.c \
	tri_intersect.c

EXTRA_DIST = \
	CMakeLists.txt \
	NOTES \
	test_bottess.c \
	wfobj

libgcv_la_SOURCES =
libgcv_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
libgcv_la_LIBADD = \
	libgcv_nil.la \
	${GCV_LIBS}

noinst_HEADERS = soup.h tri_intersect.h

AM_CFLAGS = ${STRICT_FLAGS}

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libgcv_nil_la_OBJECTS) \
	$(libgcv_la_OBJECTS) \
	$(lib_LTLIBRARIES)
