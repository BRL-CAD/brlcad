# $Id$

if JAVA
lib_LTLIBRARIES = librtserver.la
else
lib_LTLIBRARIES =
endif

MAJOR=0
MINOR=0
PATCH=1
VERSION = $(MAJOR):$(PATCH):$(MINOR)
 
librtserver_la_SOURCES = rtserver.c

AM_CFLAGS = ${JAVA_CFLAGS}

librtserver_la_LDFLAGS = -version-info $(VERSION) ${JAVA_LDFLAGS}

librtserver_la_LIBADD = ${RT} ${BN} ${BU} ${TCL}
