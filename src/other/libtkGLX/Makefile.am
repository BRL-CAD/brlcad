# $Id$

lib_LTLIBRARIES = libtkGLX.la

libtkGLX_la_SOURCES = \
	tkGLXCmds.c\
	tkGLXInit.c\
	tkGLXwin.c

noinst_HEADERS = \
	descnames.h\
	patchlevel.h\
	tkGLX.h

libtkGLX_la_LDFLAGS = -version-info 19:1 

descnames.h: 
	sh makedescnames.sh

EXTRA_DIST = makedescnames.sh

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

