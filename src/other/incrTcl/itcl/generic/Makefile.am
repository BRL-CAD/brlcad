# $Id$

lib_LTLIBRARIES = \
	libitclstub.la \
	libitclstub@ITCL_VERSION@.la

noinst_LTLIBRARIES = libitg.la

# make sure we look in tcl's directory so we find private headers
AM_CPPFLAGS = \
	${TCL_CPPFLAGS} \
	-I$(top_srcdir)/src/other/tcl/generic \
	-I$(top_srcdir)/src/other/tcl/unix

libitclstub_la_SOURCES = itclStubLib.c
libitclstub@ITCL_VERSION@_la_SOURCES = $(libitclstub_la_SOURCES)

libitg_la_SOURCES = \
	itclStubInit.c \
	itcl_bicmds.c \
	itcl_class.c \
	itcl_cmds.c \
	itcl_ensemble.c \
	itcl_linkage.c \
	itcl_methods.c \
	itcl_migrate.c \
	itcl_objects.c \
	itcl_parse.c \
	itcl_util.c

include_HEADERS = \
	itcl.h \
	itclDecls.h \
	itclInt.h \
	itclIntDecls.h

EXTRA_DIST = itcl.decls

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

