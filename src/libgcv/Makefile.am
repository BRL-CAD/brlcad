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
	wfobj/CMakeLists.txt \
	wfobj/Makefile.am \
	wfobj/Makefile.in \
	wfobj/Makefile.sample \
	wfobj/obj_grammar_decls.h.in \
	wfobj/obj_grammar.yy \
	wfobj/obj_parser.cpp \
	wfobj/obj_parser.h \
	wfobj/obj_parser_state.h \
	wfobj/obj_rules.h \
	wfobj/obj_rules.re \
	wfobj/obj_token_type.h \
	wfobj/re2c_utils.c \
	wfobj/re2c_utils.h

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
