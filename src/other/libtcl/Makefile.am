# $Id$

SUBDIRS = \
	doc \
	library

DIST_SUBDIRS = \
	doc \
	generic \
	library \
	unix

generic_src = \
	$(srcdir)/generic/regcomp.c\
	$(srcdir)/generic/regerror.c\
	$(srcdir)/generic/regexec.c\
	$(srcdir)/generic/regfree.c\
	$(srcdir)/generic/tclAlloc.c\
	$(srcdir)/generic/tclAsync.c\
	$(srcdir)/generic/tclBasic.c\
	$(srcdir)/generic/tclBinary.c\
	$(srcdir)/generic/tclCkalloc.c\
	$(srcdir)/generic/tclClock.c\
	$(srcdir)/generic/tclCmdAH.c\
	$(srcdir)/generic/tclCmdIL.c\
	$(srcdir)/generic/tclCmdMZ.c\
	$(srcdir)/generic/tclCompCmds.c\
	$(srcdir)/generic/tclCompExpr.c\
	$(srcdir)/generic/tclCompile.c\
	$(srcdir)/generic/tclDate.c\
	$(srcdir)/generic/tclEncoding.c\
	$(srcdir)/generic/tclEnv.c\
	$(srcdir)/generic/tclEvent.c\
	$(srcdir)/generic/tclExecute.c\
	$(srcdir)/generic/tclFCmd.c\
	$(srcdir)/generic/tclFileName.c\
	$(srcdir)/generic/tclGet.c\
	$(srcdir)/generic/tclHash.c\
	$(srcdir)/generic/tclHistory.c\
	$(srcdir)/generic/tclIO.c\
	$(srcdir)/generic/tclIOCmd.c\
	$(srcdir)/generic/tclIOGT.c\
	$(srcdir)/generic/tclIOSock.c\
	$(srcdir)/generic/tclIOUtil.c\
	$(srcdir)/generic/tclIndexObj.c\
	$(srcdir)/generic/tclInterp.c\
	$(srcdir)/generic/tclLink.c\
	$(srcdir)/generic/tclListObj.c\
	$(srcdir)/generic/tclLiteral.c\
	$(srcdir)/generic/tclLoad.c\
	$(srcdir)/generic/tclMain.c\
	$(srcdir)/generic/tclNamesp.c\
	$(srcdir)/generic/tclNotify.c\
	$(srcdir)/generic/tclObj.c\
	$(srcdir)/generic/tclPanic.c\
	$(srcdir)/generic/tclParse.c\
	$(srcdir)/generic/tclParseExpr.c\
	$(srcdir)/generic/tclPipe.c\
	$(srcdir)/generic/tclPkg.c\
	$(srcdir)/generic/tclPosixStr.c\
	$(srcdir)/generic/tclPreserve.c\
	$(srcdir)/generic/tclProc.c\
	$(srcdir)/generic/tclRegexp.c\
	$(srcdir)/generic/tclResolve.c\
	$(srcdir)/generic/tclResult.c\
	$(srcdir)/generic/tclScan.c\
	$(srcdir)/generic/tclStringObj.c\
	$(srcdir)/generic/tclStubInit.c\
	$(srcdir)/generic/tclStubLib.c\
	$(srcdir)/generic/tclThread.c\
	$(srcdir)/generic/tclThreadAlloc.c\
	$(srcdir)/generic/tclThreadJoin.c\
	$(srcdir)/generic/tclTimer.c\
	$(srcdir)/generic/tclUtf.c\
	$(srcdir)/generic/tclUtil.c\
	$(srcdir)/generic/tclVar.c

unix_src = \
	$(srcdir)/unix/tclLoadDl.c \
	$(srcdir)/unix/tclUnixChan.c \
	$(srcdir)/unix/tclUnixEvent.c \
	$(srcdir)/unix/tclUnixFCmd.c \
	$(srcdir)/unix/tclUnixFile.c \
	$(srcdir)/unix/tclUnixInit.c \
	$(srcdir)/unix/tclUnixNotfy.c \
	$(srcdir)/unix/tclUnixPipe.c \
	$(srcdir)/unix/tclUnixSock.c \
	$(srcdir)/unix/tclUnixThrd.c \
	$(srcdir)/unix/tclUnixTime.c

lib_LTLIBRARIES = \
	libtcl.la \
	libtcl@TCL_VERSION@.la

libtcl_la_SOURCES = $(generic_src) $(unix_src)

libtcl_la_LDFLAGS = -version-info 19:1

libtcl_la_LIBADD = ${LIBZ} ${LIBM}


libtcl@TCL_VERSION@_la_SOURCES = $(libtcl_la_SOURCES)

libtcl@TCL_VERSION@_la_LDFLAGS = $(libtcl_la_LDFLAGS)

libtcl@TCL_VERSION@_la_LIBADD = $(libtcl_la_LIBADD)


bin_PROGRAMS = tclsh tclsh.static

tclsh_SOURCES = unix/tclAppInit.c

tclsh_LDADD = $(lib_LTLIBRARIES) ${TCL}


# XXX - should not have to do this, but quick fix for muves folks for now
tclsh_static_SOURCES = unix/tclAppInit.c
tclsh_static_LDADD = .libs/libtcl.a


tcllibdir = ${libdir}

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/libtcl/generic \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0

