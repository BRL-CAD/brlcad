# $Id$

noinst_LTLIBRARIES = libtclunix.la

libtclunix_la_SOURCES = tclAppInit.c tclLoadDl.c tclUnixChan.c tclUnixEvent.c tclUnixFCmd.c tclUnixFile.c tclUnixInit.c tclUnixNotfy.c tclUnixPipe.c tclUnixSock.c tclUnixThrd.c tclUnixTime.c

tcldir = @TCL_LIBRARY@
dist_tcl_DATA = tclAppInit.c ldAix
tcllibdir = ${libdir}
tcllib_DATA = tclConfig.sh
EXTRA_DIST = tclConfig.sh.in mkLinks

AM_CFLAGS = -I${top_srcdir}/libtcl8.4/generic $(TCL_BUILD_CFLAGS)

noinst_HEADERS = tclUnixPort.h tclUnixThrd.h


