# $Id$

lib_LTLIBRARIES = libbn.la

SRCS = \
	anim.c \
	asize.c \
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
	number.c \
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
	vert_tree.c \
	wavelet.c

libbn_la_SOURCES = $(SRCS) vers.c

man_MANS = libplot3.3

EXTRA_DIST = \
	htester.c \
	$(man_MANS)


libbn_la_LDFLAGS = -version-info 19:1

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh bn_version "The BRL-CAD Numerical Computation Library" > $@
