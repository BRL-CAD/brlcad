# $Id$

lib_LTLIBRARIES = librtserver.la
MAJOR=0
MINOR=0
PATCH=0
VERSION = $(MAJOR):$(PATCH):$(MINOR)
 
librtserver_la_SOURCES = rtserver.c

AM_CFLAGS = -I$(top_srcdir)/h
librtserver_la_LDFLAGS = -version-info $(VERSION)
 
