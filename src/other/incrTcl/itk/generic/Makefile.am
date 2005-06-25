# $Id$

lib_LTLIBRARIES = \
	libitkstub.la \
	libitkstub@ITCL_VERSION@.la

noinst_LTLIBRARIES = libitg.la

libitkstub_la_SOURCES = itkStubLib.c
libitkstub@ITCL_VERSION@_la_SOURCES = $(libitkstub_la_SOURCES)

libitg_la_SOURCES = \
	itkStubInit.c \
	itk_archetype.c \
	itk_cmds.c \
	itk_option.c \
	itk_util.c

include_HEADERS = \
	itk.h \
	itkDecls.h

EXTRA_DIST = itk.decls

MAINTAINERCLEANFILES = Makefile.in

AM_CFLAGS=@X_CFLAGS@

include $(top_srcdir)/misc/Makefile.defs

