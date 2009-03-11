lib_LTLIBRARIES = libstepdai.la
noinst_LTLIBRARIES = libstepdai_nil.la

libstepdai_nil_la_SOURCES = \
	sdaiApplication_instance_set.cc \
	sdaiSession_instance.cc \
	sdaiObject.cc \
	sdaiDaObject.cc \
	sdaiEntity_extent.cc \
	sdaiEntity_extent_set.cc \
	sdaiModel_contents.cc \
	sdaiModel_contents_list.cc 

libstepdai_nil_la_LDFLAGS =
libstepdai_nil_la_LIBADD = 

libstepdai_la_SOURCES = 
libstepdai_la_LDFLAGS =
libstepdai_la_LIBADD = \
	libstepdai_nil.la

EXTRA_DIST = \
	ErrorMap.cc \
	ErrorRpt.cc  \
	StrUtil.cc

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libstepdai_la_OBJECTS)
