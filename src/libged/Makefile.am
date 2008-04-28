
lib_LTLIBRARIES = libged.la
noinst_LTLIBRARIES = libged_nil.la

libged_nil_la_SOURCES = \
	bigE.c \
	dg_obj.c \
	globals.c \
	nirt.c \
	qray.c \
	tcl.c \
	vdraw.c \
	view_obj.c \
	wdb_obj.c

noinst_HEADERS = qray.h

libged_la_SOURCES =
libged_la_LDFLAGS = -version-info 19:1
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
