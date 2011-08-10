#noinst_LTLIBRARIES = libobj.la

bin_PROGRAMS = \
	parse

parse_LDADD = ${BU}
parse_SOURCES = \
	obj_grammar.yy \
	obj_rules.ll \
	obj_parser.cc

noinst_HEADERS = \
	obj_grammar.h \
	obj_parser.h

obj_grammar.h : obj_grammar.yy
obj_parser.lo : obj_grammar.h

BUILT_SOURCES = \
	obj_rules.cc \
	obj_grammar.h \
	obj_grammar.cc

DISTCLEANFILES = $(BUILT_SOURCES)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libobj_nil_la_OBJECTS) \
	$(parse_OBJECTS)
