# $Id$

SUBDIRS = \
	doc \
	generic \
	library \
	unix

lib_LTLIBRARIES = \
	libtcl.la \
	libtcl@TCL_VERSION@.la

libtcl_la_SOURCES =
libtcl_la_LDFLAGS = -version-info 19:1
libtcl_la_LIBADD = unix/libtclu.la generic/libtclg.la ${LIBZ} ${LIBM} 

libtcl@TCL_VERSION@_la_SOURCES = $(libtcl_la_SOURCES)
libtcl@TCL_VERSION@_la_LDFLAGS = $(libtcl_la_LDFLAGS)
libtcl@TCL_VERSION@_la_LIBADD = $(libtcl_la_LIBADD)

bin_PROGRAMS = tclsh tclsh.static

tclsh_SOURCES = 
tclsh_LDADD = unix/libtclsh.la libtcl.la

tclsh_static_SOURCES = $(tclsh_SOURCES)
tclsh_static_LDADD = $(tclsh_LDADD)
tclsh_static_LDFLAGS = -static

tcllibdir = ${libdir}

AM_CFLAGS =
AM_CPPFLAGS = \
	-I${top_srcdir}/src/other/libtcl/generic \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	-DUSE_TCLALLOC=0

.PHONY : test
test: tclsh.static
	echo 'puts "hi"' | ./tclsh.static

EXTRA_DIST = \
	ChangeLog.1999 \
	ChangeLog.2000 \
	ChangeLog.2001 \
	changes \
	compat \
	mac \
	macosx \
	tests \
	tools

MAINTAINERCLEANFILES = Makefile.in
