lib_LTLIBRARIES = libstepeditor.la
noinst_LTLIBRARIES = libstepeditor_nil.la

libstepeditor_nil_la_SOURCES = \
	mgrnode.cc \
	mgrnodelist.cc \
	dispnode.cc \
	dispnodelist.cc \
	mgrnodearray.cc \
	cmdmgr.cc \
	instmgr.cc \
	s_HEADER_SCHEMA.cc \
	s_HEADER_SCHEMA.init.cc \
	STEPfile.cc \
	STEPfile.inline.cc


libstepeditor_nil_la_LDFLAGS =
libstepeditor_nil_la_LIBADD = 
libstepeditor_nil_la_CXXFLAGS = -I$(top_srcdir)/src/clstepcore -I$(top_srcdir)/src/clutils -I$(top_srcdir)/src/cldai

libstepeditor_la_SOURCES = 
libstepeditor_la_LDFLAGS =
libstepeditor_la_LIBADD = \
	libstepeditor_nil.la

noinst_HEADERS = \
	cmdmgr.h \
	dispnode.h \
	dispnodelist.h \
	editordefines.h \
	instmgr.h \
	mgrnodearray.h \
	mgrnode.h \
	mgrnodelist.h \
	seeinfodefault.h \
	s_HEADER_SCHEMA.h \
	STEPfile.h


include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libstepeditor_la_OBJECTS)
