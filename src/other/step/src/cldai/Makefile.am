lib_LTLIBRARIES = libstepdai.la
noinst_LTLIBRARIES = libstepdai_nil.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/clstepcore \
	-I$(top_srcdir)/src/clutils \
	${NOWARN}

libstepdai_nil_la_SOURCES = \
	sdaiApplication_instance_set.cc \
	sdaiSession_instance.cc \
	sdaiObject.cc \
	sdaiDaObject.cc \
	sdaiEntity_extent.cc \
	sdaiEntity_extent_set.cc \
	sdaiModel_contents.cc \
	sdaiModel_contents_list.cc 

libstepdai_nil_la_LDFLAGS = -avoid-version -no-undefined -Wc,${FEXCEPTIONS}

libstepdai_la_SOURCES = 
libstepdai_la_LDFLAGS = -no-undefined -Wc,${FEXCEPTIONS} -version-info 19:1
libstepdai_la_LIBADD = \
	libstepdai_nil.la \
	${CLUTILS} \
	${CLSTEPCORE} \
	${LIBSTDCXX}

noinst_HEADERS = \
	ErrorMap.hh \
	ErrorRpt.hh \
	impconv.hh \
	imptypes.hh \
	sdaiApplication_instance_set.h \
	sdaiDaObject.h \
	sdaiEntity_extent.h \
	sdaiEntity_extent_set.h \
	sdaiModel_contents.h \
	sdaiModel_contents_list.h \
	sdaiObject.h \
	sdaiSession_instance.h \
	StrUtil.hh

EXTRA_DIST = \
	ErrorMap.cc \
	ErrorRpt.cc  \
	StrUtil.cc \
	CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libstepdai_la_OBJECTS)
