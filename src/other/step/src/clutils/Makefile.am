lib_LTLIBRARIES = libsteputils.la
noinst_LTLIBRARIES = libsteputils_nil.la

AM_CPPFLAGS = ${NOWARN}

libsteputils_nil_la_SOURCES = \
	Str.cc \
	dirobj.cc \
	gennode.cc \
	gennodelist.cc \
	gennodearray.cc \
	scl_hash.cc \
	errordesc.cc \
	scl_char_str_list.cc

libsteputils_nil_la_LDFLAGS = -avoid-version -no-undefined -Wc,${FEXCEPTIONS}
libsteputils_nil_la_LIBADD = 

libsteputils_la_SOURCES = 
libsteputils_la_LDFLAGS = -no-undefined -Wc,${FEXCEPTIONS} -version-info 19:1
libsteputils_la_LIBADD = \
	libsteputils_nil.la \
	${LIBSTDCXX}

noinst_HEADERS = \
	dirobj.h \
	errordesc.h \
	gennodearray.h \
	gennode.h \
	gennodelist.h \
	scl_char_str_list.h \
	scldir.h \
	scl_hash.h \
	stat.h \
	Str.h \
	str_list.h


EXTRA_DIST = \
	README \
	CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libsteputils_la_OBJECTS)
