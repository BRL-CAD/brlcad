
lib_LTLIBRARIES = libbn.la
noinst_LTLIBRARIES = libbn_nil.la

noinst_PROGRAMS = bntester

man_MANS = libplot3.3

libbn_nil_la_SOURCES = \
	anim.c \
	axis.c \
	complex.c \
	font.c \
	fortran.c \
	globals.c \
	list.c \
	marker.c \
	mat.c \
	msr.c \
	multipoly.c \
	noise.c \
	plane.c \
	plot3.c \
	poly.c \
	qmath.c \
	rand.c \
	randmt.c \
	scale.c \
	sphmap.c \
	symbol.c \
	tabdata.c \
	tcl.c \
	tplot.c \
	vectfont.c \
	vector.c \
	vers.c \
	vert_tree.c \
	wavelet.c

libbn_la_SOURCES =
libbn_la_LDFLAGS = -version-info 19:1 -no-undefined
libbn_la_LIBADD = \
	libbn_nil.la \
	${BN_LIBS}

bntester_SOURCES = bntester.c
bntester_LDADD = \
	libbn.la \
	${BN_LIBS}

AM_CPPFLAGS = ${TCL_CPPFLAGS}

AM_CFLAGS = ${STRICT_FLAGS}

EXTRA_DIST = \
	$(man_MANS) \
	bntester.dat \
	ulp.c \
	CMakeLists.txt \
	README

DEPENDS = src/libbu

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libbn_nil_la_OBJECTS) \
	$(libbn_la_OBJECTS) \
	$(lib_LTLIBRARIES) \
	$(noinst_LTLIBRARIES) \
	$(bntester_OBJECTS) \
	$(noinst_PROGRAMS)
