# $Id$

noinst_LTLIBRARIES = libitclgeneric.la

libitclgeneric_la_SOURCES = itcl_bicmds.c itcl_class.c itcl_cmds.c itcl_ensemble.c itcl_linkage.c itcl_methods.c itcl_migrate.c itcl_objects.c itcl_obsolete.c itcl_parse.c itclStubInit.c itclStubLib.c itcl_util.c
brlcadincludedir = $(includedir)/brlcad
include_HEADERS = itclDecls.h itcl.h itclIntDecls.h itclInt.h
brlcadinclude_HEADERS = $(include_HEADERS)

libitclgeneric_la_CFLAGS = -I${top_srcdir}/libtcl8.4/generic
