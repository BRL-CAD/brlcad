# $Id$

AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/libtcl/generic \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0

noinst_LTLIBRARIES = libtclu.la libtclsh.la

libtclu_la_LDFLAGS = -static
libtclu_la_SOURCES = \
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

libtclsh_la_LDFLAGS = -static
libtclsh_la_SOURCES = tclAppInit.c

EXTRA_DIST = \
	aclocal.m4 \
	dltest \
	ldAix \
	mkLinks \
	tclConfig.sh.in \
	tclLoadAix.c \
	tclLoadAout.c \
	tclLoadDl.c \
	tclLoadDld.c \
	tclLoadDyld.c \
	tclLoadNext.c \
	tclLoadOSF.c \
	tclLoadShl.c \
	tclUnixTest.c \
	tclXtNotify.c \
	tclXtTest.c \
	tcl.m4 \
	tcl.spec

tcllibdir = ${libdir}
tcllib_DATA = tclConfig.sh

noinst_HEADERS = \
	tclUnixPort.h \
	tclUnixThrd.h

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
