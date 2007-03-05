# $Id$

lib_LTLIBRARIES = \
	libitkstub.la \
	libitkstub@ITCL_VERSION@.la

noinst_LTLIBRARIES = libitg.la

AM_CFLAGS = ${X_CFLAGS}
# make sure we look in tcl's directory so we find private headers
AM_CPPFLAGS = \
	${TCL_CPPFLAGS} \
	${ITCL_CPPFLAGS} \
	${TK_CPPFLAGS} \
	-I$(top_srcdir)/src/other/tcl/generic \
	-I$(top_srcdir)/src/other/tcl/unix \
	-I$(top_srcdir)/src/other/tk/generic \
	-I$(top_srcdir)/src/other/tk/unix

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

include $(top_srcdir)/misc/Makefile.defs

