
proc_db_libs = ${WDB} ${WDB_LIBS}

LDADD = ${proc_db_libs}

AM_CPPFLAGS = ${TCL_CPPFLAGS}
AM_CFLAGS = ${STRICT_FLAGS}

bin_PROGRAMS = \
	bottest \
	brep_cube \
	brep_simple \
	brepintersect \
	breplicator \
	brickwall \
	clutter \
	contours \
        csgbrep \
	globe \
	kurt \
	lens \
	masonry \
	metaball \
	mkbuilding \
	molecule \
	nmgmodel \
	pipe \
	pipetest \
	pix2g \
	pyramid \
	rawbot \
	ringworld \
	room \
	sketch \
	sphflake \
	spltest \
	surfaceintersect \
	tea \
	tea_nmg \
	torii \
	tube \
	vegetation \
	wavy \
	wdb_example

bottest_SOURCES = bottest.c

brep_cube_SOURCES = brep_cube.cpp
brep_simple_SOURCES = brep_simple.cpp
breplicator_SOURCES = breplicator.cpp
brepintersect_SOURCES = brepintersect.cpp
surfaceintersect_SOURCES = surfaceintersect.cpp

# per-target CPPFLAGS not available until automake 1.7+
brep_cube_CXXFLAGS = -DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS}
brep_simple_CXXFLAGS = -DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS}
breplicator_CXXFLAGS = -DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS}
brepintersect_CXXFLAGS = -DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS}
surfaceintersect_CXXFLAGS = -DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS}

brep_cube_LDADD = \
	${proc_db_libs} \
	${OPENNURBS}
brep_simple_LDADD = \
	${proc_db_libs} \
	${OPENNURBS}
breplicator_LDADD = \
	${proc_db_libs} \
	${OPENNURBS}
brepintersect_LDADD = \
	${proc_db_libs} \
	${OPENNURBS}
csgbrep_LDADD = \
	${proc_db_libs} \
	${OPENNURBS}
surfaceintersect_LDADD = \
	${proc_db_libs} \
	${OPENNURBS}

brickwall_SOURCES = brickwall.c
clutter_SOURCES = clutter.c common.c
contours_SOURCES = contours.c
csgbrep_SOURCES = csgbrep.cpp
globe_SOURCES = globe.c
kurt_SOURCES = kurt.c
lens_SOURCES = lens.c
masonry_SOURCES = masonry.c
metaball_SOURCES = metaball.c
mkbuilding_SOURCES = mkbuilding.c \
	makebuilding/makebuilding.c
molecule_SOURCES = molecule.c
nmgmodel_SOURCES = nmgmodel.c
pipe_SOURCES = pipe.c
pipetest_SOURCES = pipetest.c
pix2g_SOURCES = pix2g.c
pyramid_SOURCES = pyramid.c
rawbot_SOURCES = rawbot.c
ringworld_SOURCES = ringworld.c
room_SOURCES = room.c common.c
sketch_SOURCES = sketch.c
sphflake_SOURCES = sphflake.c
spltest_SOURCES = spltest.c
tea_SOURCES = tea.c
tea_nmg_SOURCES = tea_nmg.c
torii_SOURCES = torii.c
tube_SOURCES = tube.c
vegetation_SOURCES = vegetation.c
wavy_SOURCES = wavy.c
wdb_example_SOURCES = wdb_example.c

noinst_HEADERS = \
	brepintersect.h \
	mkbuilding.h \
	surfaceintersect.h \
	tea.h \
	vegetation.h

EXTRA_DIST = \
	metaballs.pl \
	sgi.sh \
	spiral.pl \
	CMakeLists.txt

DEPENDS = src/libwdb

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bottest_OBJECTS) \
	$(brep_simple_OBJECTS) \
	$(brep_cube_OBJECTS) \
	$(brepintersect_OBJECTS) \
	$(breplicator_OBJECTS) \
	$(brickwall_OBJECTS) \
	$(clutter_OBJECTS) \
	$(contours_OBJECTS) \
	$(csgbrep_OBJECTS) \
	$(globe_OBJECTS) \
	$(kurt_OBJECTS) \
	$(lens_OBJECTS) \
	$(masonry_OBJECTS) \
	$(molecule_OBJECTS) \
	$(nmgmodel_OBJECTS) \
	$(pipe_OBJECTS) \
	$(pipetest_OBJECTS) \
	$(pix2g_OBJECTS) \
	$(pyramid_OBJECTS) \
	$(rawbot_OBJECTS) \
	$(ringworld_OBJECTS) \
	$(room_OBJECTS) \
	$(sketch_OBJECTS) \
	$(sphflake_OBJECTS) \
	$(spltest_OBJECTS) \
	$(surfaceintersect_OBJECTS) \
	$(tea_OBJECTS) \
	$(tea_nmg_OBJECTS) \
	$(wavy_OBJECTS) \
	$(torii_OBJECTS) \
	$(tube_OBJECTS) \
	$(vegetation_OBJECTS) \
	$(wdb_example_OBJECTS) \
	$(bin_PROGRAMS)
