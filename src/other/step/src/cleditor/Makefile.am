lib_LTLIBRARIES = libstepeditor.la
noinst_LTLIBRARIES = libstepeditor_nil.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/clstepcore \
	-I$(top_srcdir)/src/clutils \
	-I$(top_srcdir)/src/cldai \
	${NOWARN}

libstepeditor_nil_la_SOURCES = \
	STEPfile.cc \
	STEPfile.inline.cc \
	cmdmgr.cc \
	dispnode.cc \
	dispnodelist.cc \
	instmgr.cc \
	mgrnode.cc \
	mgrnodearray.cc \
	mgrnodelist.cc \
	needFunc.cc \
	s_HEADER_SCHEMA.cc \
	s_HEADER_SCHEMA.init.cc

libstepeditor_nil_la_LDFLAGS = -avoid-version -no-undefined -Wc,${FEXCEPTIONS}

libstepeditor_la_SOURCES = 
libstepeditor_la_LDFLAGS = -no-undefined -Wc,${FEXCEPTIONS} -version-info 19:1
libstepeditor_la_LIBADD = \
	libstepeditor_nil.la \
	${CLSTEPCORE} \
	${CLDAI} \
	${CLUTILS} \
	${LIBSTDCXX}

noinst_HEADERS = \
	STEPfile.h \
	cmdmgr.h \
	dispnode.h \
	dispnodelist.h \
	editordefines.h \
	instmgr.h \
	mgrnode.h \
	mgrnodearray.h \
	mgrnodelist.h \
	needFunc.h \
	s_HEADER_SCHEMA.h \
	seeinfodefault.h

EXTRA_DIST = CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libstepeditor_la_OBJECTS)
