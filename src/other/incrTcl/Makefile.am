# $Id$

SUBDIRS = \
	doc \
	itcl \
	itk \
	tools

EXTRA_DIST = \
	CHANGES \
	INCOMPATIBLE \
	license.terms \
	makefile.bc \
	makefile.vc \
	rules.vc \
	tcl.m4

lib_LTLIBRARIES = \
	libitcl.la \
	libitcl@ITCL_VERSION@.la \
	libitk.la \
	libitk@ITCL_VERSION@.la

libitcl_la_LIBADD = \
	itcl/generic/itclStubInit.lo \
	itcl/generic/itcl_bicmds.lo \
	itcl/generic/itcl_class.lo \
	itcl/generic/itcl_cmds.lo \
	itcl/generic/itcl_ensemble.lo \
	itcl/generic/itcl_linkage.lo \
	itcl/generic/itcl_methods.lo \
	itcl/generic/itcl_migrate.lo \
	itcl/generic/itcl_objects.lo \
	itcl/generic/itcl_parse.lo \
	itcl/generic/itcl_util.lo

libitcl_la_SOURCES =
libitcl@ITCL_VERSION@_la_LIBADD = $(libitcl_la_LIBADD)
libitcl@ITCL_VERSION@_la_SOURCES = $(libitcl_la_SOURCES)

libitk_la_LIBADD = \
	itk/generic/itkStubInit.lo \
	itk/generic/itk_archetype.lo \
	itk/generic/itk_cmds.lo \
	itk/generic/itk_option.lo \
	itk/generic/itk_util.lo

libitk_la_SOURCES =
libitk@ITCL_VERSION@_la_LIBADD = $(libitk_la_LIBADD)
libitk@ITCL_VERSION@_la_SOURCES = $(libitk_la_SOURCES)

if BUILD_TK
DEPS=src/other/libtk
else
DEPS=
endif

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

