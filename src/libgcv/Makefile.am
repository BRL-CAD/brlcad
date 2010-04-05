obj_dir = obj

#if WITH_PARSERS
#OBJ = ${obj_dir}
#OBJLIB=obj/libobj_nil.la
#else
OBJ = 
OBJLIB = 
#endif

SUBDIRS = \
	${OBJ}

DIST_SUBDIRS = \
	${OBJ}

lib_LTLIBRARIES = libgcv.la
noinst_LTLIBRARIES = libgcv_nil.la

AM_CPPFLAGS = ${TCL_CPPFLAGS}

libgcv_nil_la_SOURCES = \
	region_end.c \
	region_end_mc.c

EXTRA_DIST = \
	CMakeLists.txt \
	NOTES \
	obj/Makefile.in \
	obj/Makefile.am \
	obj/obj_grammar.y \
	obj/obj_grammar.yy \
	obj/obj_parser.cc \
	obj/obj_parser.h \
	obj/obj_parser_state.h \
	obj/obj_rules.l \
	obj/obj_rules.ll

CLEANFILES = obj/Makefile

libgcv_la_SOURCES =
libgcv_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS} 
libgcv_la_LIBADD = \
	libgcv_nil.la \
	${OBJLIB} \
	${GCV_LIBS}

AM_CFLAGS = ${STRICT_FLAGS}

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libgcv_nil_la_OBJECTS) \
	$(libgcv_la_OBJECTS) \
	$(lib_LTLIBRARIES)
