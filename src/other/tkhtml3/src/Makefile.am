# $Id$

noinst_LTLIBRARIES = tkhtml3b.la

AM_CFLAGS = \
	-DPACKAGE_NAME="Tkhtml" \
	-DPACKAGE_VERSION="3.0" \
	-DPACKAGE_STRING="Tkhtml 3.0" \
	-DTCL_LIBRARY="\"${TCL_LIBRARY}\"" \
	-DTCL_PACKAGE_PATH="\"${TCL_PACKAGE_PATH}\"" \
	${TCL_CPPFLAGS} \
	${TK_CPPFLAGS} \
	-I${top_srcdir}/src/other/tkhtml3/include 

tkhtml3b_la_LDFLAGS = -module
tkhtml3b_la_SOURCES = \
	css.c \
	cssdynamic.c \
	cssparser.c \
	cssprop.c \
	csssearch.c \
	htmldecode.c \
	htmldefaultstyle.c \
	htmldraw.c \
	htmlfloat.c \
	htmlhash.c \
	htmlimage.c \
	htmlinline.c \
	htmllayout.c \
	htmlparse.c \
	htmlprop.c \
	htmlstyle.c \
	htmltable.c \
	htmltagdb.c \
	htmltcl.c \
	htmltext.c \
	htmltree.c \
	htmlutil.c \
	restrack.c \
	swproc.c

EXTRA_DIST = \
	htmltoken.c

include $(top_srcdir)/misc/Makefile.defs
