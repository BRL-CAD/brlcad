# $Id$

if WITH_JAVA
lib_LTLIBRARIES = librtserver.la
else
lib_LTLIBRARIES =
endif

librtserver_la_SOURCES = rtserver.c

AM_CFLAGS = ${JAVA_CFLAGS}

LIBRTSERVER_VERSION = ${MAJOR_VERSION}:${MINOR_VERSION}:${PATCH_VERSION}

librtserver_la_LDFLAGS = -version-info ${LIBRTSERVER_VERSION} ${JAVA_LDFLAGS}

librtserver_la_LIBADD = ${LIBRT} ${BN} ${BU} ${TCL}

EXTRA_PROGRAMS = rtserverTest

rtserverTest_SOURCES = rtserverTest.c

rtserverTest_LDADD = ${lib_LTLIBRARIES}

include $(top_srcdir)/misc/Makefile.defs

