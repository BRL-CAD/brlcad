
lib_LTLIBRARIES = libwdb.la
noinst_LTLIBRARIES = libwdb_nil.la

BREP_CPPFLAGS=-DOBJ_BREP=1 $(OPENNURBS_CPPFLAGS)
AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS} \
	${BREP_CPPFLAGS}

libwdb_nil_la_SOURCES = \
	arbn.c \
	ars.c \
	bot.c \
	brep.cpp \
	cline.c \
	constraint.c \
	dsp.c \
	dynamic_geometry.c \
	ebm.c \
	export.c \
	extrude.c \
	id.c \
	mater.c \
	nmg.c \
	nurb.c \
	pipe.c \
	reg.c \
	sketch.c \
	submodel.c \
	units.c \
	vol.c \
	wdb.c

libwdb_la_SOURCES =
libwdb_la_LDFLAGS = -version-info 20:1 -no-undefined -Wc,${FEXCEPTIONS}
libwdb_la_LIBADD = \
	libwdb_nil.la \
	${WDB_LIBS}

EXTRA_DIST = \
	CMakeLists.txt

DEPS = src/librt

include $(top_srcdir)/misc/Makefile.defs
