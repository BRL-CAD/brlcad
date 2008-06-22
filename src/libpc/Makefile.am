
lib_LTLIBRARIES = libpc.la
noinst_LTLIBRARIES = libpc_nil.la

AM_CPPFLAGS = ${TCL_CPPFLAGS}

noinst_PROGRAMS = \
	pc_test \
	solver_test

noinst_HEADERS = \
	pc_constraint.h \
	pcBasic.h \
	pcInterval.h \
	pcVariable.h

libpc_nil_la_SOURCES = \
	pc_main.c \
	pc_solver.cpp

libpc_la_SOURCES =
libpc_la_LDFLAGS = -version-info 19:1 -no-undefined
libpc_la_LIBADD = \
	libpc_nil.la \
	${PC_LIBS}

pc_test_SOURCES = pc_test.c
pc_test_LDADD = libpc.la

solver_test_SOURCES = solver_test.cpp
solver_test_LDADD = libpc.la

FAST_OBJECTS = \
	$(libpc_nil_la_OBJECTS) \
	$(libpc_la_OBJECTS) \
	$(lib_LTLIBRARIES) \
	$(noinst_LTLIBRARIS) \
	$(pc_test_OBJECTS) \
	$(solver_test_OBJECTS) \
	$(noinst_PROGRAMS)

include $(top_srcdir)/misc/Makefile.defs
