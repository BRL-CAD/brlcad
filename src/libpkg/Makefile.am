
lib_LTLIBRARIES = libpkg.la

noinst_PROGRAMS = tpkg

AM_CPPFLAGS = ${TCL_CPPFLAGS}

AM_CFLAGS = ${STRICT_FLAGS}

libpkg_la_SOURCES = \
	pkg.c \
	tcl.c \
	vers.c

libpkg_la_LDFLAGS = -version-info 19:1 -no-undefined
libpkg_la_LIBADD = ${PKG_LIBS}

tpkg_SOURCES = tpkg.c
tpkg_LDADD = \
	libpkg.la \
	${BU} \
	${BU_LIBS}

sample_applicationsdir=$(SAMPLE_APPLICATIONS_DIR)
sample_applications_DATA = tpkg.c

EXTRA_DIST = \
	CMakeLists.txt \
	example

DEPENDS = src/libbu

include $(top_srcdir)/misc/Makefile.defs
