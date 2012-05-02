
lib_LTLIBRARIES = libpc.la
noinst_LTLIBRARIES = libpc_nil.la

# TODO: check existing boost using AX_BOOST_BASE([MINIMUM-VERSION]) ? in configure.ac
#       and remove blind Include
AM_CPPFLAGS = \
	${BOOST_CPPFLAGS} \
	${TCL_CPPFLAGS}

# portability support for gcc 2.95, conditionally adds -ftemplate-depth-NN
AM_CXXFLAGS = ${TEMPLATE_DEPTH}

noinst_PROGRAMS = \
	solver_test \
	vm_test

noinst_HEADERS = \
	pcBasic.h \
	pcInterval.h \
	pcVariable.h \
	pcParameter.h \
	pcConstraint.h \
	pcVCSet.h \
	pcNetwork.h \
	pcGenerator.h \
	pcParser.h \
	pcMathVM.h \
	pcMathLF.h \
	pcMathGrammar.h \
	pcSolver.h

libpc_nil_la_SOURCES = \
	pc_main.c \
	pc_constraints.c \
	pcVariable.cpp \
	pcParameter.cpp \
	pcConstraint.cpp \
	pcVCSet.cpp \
	pcNetwork.cpp \
	pcGenerator.cpp \
	pcParser.cpp \
	pcMathVM.cpp

libpc_la_SOURCES =
libpc_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
libpc_la_LIBADD = \
	libpc_nil.la \
	${PC_LIBS}

solver_test_SOURCES = solver_test.cpp
solver_test_LDADD = libpc.la ${PC_LIBS}

vm_test_SOURCES = vm_test.cpp
vm_test_LDADD = libpc.la ${PC_LIBS}

EXTRA_DIST = NOTES CMakeLists.txt

FAST_OBJECTS = \
	$(libpc_nil_la_OBJECTS) \
	$(libpc_la_OBJECTS) \
	$(lib_LTLIBRARIES) \
	$(noinst_LTLIBRARIS) \
	$(pc_test_OBJECTS) \
	$(solver_test_OBJECTS) \
	$(noinst_PROGRAMS)

include $(top_srcdir)/misc/Makefile.defs
