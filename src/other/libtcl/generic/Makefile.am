# $Id$

lib_LTLIBRARIES = \
	libtclstub.la \
	libtclstub@TCL_VERSION@.la

noinst_LTLIBRARIES = libtclg.la

libtclstub_la_SOURCES = tclStubLib.c
libtclstub@TCL_VERSION@_la_SOURCES = $(libtclstub_la_SOURCES)

libtclg_la_LDFLAGS = -static
libtclg_la_SOURCES = \
	regcomp.c \
	regerror.c \
	regexec.c \
	regfree.c \
	tclAlloc.c \
	tclAsync.c \
	tclBasic.c \
	tclBinary.c \
	tclCkalloc.c \
	tclClock.c \
	tclCmdAH.c \
	tclCmdIL.c \
	tclCmdMZ.c \
	tclCompCmds.c \
	tclCompExpr.c \
	tclCompile.c \
	tclDate.c \
	tclEncoding.c \
	tclEnv.c \
	tclEvent.c \
	tclExecute.c \
	tclFCmd.c \
	tclFileName.c \
	tclGet.c \
	tclHash.c \
	tclHistory.c \
	tclIO.c \
	tclIOCmd.c \
	tclIOGT.c \
	tclIOSock.c \
	tclIOUtil.c \
	tclIndexObj.c \
	tclInterp.c \
	tclLink.c \
	tclListObj.c \
	tclLiteral.c \
	tclLoad.c \
	tclMain.c \
	tclNamesp.c \
	tclNotify.c \
	tclObj.c \
	tclPanic.c \
	tclParse.c \
	tclParseExpr.c \
	tclPipe.c \
	tclPkg.c \
	tclPosixStr.c \
	tclPreserve.c \
	tclProc.c \
	tclRegexp.c \
	tclResolve.c \
	tclResult.c \
	tclScan.c \
	tclStringObj.c \
	tclStubInit.c \
	tclStubLib.c \
	tclThread.c \
	tclThreadAlloc.c \
	tclThreadJoin.c \
	tclTimer.c \
	tclUtf.c \
	tclUtil.c \
	tclVar.c

noinst_HEADERS = \
	regcustom.h \
	regerrs.h \
	regex.h \
	regguts.h \
	tclCompile.h \
	tclIO.h \
	tclInitScript.h \
	tclInt.h \
	tclIntDecls.h \
	tclIntPlatDecls.h \
	tclMath.h \
	tclPort.h \
	tclRegexp.h

include_HEADERS = \
	tcl.h \
	tclDecls.h \
	tclPlatDecls.h

EXTRA_DIST = \
	regc_color.c \
	regc_cvec.c \
	regc_lex.c \
	regc_locale.c \
	regc_nfa.c \
	rege_dfa.c \
	regfronts.c \
	tcl.decls \
	tclGetDate.y \
	tclInt.decls \
	tclLoadNone.c \
	tclTest.c \
	tclTestObj.c \
	tclTestProcBodyObj.c \
	tclThreadTest.c \
	tclUniData.c

brlcadincludedir = $(includedir)/brlcad
brlcadinclude_HEADERS = $(include_HEADERS)

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/libtcl/generic \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
