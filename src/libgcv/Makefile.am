
lib_LTLIBRARIES = libgcv.la
noinst_LTLIBRARIES = libgcv_nil.la

AM_CPPFLAGS = ${TCL_CPPFLAGS}
AM_YFLAGS = -d -p obj/obj_parser_

libgcv_nil_la_SOURCES = \
	obj/obj_rules.cc \
	obj/obj_grammar.cc \
	obj/obj_parser.cc \
	region_end.c \
	region_start_mc.c

#
#  lex/yacc requirements appear to be too much for a number of platforms
#  include and build the generated cc files for now
#
#obj/obj_rules.cc: obj/obj_rules.ll
#	$(LEX) -Pobj_parser_ -oobj_rules.cc --header-file=obj/obj_rules.h $^

#BUILT_SOURCES = \
#	obj_rules.cc \
#	obj_grammar.cc


libgcv_la_SOURCES =
libgcv_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
libgcv_la_LIBADD = \
	libgcv_nil.la \
	${GCV_LIBS}

noinst_HEADERS = \
	obj/obj_parser.h \
	obj/obj_rules.h \
	obj/obj_grammar.h \
	obj/obj_parser_state.h

EXTRA_DIST = \
	CMakeLists.txt \
	NOTES \
	obj/obj_rules.ll \
	obj/obj_grammar.yy

AM_CFLAGS = ${STRICT_FLAGS}

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libgcv_nil_la_OBJECTS) \
	$(libgcv_la_OBJECTS) \
	$(lib_LTLIBRARIES)
