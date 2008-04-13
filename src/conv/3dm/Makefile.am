# $Id$

AM_CPPFLAGS = ${TCL_CPPFLAGS} -Wall

bin_PROGRAMS = 3dm-g

3dm_g_SOURCES = 3dm-g.cpp

if BUILD_OPENNURBS
3dm_g_CXXFLAGS = -DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS}
else
3dm_g_CXXFLAGS =
endif

3dm_g_LDADD = \
	${WDB} \
	${WDB_LIBS} \
	${OPENNURBS}

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(3dm-g_OBJECTS) \
	$(bin_PROGRAMS)
