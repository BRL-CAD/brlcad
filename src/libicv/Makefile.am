lib_LTLIBRARIES = libicv.la
noinst_LTLIBRARIES = libicv_nil.la

AM_CPPFLAGS = ${TCL_CPPFLAGS}

libicv_nil_la_SOURCES = \
	rot.c

EXTRA_DIST = \
	CMakeLists.txt

libicv_la_SOURCES =
libicv_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
libicv_la_LIBADD = \
	libicv_nil.la \
	${ICV_LIBS}

AM_CFLAGS = ${STRICT_FLAGS}

DEPENDS = \
	src/libbu \
	src/libbn

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libicv_nil_la_OBJECTS) \
	$(libicv_la_OBJECTS) \
	$(lib_LTLIBRARIES)
