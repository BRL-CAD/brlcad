# $Id$

lib_LTLIBRARIES = libbn.la
noinst_LTLIBRARIES = libbn_nil.la

man_MANS = libplot3.3

SRCS = \
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
	vert_tree.c \
	wavelet.c

libbn_nil_la_SOURCES = \
	$(SRCS) \
	vers.c

libbn_la_SOURCES =
libbn_la_LDFLAGS = -version-info 19:1
libbn_la_LIBADD = \
	libbn_nil.la \
	${BU} \
	${TCL} \
	${LIBM}

EXTRA_DIST = \
	$(man_MANS) \
	libbn.dsp \
	plot3.doc \
	vers_win.c

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh bn_version "The BRL-CAD Numerical Computation Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs
