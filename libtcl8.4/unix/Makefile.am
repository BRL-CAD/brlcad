# $Id$

noinst_LTLIBRARIES = libtclunix.la

libtclunix_la_SOURCES = tclAppInit.c tclLoadDl.c tclUnixChan.c tclUnixEvent.c tclUnixFCmd.c tclUnixFile.c tclUnixInit.c tclUnixNotfy.c tclUnixPipe.c tclUnixSock.c tclUnixThrd.c tclUnixTime.c

tcllibdir = @TCL_LIBRARY@
dist_tcllib_DATA = tclAppInit.c ldAix

AM_CFLAGS = -I${top_srcdir}/libtcl8.4/generic $(TCL_BUILD_CFLAGS)

noinst_HEADERS = tclUnixPort.h tclUnixThrd.h


