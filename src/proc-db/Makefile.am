# $Id$

proc_db_libs = \
	${WDB} \
	${RT} \
	${BN} \
	${BU} \
	${SYSV}

LDADD = ${proc_db_libs}
AM_CPPFLAGS = ${TCL_CPPFLAGS}

bin_PROGRAMS = \
	bottest \
	brep_simple \
	brep_cube \
	brickwall \
	clutter \
	contours \
	fence \
	globe \
	kurt \
	molecule \
	nmgmodel \
	picket_fence \
	pipe \
	pipetest \
	pix2g \
	pyramid \
	rawbot \
	room \
	sketch \
	sphflake \
	spltest \
	tea \
	tea_nmg \
	terrain \
	torii \
	tube \
	vegitation \
	wall \
	wdb_example

bottest_SOURCES = bottest.c

brep_cube_SOURCES = brep_cube.cpp
brep_simple_SOURCES = brep_simple.cpp
# per-target CPPFLAGS not available until automake 1.7+
if WITH_OPENNURBS
brep_cube_CXXFLAGS = -DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS}
brep_simple_CXXFLAGS = -DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS}
else !WITH_OPENNURBS
brep_cube_CXXFLAGS =
brep_simple_CXXFLAGS =
endif
brep_cube_LDADD = \
	${proc_db_libs} \
	${OPENNURBS}
brep_simple_LDADD = \
	${proc_db_libs} \
	${OPENNURBS}

brickwall_SOURCES = brickwall.c
brickwall_LDADD = \
	${WDB} \
	${RT} \
	${BN} \
	${BU}

clutter_SOURCES = clutter.c common.c

contours_SOURCES = contours.c
contours_LDADD = \
	${WDB} \
	${RT} \
	${BN} \
	${BU}

fence_SOURCES = fence.c
globe_SOURCES = globe.c
kurt_SOURCES = kurt.c
molecule_SOURCES = molecule.c
nmgmodel_SOURCES = nmgmodel.c
picket_fence_SOURCES = picket_fence.c
pipe_SOURCES = pipe.c
pipetest_SOURCES = pipetest.c
pix2g_SOURCES = pix2g.c
pyramid_SOURCES = pyramid.c
rawbot_SOURCES = rawbot.c
room_SOURCES = room.c common.c
sketch_SOURCES = sketch.c
sphflake_SOURCES = sphflake.c
spltest_SOURCES = spltest.c
tea_SOURCES = tea.c
tea_nmg_SOURCES = tea_nmg.c
terrain_SOURCES = terrain.c
torii_SOURCES = torii.c
tube_SOURCES = tube.c
vegitation_SOURCES = vegitation.c
wall_SOURCES = wall.c
wdb_example_SOURCES = wdb_example.c

noinst_HEADERS = \
	ducks.h \
	fence.h \
	patches.h \
	tea.h \
	vegitation.h

man_MANS = picket_fence.1

EXTRA_DIST = $(man_MANS)

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(bottest_OBJECTS) \
	$(brickwall_OBJECTS) \
	$(clutter_OBJECTS) \
	$(contours_OBJECTS) \
	$(fence_OBJECTS) \
	$(globe_OBJECTS) \
	$(kurt_OBJECTS) \
	$(molecule_OBJECTS) \
	$(nmgmodel_OBJECTS) \
	$(picket_fence_OBJECTS) \
	$(pipe_OBJECTS) \
	$(pipetest_OBJECTS) \
	$(pix2g_OBJECTS) \
	$(pyramid_OBJECTS) \
	$(rawbot_OBJECTS) \
	$(room_OBJECTS) \
	$(sketch_OBJECTS) \
	$(sphflake_OBJECTS) \
	$(spltest_OBJECTS) \
	$(tea_OBJECTS) \
	$(tea_nmg_OBJECTS) \
	$(terrain_OBJECTS) \
	$(torii_OBJECTS) \
	$(tube_OBJECTS) \
	$(vegitation_OBJECTS) \
	$(wall_OBJECTS) \
	$(wdb_example_OBJECTS) \
	$(bin_PROGRAMS)
