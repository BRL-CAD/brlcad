SUBDIRS=\
	tests

lib_LTLIBRARIES= libobj.la

BUILT_SOURCES = obj_grammar.h
AM_YFLAGS = -d -p obj_parser_
AM_LFLAGS= -Pobj_parser_ -oobj_rules.cc --header-file=obj_rules.h

libobj_la_SOURCES= \
	obj_rules.cc \
	obj_grammar.yy \
	obj_parser_state.h \
	obj_parser.h \
	obj_parser.cc

include_HEADERS= \
	obj_parser.h

obj_rules.cc: obj_rules.ll
	$(LEX) $(LFLAGS) $(AM_LFLAGS) $^

libobj_la_CXXFLAGS=-pedantic -ansi -Wall ${BOOST_CPPFLAGS}

EXTRA_DIST=\
	boost_shared_ptr \
	obj_rules.ll \
	obj_rules.h \
	obj_rules.cc \
	obj_grammar.h \
	obj_grammar.cc

DISTCLEANFILES= \
	obj_rules.h \
	obj_rules.cc \
	obj_grammar.h \
	obj_grammar.cc

