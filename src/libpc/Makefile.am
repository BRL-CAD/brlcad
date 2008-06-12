lib_LTLIBRARIES = libpc.la
noinst_LTLIBRARIES = libpc_nil.la

AM_CPPFLAGS = ${TCL_CPPFLAGS}

noinst_PROGRAMS = \
	pc_test \
	solver_test

libpc_nil_la_SOURCES = \
		pc_main.c \
		pc\_solver.cpp

lipc_nil_la_CXXFLAGS = -Wall

libpc_la_SOURCES = 
libpc_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc
libpc_la_LIBADD = \
	libpc_nil.la\
	${PC_LIBS} ${WDB}

pc_test_SOURCES = pc_test.c
pc_test_LDADD = ${PC} ${WDB}

solver_test_SOURCES = solver_test.c
solver_test_LDADD =

include $(top_srcdir)/misc/Makefile.defs
