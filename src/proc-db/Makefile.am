# $Id$


if WITH_OPENNURBS
BREP_CPPFLAGS=-DOBJ_BREP=1 ${OPENNURBS_CPPFLAGS}
BREP_PROGRAM=brep_cube
else
BREP_CPPFLAGS=
BREP_PROGRAM=
endif

# apply-mdb was removed due to compile issues... should be readded once those are resolved...
# nmgxplode was removed because nmgxplode.c doesn't seem to compile
bin_PROGRAMS = \
	$(BREP_PROGRAM) \
	bottest \
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

noinst_HEADERS = \
	ducks.h \
	fence.h \
	patches.h \
	tea.h \
	vegitation.h

man_MANS = picket_fence.1

AM_CPPFLAGS = ${TCL_CPPFLAGS} ${BREP_CPPFLAGS}


# XXX does not compile?
#apply_mdb_SOURCES = apply-mdb.c
#apply_mdb_LDADD = ${WDB}

bottest_SOURCES = bottest.c
bottest_LDADD = ${WDB} ${SYSV}

if WITH_OPENNURBS
brep_cube_SOURCES = brep_cube.cpp
brep_cube_LDADD = ${WDB} ${SYSV}
endif

brickwall_SOURCES = brickwall.c
brickwall_LDADD = ${WDB}

clutter_SOURCES = clutter.c common.c
clutter_LDADD = ${WDB} ${SYSV}

contours_SOURCES = contours.c
contours_LDADD = ${WDB}

fence_SOURCES = fence.c
fence_LDADD = ${WDB} ${SYSV}

globe_SOURCES = globe.c
globe_LDADD = ${WDB} ${SYSV}

kurt_SOURCES = kurt.c
kurt_LDADD = ${WDB} ${SYSV}

molecule_SOURCES = molecule.c
molecule_LDADD = ${WDB} ${SYSV}

nmgmodel_SOURCES = nmgmodel.c
nmgmodel_LDADD = ${WDB} ${SYSV}

#nmgxplode_SOURCES = nmgxplode.c
#nmgxplode_LDADD = ${WDB}

picket_fence_SOURCES = picket_fence.c
picket_fence_LDADD = ${WDB} ${SYSV}

pipe_SOURCES = pipe.c
pipe_LDADD = ${WDB} ${SYSV}

pipetest_SOURCES = pipetest.c
pipetest_LDADD = ${WDB} ${SYSV}

pix2g_SOURCES = pix2g.c
pix2g_LDADD = ${WDB}

pyramid_SOURCES = pyramid.c
pyramid_LDADD = ${WDB} ${SYSV}

rawbot_SOURCES = rawbot.c
rawbot_LDADD = ${WDB} ${SYSV}

room_SOURCES = room.c common.c
room_LDADD = ${WDB} ${SYSV}

sketch_SOURCES = sketch.c
sketch_LDADD = ${WDB} ${SYSV}

sphflake_SOURCES = sphflake.c
sphflake_LDADD = ${WDB} ${SYSV}

spltest_SOURCES = spltest.c
spltest_LDADD = ${WDB} ${SYSV}

tea_SOURCES = tea.c
tea_LDADD = ${WDB} ${SYSV}

tea_nmg_SOURCES = tea_nmg.c
tea_nmg_LDADD = ${WDB} ${SYSV}

terrain_SOURCES = terrain.c
terrain_LDADD = ${WDB} ${SYSV}

torii_SOURCES = torii.c
torii_LDADD = ${WDB} ${SYSV}

tube_SOURCES = tube.c
tube_LDADD = ${WDB} ${SYSV}

vegitation_SOURCES = vegitation.c
vegitation_LDADD = ${WDB} ${SYSV}

wall_SOURCES = wall.c
wall_LDADD = ${WDB} ${SYSV}

wdb_example_SOURCES = wdb_example.c
wdb_example_LDADD = ${WDB} ${SYSV}

# XXX .c added until they compile
EXTRA_DIST = \
	$(man_MANS) \
	apply-mdb.c \
	nmgxplode.c

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
