# $Id$

lib_LTLIBRARIES = libmultispectral.la

AM_CFLAGS = -DRT_MULTISPECTRAL

libmultispectral_la_LDFLAGS = -version-info 19:1
libmultispectral_la_LIBADD = \
	${top_builddir}/src/liboptical/librtms.la \
	${RT} \
	${BN} \
	${BU} \
	${TCL}
libmultispectral_la_SOURCES = \
	init.c \
	sh_temp.c \
	vers.c

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	sh $(top_srcdir)/sh/vers.sh libmultispectral_version "The BRL-CAD MultiSpectral Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

