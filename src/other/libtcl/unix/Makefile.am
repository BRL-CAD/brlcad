# $Id$

noinst_LTLIBRARIES = libtclunix.la

libtclunix_la_SOURCES = \
	tclAppInit.c \
	tclLoadDl.c \
	tclUnixChan.c \
	tclUnixEvent.c \
	tclUnixFCmd.c \
	tclUnixFile.c \
	tclUnixInit.c \
	tclUnixNotfy.c \
	tclUnixPipe.c \
	tclUnixSock.c \
	tclUnixThrd.c \
	tclUnixTime.c

tcllib_DATA = tclConfig.sh

EXTRA_DIST = \
	dltest \
	tclConfig.sh.in \
	mkLinks \
	tclAppInit.c \
	ldAix

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/libtcl/generic \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0

noinst_HEADERS = \
	tclUnixPort.h \
	tclUnixThrd.h

tcllibdir = ${libdir}

