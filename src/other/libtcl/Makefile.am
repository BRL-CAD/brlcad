# $Id$

SUBDIRS = \
	doc \
	generic \
	library \
	unix

AM_CFLAGS =
AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/libtcl/generic \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0 \
	${LIBZ_CPPFLAGS}

lib_LTLIBRARIES = \
	libtcl.la \
	libtcl@TCL_VERSION@.la
noinst_LTLIBRARIES = \
	libtcl_nil.la

libtcl_la_SOURCES =
libtcl_la_LDFLAGS = -version-info 19:1
libtcl_la_LIBADD = \
	libtcl_nil.la

libtcl_nil_la_SOURCES =
libtcl_nil_la_LIBADD = \
	generic/regcomp.lo \
	generic/regerror.lo \
	generic/regexec.lo \
	generic/regfree.lo \
	generic/tclAlloc.lo \
	generic/tclAsync.lo \
	generic/tclBasic.lo \
	generic/tclBinary.lo \
	generic/tclCkalloc.lo \
	generic/tclClock.lo \
	generic/tclCmdAH.lo \
	generic/tclCmdIL.lo \
	generic/tclCmdMZ.lo \
	generic/tclCompCmds.lo \
	generic/tclCompExpr.lo \
	generic/tclCompile.lo \
	generic/tclDate.lo \
	generic/tclEncoding.lo \
	generic/tclEnv.lo \
	generic/tclEvent.lo \
	generic/tclExecute.lo \
	generic/tclFCmd.lo \
	generic/tclFileName.lo \
	generic/tclGet.lo \
	generic/tclHash.lo \
	generic/tclHistory.lo \
	generic/tclIO.lo \
	generic/tclIOCmd.lo \
	generic/tclIOGT.lo \
	generic/tclIOSock.lo \
	generic/tclIOUtil.lo \
	generic/tclIndexObj.lo \
	generic/tclInterp.lo \
	generic/tclLink.lo \
	generic/tclListObj.lo \
	generic/tclLiteral.lo \
	generic/tclLoad.lo \
	generic/tclMain.lo \
	generic/tclNamesp.lo \
	generic/tclNotify.lo \
	generic/tclObj.lo \
	generic/tclPanic.lo \
	generic/tclParse.lo \
	generic/tclParseExpr.lo \
	generic/tclPipe.lo \
	generic/tclPkg.lo \
	generic/tclPosixStr.lo \
	generic/tclPreserve.lo \
	generic/tclProc.lo \
	generic/tclRegexp.lo \
	generic/tclResolve.lo \
	generic/tclResult.lo \
	generic/tclScan.lo \
	generic/tclStringObj.lo \
	generic/tclStubInit.lo \
	generic/tclStubLib.lo \
	generic/tclThread.lo \
	generic/tclThreadAlloc.lo \
	generic/tclThreadJoin.lo \
	generic/tclTimer.lo \
	generic/tclUtf.lo \
	generic/tclUtil.lo \
	generic/tclVar.lo \
	unix/tclLoadDl.lo \
	unix/tclUnixChan.lo \
	unix/tclUnixEvent.lo \
	unix/tclUnixFCmd.lo \
	unix/tclUnixFile.lo \
	unix/tclUnixInit.lo \
	unix/tclUnixNotfy.lo \
	unix/tclUnixPipe.lo \
	unix/tclUnixSock.lo \
	unix/tclUnixThrd.lo \
	unix/tclUnixTime.lo \
	${LIBDL} \
	${LIBSOCKET} \
	${LIBNSL} \
	${LIBZ} \
	${LIBM} 

libtcl@TCL_VERSION@_la_SOURCES = $(libtcl_la_SOURCES)
libtcl@TCL_VERSION@_la_LDFLAGS = $(libtcl_la_LDFLAGS)
libtcl@TCL_VERSION@_la_LIBADD = $(libtcl_la_LIBADD)

noinst_PROGRAMS = tclsh

# MIPSpro 7.3 has a linker bug with long rpaths
if LINK_STATIC_REQUIRED
tclsh_LDFLAGS = -static
endif

tclsh_SOURCES = 
tclsh_LDADD = \
	unix/tclAppInit.lo \
	libtcl.la

tcllibdir = ${libdir}

EXTRA_DIST = \
	ChangeLog.1999 \
	ChangeLog.2000 \
	ChangeLog.2001 \
	changes \
	compat \
	mac \
	macosx \
	tests \
	tools \
	win

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
