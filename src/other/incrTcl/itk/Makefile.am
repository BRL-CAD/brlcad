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

brlcadscriptdir = $(libdir)/itk3.3

brlcadscript_SCRIPTS = pkgIndex.tcl

EXTRA_DIST = $(brlcadscript_SCRIPTS)
