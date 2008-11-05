
lib_LTLIBRARIES = libwdb.la
noinst_LTLIBRARIES = libwdb_nil.la


BREP_CPPFLAGS=-DOBJ_BREP=1 $(OPENNURBS_CPPFLAGS)
BREP_SRCS=brep.cpp
BREP_EXTRA=brep_stub.c

AM_CPPFLAGS = ${TCL_CPPFLAGS} ${BREP_CPPFLAGS}

libwdb_nil_la_SOURCES = \
	$(BREP_SRCS) \
	arbn.c \
	ars.c \
	bot.c \
	cline.c \
	constraint.c \
	dsp.c \
	dynamic_geometry.c \
	ebm.c \
	export.c \
	extr.c \
	id.c \
	mater.c \
	nmg.c \
	nurb.c \
	pipe.c \
	reg.c \
	skt.c \
	submodel.c \
	units.c \
	vol.c \
	wdb.c

libwdb_la_SOURCES =
libwdb_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
libwdb_la_LIBADD = \
	libwdb_nil.la \
	${WDB_LIBS}

man_MANS = libwdb.3

# the C files here are deprecated
EXTRA_DIST = \
	$(man_MANS) \
	$(BREP_EXTRA) \
	CMakeLists.txt

DEPS = src/librt

include $(top_srcdir)/misc/Makefile.defs
