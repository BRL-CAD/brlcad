# $Id$

lib_LTLIBRARIES = libbn.la
noinst_LTLIBRARIES = libbn_nil.la

man_MANS = libplot3.3

libbn_nil_la_SOURCES = \
	anim.c \
	axis.c \
	bn_tcl.c \
	complex.c \
	const.c \
	font.c \
	fortran.c \
	list.c \
	marker.c \
	mat.c \
	msr.c \
	noise.c \
	plane.c \
	plot3.c \
	poly.c \
	qmath.c \
	rand.c \
	scale.c \
	sphmap.c \
	symbol.c \
	tabdata.c \
	tplot.c \
	vectfont.c \
	vector.c \
	vers.c \
	vert_tree.c \
	wavelet.c

libbn_la_SOURCES =
libbn_la_LDFLAGS = -version-info 19:1
libbn_la_LIBADD = \
	libbn_nil.la

AM_CPPFLAGS = ${TCL_CPPFLAGS}

EXTRA_DIST = \
	$(man_MANS) \
	libbn.dsp \
	plot3.doc

MAINTAINERCLEANFILES = Makefile.in

DEPS=src/libbu

include $(top_srcdir)/misc/Makefile.defs
