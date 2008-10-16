
bin_PROGRAMS = \
	bot-bldxf \
	dxf-g \
	g-dxf

AM_CPPFLAGS = ${TCL_CPPFLAGS}

bot_bldxf_SOURCES = bot-bldxf.c
bot_bldxf_LDADD = ${RT}

dxf_g_SOURCES = dxf-g.c
dxf_g_LDADD = ${WDB}

g_dxf_SOURCES = g-dxf.c
g_dxf_LDADD = ${WDB} ${GCV}

dist_man_MANS = \
	dxf-g.1 \
	g-dxf.1

noinst_HEADERS = dxf.h

EXTRA_DIST = \
	CMakeLists.txt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bot_bldxf_OBJECTS) \
	$(dxf_g_OBJECTS) \
	$(g_dxf_OBJECTS) \
	$(bin_PROGRAMS)
