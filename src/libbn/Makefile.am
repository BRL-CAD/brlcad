
lib_LTLIBRARIES = libbn.la
noinst_LTLIBRARIES = libbn_nil.la

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

AM_CPPFLAGS = ${TCL_CPPFLAGS}

AM_CFLAGS = ${STRICT_FLAGS}

EXTRA_DIST = \
	$(man_MANS) \
	ulp.c \
	CMakeLists.txt

DEPENDS = src/libbu

include $(top_srcdir)/misc/Makefile.defs
