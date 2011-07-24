include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${ZLIB_INCLUDE_DIR}
  ${OPENNURBS_INCLUDE_DIR}
  ${TCL_INCLUDE_DIRS}
)

BRLCAD_ADDEXEC(bottest bottest.c libwdb)
BRLCAD_ADDEXEC(brep_cube brep_cube.cpp "libwdb ${OPENNURBS_LIBRARY}")
set_target_properties(brep_cube PROPERTIES COMPILE_DEFINITIONS "OBJ_BREP=1")
BRLCAD_ADDEXEC(brep_simple brep_simple.cpp "libwdb ${OPENNURBS_LIBRARY}")
set_target_properties(brep_simple PROPERTIES COMPILE_DEFINITIONS "OBJ_BREP=1")
BRLCAD_ADDEXEC(brepintersect brepintersect.cpp "libwdb ${OPENNURBS_LIBRARY}")
set_target_properties(brepintersect PROPERTIES COMPILE_DEFINITIONS "OBJ_BREP=1")
BRLCAD_ADDEXEC(breplicator breplicator.cpp "libwdb ${OPENNURBS_LIBRARY}")
set_target_properties(breplicator PROPERTIES COMPILE_DEFINITIONS "OBJ_BREP=1")
BRLCAD_ADDEXEC(brickwall brickwall.c libwdb)
BRLCAD_ADDEXEC(clutter "clutter.c common.c" "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(contours contours.c libwdb)
BRLCAD_ADDEXEC(csgbrep csgbrep.cpp libwdb)
BRLCAD_ADDEXEC(globe globe.c libwdb)
BRLCAD_ADDEXEC(kurt kurt.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(lens lens.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(masonry masonry.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(metaball metaball.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(mkbuilding "mkbuilding.c makebuilding/makebuilding.c" libwdb)
BRLCAD_ADDEXEC(molecule molecule.c libwdb)
BRLCAD_ADDEXEC(nmgmodel nmgmodel.c libwdb)
BRLCAD_ADDEXEC(pipe pipe.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(pipetest pipetest.c libwdb)
BRLCAD_ADDEXEC(pix2g pix2g.c libwdb)
BRLCAD_ADDEXEC(pyramid pyramid.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(rawbot rawbot.c libwdb)
BRLCAD_ADDEXEC(ringworld ringworld.c libwdb)
BRLCAD_ADDEXEC(room "room.c common.c" "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(sketch sketch.c libwdb)
BRLCAD_ADDEXEC(sphflake sphflake.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(spltest spltest.c libwdb)
BRLCAD_ADDEXEC(surfaceintersect surfaceintersect.cpp "libwdb ${OPENNURBS_LIBRARY}")
set_target_properties(surfaceintersect PROPERTIES COMPILE_DEFINITIONS "OBJ_BREP=1")
BRLCAD_ADDEXEC(tea tea.c libwdb)
BRLCAD_ADDEXEC(tea_nmg tea_nmg.c libwdb)
BRLCAD_ADDEXEC(wavy wavy.c libwdb)
BRLCAD_ADDEXEC(torii torii.c libwdb)
BRLCAD_ADDEXEC(tube tube.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(vegetation vegetation.c "libwdb ${M_LIBRARY}")
BRLCAD_ADDEXEC(wdb_example wdb_example.c libwdb)

set(proc-db_ignore_files
	brepintersect.h
	metaballs.pl
	mkbuilding.h
	sgi.sh
	spiral.pl
	surfaceintersect.h
	tea.h
	vegetation.h
)
CMAKEFILES(${proc-db_ignore_files})
