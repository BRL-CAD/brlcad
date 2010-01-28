
ITCLLIB = \
	libitcl.la \
	libitcl@ITCL_VERSION@.la

if BUILD_ITK
ITKDIR = itk
ITKLIB = \
	libitk.la \
	libitk@ITCL_VERSION@.la
else !BUILD_ITK
ITKDIR =
ITKLIB =
endif

SUBDIRS = \
	doc \
	itcl \
	$(ITKDIR) \
	tools

DIST_SUBDIRS = \
	doc \
	itcl \
	itk \
	tools

EXTRA_DIST = \
	CHANGES \
	INCOMPATIBLE \
	compat \
	license.terms \
	makefile.bc \
	makefile.vc \
	rules.vc \
	tcl.m4

lib_LTLIBRARIES = \
	$(ITCLLIB) \
	$(ITKLIB)

libitcl_la_LIBADD = \
	itcl/generic/itclStubInit.lo \
	itcl/generic/itcl2TclOO.lo \
	itcl/generic/itclBase.lo \
	itcl/generic/itclBuiltin.lo \
	itcl/generic/itclClass.lo \
	itcl/generic/itclCmd.lo \
	itcl/generic/itclEnsemble.lo \
	itcl/generic/itclHelpers.lo \
	itcl/generic/itclInfo.lo \
	itcl/generic/itclLinkage.lo \
	itcl/generic/itclMethod.lo \
	itcl/generic/itclMigrate2TclCore.lo \
	itcl/generic/itclNeededFromTclOO.lo \
	itcl/generic/itclObject.lo \
	itcl/generic/itclParse.lo \
	itcl/generic/itclResolve.lo \
	itcl/generic/itclResolve2.lo \
	itcl/generic/itclStubLib.lo \
	itcl/generic/itclStubs.lo \
	itcl/generic/itclTclIntStubsFcn.lo \
	itcl/generic/itclTestRegisterC.lo \
	itcl/generic/itclUtil.lo \
	itcl/generic/itclVarsAndCmds.lo


libitcl_la_SOURCES =
libitcl@ITCL_VERSION@_la_LIBADD = $(libitcl_la_LIBADD)
libitcl@ITCL_VERSION@_la_SOURCES = $(libitcl_la_SOURCES)

libitk_la_LIBADD = \
	itk/generic/itkStubInit.lo \
	itk/generic/itkArchBase.lo \
	itk/generic/itkArchetype.lo \
	itk/generic/itkBase.lo \
	itk/generic/itkCmd.lo \
	itk/generic/itkHelpers.lo \
	itk/generic/itkOption.lo \
	itk/generic/itkStubLib.lo \
	itk/generic/itkUtil.lo

	
libitk_la_SOURCES =
libitk@ITCL_VERSION@_la_LIBADD = $(libitk_la_LIBADD)
libitk@ITCL_VERSION@_la_SOURCES = $(libitk_la_SOURCES)

if BUILD_TK
DEPS=src/other/tk
else
DEPS=
endif

include $(top_srcdir)/misc/Makefile.defs

