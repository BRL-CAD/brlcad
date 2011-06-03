wfobj_dir = wfobj

#if WITH_PARSERS
#WFOBJ = ${wfobj_dir}
#WFOBJLIB=wvobj/libwfobj_nil.la
#else
WFOBJ = 
WFOBJLIB = 
#endif

SUBDIRS = \
	${WFOBJ}

DIST_SUBDIRS = \
	${WFOBJ}

lib_LTLIBRARIES = libgcv.la
noinst_LTLIBRARIES = libgcv_nil.la

AM_CPPFLAGS = \
	${STRICT_FLAGS} \
	${TCL_CPPFLAGS}

libgcv_nil_la_SOURCES = \
	region_end.c \
	region_end_mc.c \
	bottess.c

EXTRA_DIST = \
	CMakeLists.txt \
	NOTES \
	wfobj/Makefile.am \
	wfobj/Makefile.in \
	wfobj/obj_grammar.y \
	wfobj/obj_grammar.yy \
	wfobj/obj_parser.c \
	wfobj/obj_parser.cc \
	wfobj/obj_parser.h \
	wfobj/obj_parser.h.orig \
	wfobj/obj_parser_state.h \
	wfobj/obj_rules.l \
	wfobj/obj_rules.ll

CLEANFILES = wfobj/Makefile

libgcv_la_SOURCES =
libgcv_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS} 
libgcv_la_LIBADD = \
	libgcv_nil.la \
	${WFOBJLIB} \
	${GCV_LIBS}

AM_CFLAGS = ${STRICT_FLAGS}

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libgcv_nil_la_OBJECTS) \
	$(libgcv_la_OBJECTS) \
	$(lib_LTLIBRARIES)
