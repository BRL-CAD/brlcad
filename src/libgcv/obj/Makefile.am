noinst_LTLIBRARIES = libobj_nil.la

AM_YFLAGS = -d -p obj_parser_
AM_LFLAGS= -Pobj_parser_ -oobj_rules.cc --header-file=obj_rules.h

libobj_nil_la_SOURCES = \
	obj_rules.ll \
	obj_grammar.yy \
	obj_parser.cc

noinst_HEADERS = \
	obj_parser.h

obj_rules.cc: obj_rules.ll
	$(LEX) $(LFLAGS) $(AM_LFLAGS) $^

obj_rules.h : obj_rules.cc
obj_grammar.h: obj_grammar.cc

BUILT_SOURCES = \
	obj_rules.cc \
	obj_rules.h \
	obj_grammar.cc \
	obj_grammar.h

DISTCLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = \
	obj_rules.l \
	obj_grammar.y

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libobj_nil_la_OBJECTS)
