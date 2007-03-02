# $Id$

if WITH_JAVA
lib_LTLIBRARIES = librtserver.la
else
lib_LTLIBRARIES =
endif

librtserver_la_SOURCES = rtserver.c

AM_CFLAGS = ${JAVA_CFLAGS}
AM_CPPFLAGS = ${TCL_CPPFLAGS}

LIBRTSERVER_VERSION = ${MAJOR_VERSION}:${MINOR_VERSION}:${PATCH_VERSION}

librtserver_la_LDFLAGS = -version-info ${LIBRTSERVER_VERSION} ${JAVA_LDFLAGS} -module

if BUILD_OPENNURBS
ON_NIL = $(top_builddir)/src/other/openNURBS/libopenNURBS_nil.la
else
ON_NIL =
endif

librtserver_la_LIBADD = \
	$(top_builddir)/src/librt/librt_nil.la \
	$(top_builddir)/src/libbn/libbn_nil.la \
	$(top_builddir)/src/libbu/libbu_nil.la \
	${ON_NIL} \
	${TCL}

EXTRA_PROGRAMS = rtserverTest

rtserverTest_SOURCES = rtserverTest.c

rtserverTest_LDADD = ${lib_LTLIBRARIES}

DEPS=src/librt

include $(top_srcdir)/misc/Makefile.defs
