
lib_LTLIBRARIES = libp2t.la

libp2t_la_SOURCES = \
	poly2tri/common/shapes.cc \
	poly2tri/common/utils.cc \
	poly2tri/sweep/advancing_front.cc \
	poly2tri/sweep/cdt.cc \
	poly2tri/sweep/sweep_context.cc \
	poly2tri/sweep/sweep.cc

noinst_HEADERS = \
	poly2tri/common/shapes.h \
	poly2tri/common/utils.h \
	poly2tri/sweep/advancing_front.h \
	poly2tri/sweep/cdt.h \
	poly2tri/sweep/sweep_context.h \
	poly2tri/sweep/sweep.h

include_HEADERS = \
	poly2tri/poly2tri.h

EXTRA_DIST = \
	AUTHORS \
	LICENSE \
	readme.txt \
	README.md \
	testbed \
	CMakeLists.txt \
	waf \
	wscript

include $(top_srcdir)/misc/Makefile.defs

