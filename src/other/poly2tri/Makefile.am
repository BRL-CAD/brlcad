
lib_LTLIBRARIES = libp2t.la

libp2t_la_SOURCES = \
	poly2tri/common/shapes.cc \
	poly2tri/common/utils.cc \
	poly2tri/sweep/advancing_front.cc \
	poly2tri/sweep/cdt.cc \
	poly2tri/sweep/sweep_context.cc \
	poly2tri/sweep/sweep.cc

noinst_HEADERS = \
	poly2tri/common/utils.h \
	poly2tri/sweep/advancing_front.h \
	poly2tri/sweep/sweep_context.h \
	poly2tri/sweep/sweep.h

libp2tincludedir = $(includedir)/poly2tri
libp2tinclude_HEADERS = \
	poly2tri/poly2tri.h

libp2tcommonincludedir = $(includedir)/poly2tri/common
libp2tcommoninclude_HEADERS = \
	poly2tri/common/shapes.h
	
libp2tsweepincludedir = $(includedir)/poly2tri/sweep
libp2tsweepinclude_HEADERS = \
	poly2tri/sweep/cdt.h

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

