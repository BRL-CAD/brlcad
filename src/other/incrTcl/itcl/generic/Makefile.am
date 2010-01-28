
lib_LTLIBRARIES = \
	libitclstub.la \
	libitclstub@ITCL_VERSION@.la

noinst_LTLIBRARIES = libitg.la

# make sure we find Tcl's private headers
if TCL_8_4_HEADERS
compat_cppflags = \
	-I$(top_srcdir)/src/other/incrTcl/compat/8.4
else
compat_cppflags = \
	-I$(top_srcdir)/src/other/tcl/generic \
	-I$(top_srcdir)/src/other/tcl/unix
endif # TCL_8_4_HEADERS

AM_CPPFLAGS = \
	${TCL_CPPFLAGS} \
	${compat_cppflags}

libitclstub_la_SOURCES = itclStubLib.c
libitclstub@ITCL_VERSION@_la_SOURCES = $(libitclstub_la_SOURCES)

libitg_la_SOURCES = \
	itcl2TclOO.c \
	itclBase.c \
	itclBuiltin.c \
	itclClass.c \
	itclCmd.c \
	itclEnsemble.c \
	itclHelpers.c \
	itclInfo.c \
	itclLinkage.c \
	itclMethod.c \
	itclMigrate2TclCore.c \
	itclNeededFromTclOO.c \
	itclObject.c \
	itclParse.c \
	itclResolve.c \
	itclResolve2.c \
	itclStubInit.c \
	itclStubs.c \
	itclTclIntStubsFcn.c \
	itclTestRegisterC.c \
	itclUtil.c \
	itclVarsAndCmds.c


include_HEADERS = \
	itcl.h	\
	itcl2TclOO.h \
	itclDecls.h \
	itclInt.h \
	itclIntDecls.h \
	itclMigrate2TclCore.h \
	itclNeededFromTclOO.h \
	itclTclIntStubsFcn.h \
	itclVCInt.h \
	itclVarsAndCmds.h

	
EXTRA_DIST = \
	itcl.decls \
	itclInt.decls

include $(top_srcdir)/misc/Makefile.defs

