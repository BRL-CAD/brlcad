
lib_LTLIBRARIES = libged.la
noinst_LTLIBRARIES = libged_nil.la

libged_nil_la_SOURCES = \
	arced.c \
	autoview.c \
	bigE.c \
	blast.c \
	comb_color.c \
	dg_obj.c \
	draw.c \
	editit.c \
	edcomb.c \
	edmater.c \
	erase.c \
	erase_all.c \
	ged.c \
	get_autoview.c \
	get_eyemodel.c \
	get_obj_bounds.c \
	globals.c \
	importFg4Section.c \
	illum.c \
	item.c \
	log.c \
	make.c \
	make_name.c \
	mater.c \
	mirror.c \
	nirt.c \
	ocenter.c \
	orotate.c \
	oscale.c \
	otranslate.c \
	overlay.c \
	qray.c \
	rmater.c \
	rt.c \
	set_outputHandler.c \
	set_uplotOutputMode.c \
	shader.c \
	trace.c \
	track.c \
	vdraw.c \
	view_obj.c \
	vutil.c \
	wdb_comb_std.c \
	wdb_obj.c \
	who.c \
	wmater.c \
	zap.c

# cannot set per-target CPPFLAGS until automake 1.7+
AM_CPPFLAGS = \
	${TCL_CPPFLAGS}

noinst_HEADERS = \
	ged_private.h \
	qray.h

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
