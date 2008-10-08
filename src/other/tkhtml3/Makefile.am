# $Id$

SUBDIRS= \
	src \
	include

AM_CFLAGS = \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\""
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS} 

tkhtml3_la_LDFLAGS = -module

lib_LTLIBRARIES = tkhtml3.la

tkhtml3_la_SOURCES = 

tkhtml3_la_LIBADD = \
	src/css.lo \
	src/cssdynamic.lo \
	src/cssparser.lo \
	src/cssprop.lo \
	src/csssearch.lo \
	src/htmldecode.lo \
	src/htmldefaultstyle.lo \
	src/htmldraw.lo \
	src/htmlfloat.lo \
	src/htmlhash.lo \
	src/htmlimage.lo \
	src/htmlinline.lo \
	src/htmllayout.lo \
	src/htmlparse.lo \
	src/htmlprop.lo \
	src/htmlstyle.lo \
	src/htmltable.lo \
	src/htmltagdb.lo \
	src/htmltcl.lo \
	src/htmltext.lo \
	src/htmltree.lo \
	src/htmlutil.lo \
	src/restrack.lo \
	src/swproc.lo \
	${TCL} \
	${TK}


EXTRA_DIST = \
	COPYRIGHT

include $(top_srcdir)/misc/Makefile.defs
