
lib_LTLIBRARIES = libgcv.la
noinst_LTLIBRARIES = libgcv_nil.la

AM_CPPFLAGS = -I$(top_srcdir)/src/libgcv/obj ${TCL_CPPFLAGS}
AM_YFLAGS = -d -p obj_parser_

if WITH_MODERN_PARSERS

OBJ_SRCS = \
	obj/obj_rules.ll \
	obj/obj_grammar.yy \
	obj/obj_parser.cc

OBJ_BUILT = \
	obj_rules.cc \
	obj_rules.h \
	obj_grammar.cc \
	obj_grammar.h

OBJ_noinst_HEADER =

OBJ_extradist = \
	obj/obj_rules_static.cc \
	obj/obj_rules_static.h \
	obj/obj_grammar_static.cc \
	obj/obj_grammar_static.h \
	obj/obj_parser_static.cc
	
else !WITH_MODERN_PARSERS

OBJ_SRCS = \
	obj/obj_rules_static.cc \
	obj/obj_grammar_static.cc \
	obj/obj_parser_static.cc

OBJ_BUILT = 

OBJ_noinst_HEADER = \
	obj/obj_rules_static.h \
	obj/obj_grammar_static.h

OBJ_extradist = \
	obj/obj_rules.ll \
	obj/obj_grammar.yy \
	obj/obj_parser.cc

endif

obj_rules.cc: obj/obj_rules.ll obj_grammar.h
	$(LEX) -Pobj_parser_ -oobj_rules.cc --header-file=obj_rules.h $^

obj_grammar.cc: obj/obj_grammar.yy
	$(top_srcdir)/misc/ylwrap $^ y.tab.c obj_grammar.cc y.tab.h obj_grammar.h y.output obj_grammar.output -- $(YACC) -d -p obj_parser_

obj_grammar.h: obj_grammar.cc

libgcv_nil_la_SOURCES = \
	$(OBJ_SRCS) \
	region_end.c \
	region_start_mc.c

EXTRA_DIST = \
	$(OBJ_extradist) \
	CMakeLists.txt \
	NOTES

libgcv_la_SOURCES =
libgcv_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS} 
libgcv_la_LIBADD = \
	libgcv_nil.la \
	${GCV_LIBS}

noinst_HEADERS = \
	$(OBJ_noinst_HEADER) \
	obj/obj_parser.h \
	obj/obj_parser_state.h

DISTCLEANFILES = \
	obj_rules.cc \
	obj_rules.h \
	obj_grammar.cc \
	obj_grammar.h

AM_CFLAGS = ${STRICT_FLAGS}

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libgcv_nil_la_OBJECTS) \
	$(libgcv_la_OBJECTS) \
	$(lib_LTLIBRARIES)
