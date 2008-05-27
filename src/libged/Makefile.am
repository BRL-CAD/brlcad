
lib_LTLIBRARIES = libged.la
noinst_LTLIBRARIES = libged_nil.la

libged_nil_la_SOURCES = \
	arced.c \
	bigE.c \
	comb_color.c \
	dg_obj.c \
	editit.c \
	edcomb.c \
	edmater.c \
	globals.c \
	importFg4Section.c \
	item.c \
	log.c \
	make.c \
	make_name.c \
	mater.c \
	mirror.c \
	nirt.c \
	qray.c \
	rmater.c \
	shader.c \
	tcl.c \
	track.c \
	vdraw.c \
	view_obj.c \
	wdb_comb_std.c \
	wdb_obj.c \
	wmater.c

# cannot set per-target CPPFLAGS until automake 1.7+
AM_CPPFLAGS = \
	${TCL_CPPFLAGS}

noinst_HEADERS = qray.h

libged_la_SOURCES =
libged_la_LDFLAGS = -version-info 19:1 -no-undefined
libged_la_LIBADD = \
	libged_nil.la \
	${GED_LIBS}

EXTRA_DIST = \
        CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libged_nil_la_OBJECTS) \
	$(libged_la_OBJECTS) \
	$(lib_LTLIBRARIES)
