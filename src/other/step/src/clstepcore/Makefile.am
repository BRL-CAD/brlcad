lib_LTLIBRARIES = libstepcore.la
noinst_LTLIBRARIES = libstepcore_nil.la

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


libstepcore_nil_la_LDFLAGS =
libstepcore_nil_la_LIBADD = 

libstepcore_la_SOURCES = 
libstepcore_la_LDFLAGS =
libstepcore_la_LIBADD = \
	libstepcore_nil.la

EXTRA_DIST = \
	README \
	dict-pic.txt \
	notes.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libstepcore_la_OBJECTS)
