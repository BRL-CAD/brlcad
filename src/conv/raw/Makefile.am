
bin_PROGRAMS = raw-g

AM_CPPFLAGS = ${TCL_CPPFLAGS} ${OPENNURBS_CPPFLAGS}

raw_g_SOURCES = \
	Bot.cpp \
	raw-g.cpp \
	RegionList.cpp

raw_g_LDADD = ${WDB} ${WDB_LIBS} ${BU} ${BU_LIBS}

noinst_HEADERS = Bot.h \
	RegionList.h

EXTRA_DIST = \
	CMakeLists.txt

DEPS=src/libwdb src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(raw_g_OBJECTS) \
	$(bin_PROGRAMS)
