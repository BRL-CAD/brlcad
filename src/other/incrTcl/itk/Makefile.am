# $Id$

SUBDIRS = library

lib_LTLIBRARIES = \
	libitk3.3.la \
	libitkstub3.3.la

libitkstub3_3_la_SOURCES = \
	generic/itkStubLib.c

libitk3_3_la_SOURCES = \
	generic/itk_cmds.c \
	generic/itk_option.c \
	generic/itk_archetype.c \
	generic/itk_util.c \
	generic/itkStubInit.c

include_HEADERS = \
	generic/itk.h \
	generic/itkDecls.h

AM_CFLAGS = ${X_CFLAGS}

MAINTAINERCLEANFILES = \
	Makefile.in

pkglibdir = $(libdir)/itk3.3

install-data-hook:
	@mkdir -p $(DESTDIR)$(pkglibdir)
	$(INSTALL_DATA) $(top_builddir)/src/other/incrTcl/itk/pkgIndex.tcl $(DESTDIR)$(pkglibdir)/. 

EXTRA_DIST = pkgIndex.tcl
