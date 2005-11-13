bin_PROGRAMS = \
	g-off \
	off-g

g_off_SOURCES = g-off.c
g_off_LDADD = ${LIBRT}

off_g_SOURCES = off-g.c
off_g_LDADD = ${LIBRT} ${WDB}

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(g_off_OBJECTS) \
	$(off_g_OBJECTS) \
	$(bin_PROGRAMS)
