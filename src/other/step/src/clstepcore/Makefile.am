lib_LTLIBRARIES = libstepcore.la
noinst_LTLIBRARIES = libstepcore_nil.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/cldai \
	-I$(top_srcdir)/src/cleditor \
	-I$(top_srcdir)/src/clutils \
	${NOWARN}

libstepcore_nil_la_SOURCES = \
	sdaiApplication_instance.cc \
	STEPcomplex.cc \
	STEPattribute.cc \
	STEPattribute.inline.cc \
	sdai.cc \
	sdaiEnum.cc \
	sdaiString.cc \
	sdaiSelect.cc \
	sdaiBinary.cc \
	STEPaggregate.cc \
	STEPundefined.cc \
	STEPattributeList.cc \
	SingleLinkList.cc \
	SingleLinkList.inline.cc \
	Registry.inline.cc \
	ExpDict.cc \
	ExpDict.inline.cc \
	read_func.cc \
	collect.cc \
	complexlist.cc \
	entlist.cc \
	multlist.cc \
	orlist.cc \
	entnode.cc \
	non-ors.cc \
	match-ors.cc \
	trynext.cc \
	print.cc

noinst_HEADERS = \
	baseType.h \
	complexSupport.h \
	dictdefs.h \
	ExpDict.h \
	read_func.h \
	Registry.h \
	scl_osschema.h \
	sdaiApplication_instance.h \
	sdaiBinary.h \
	sdaiEnum.h \
	sdai.h \
	sdaiSelect.h \
	sdaiString.h \
	Select.h \
	SingleLinkList.h \
	STEPaggregate.h \
	STEPattribute.h \
	STEPattributeList.h \
	STEPcomplex.h \
	STEPundefined.h

libstepcore_nil_la_LDFLAGS = -avoid-version -Wc,${FEXCEPTIONS}

libstepcore_la_SOURCES = 
libstepcore_la_LDFLAGS = -Wc,${FEXCEPTIONS} -version-info 19:1
libstepcore_la_LIBADD = \
	libstepcore_nil.la \
	${CLUTILS} \
	${EXPRESS} \
	${LIBSTDCXX}

# FIXME: cyclic dependencies that need squashing
#	${CLDAI}
#	${CLEDITOR}

EXTRA_DIST = \
	ExpDict.h.s \
	README \
	dict-pic.txt \
	notes.txt \
	CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libstepcore_la_OBJECTS)
