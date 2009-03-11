lib_LTLIBRARIES = libsteputils.la
noinst_LTLIBRARIES = libsteputils_nil.la

libsteputils_nil_la_SOURCES = \
	Str.cc \
	gennode.cc \
	gennodelist.cc \
	gennodearray.cc \
	scl_hash.cc \
	errordesc.cc \
	scl_string.cc \
	scl_char_str_list.cc

libsteputils_nil_la_LDFLAGS =
libsteputils_nil_la_LIBADD = 

libsteputils_la_SOURCES = 
libsteputils_la_LDFLAGS =
libsteputils_la_LIBADD = \
	libsteputils_nil.la

EXTRA_DIST = \
	dirobj.cc \
	README

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libsteputils_la_OBJECTS)
