#noinst_LTLIBRARIES = libobj.la

bin_PROGRAMS = \
	parse

#libobj_la_LDFLAGS = 
#libobj_la_LIBADD = ${BU}
#libobj_la_SOURCES = \
#	obj_grammar.y \
#	obj_rules.l \
#	obj_parser.c

parse_LDADD = ${BU}
parse_SOURCES = \
	obj_grammar.y \
	obj_rules.l \
	obj_parser.c

noinst_HEADERS = \
	obj_grammar.h \
	obj_parser.h

obj_grammar.h : obj_grammar.y
obj_parser.lo : obj_grammar.h

BUILT_SOURCES = \
	obj_rules.c \
	obj_grammar.h \
	obj_grammar.c

EXTRA_DIST = obj_parser.h.orig

DISTCLEANFILES = $(BUILT_SOURCES)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libobj_nil_la_OBJECTS) \
	$(parse_OBJECTS)
