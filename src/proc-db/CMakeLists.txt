set(PROC_DB_INCLUDE_DIRS
  ${NMG_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )
list(REMOVE_DUPLICATES PROC_DB_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(PROC_DB_INCLUDE_DIRS)

# DEVELOPMENT DEMO & TESTING TOOLS
#
# these are development demonstrations and testing tools that are not
# intended for general use.  new tools should start as NO_INSTALL
# until they reach a state where they provide some conceivable value
# to an end user.  if a tool is fully developed, it should migrate to
# src/shapes or some similar location.
BRLCAD_ADDEXEC(bottest bottest.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(brep_cobb brep_cobb.cpp "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(brep_cube "brep_cube.cpp;twistedcube.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(brep_invalid "brep_invalid.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(brep_simple "brep_simple.cpp;twistedcube.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL) # NOTE: has manual page
BRLCAD_ADDEXEC(brep_surface "brep_surface.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(brep_trimmed "brep_trimmed.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(brepintersect brepintersect.cpp "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(breplicator breplicator.cpp "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(csgbrep csgbrep.cpp "libwdb;libnmg" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(donuts donuts.c "libwdb" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(gtimes gtimes.c "libged;librt;libbu" NO_INSTALL)
BRLCAD_ADDEXEC(nmgmodel nmgmodel.c "libwdb;libnmg" NO_INSTALL)
BRLCAD_ADDEXEC(pipetest pipetest.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(spltest spltest.c "libwdb;libnmg" NO_INSTALL)
BRLCAD_ADDEXEC(surfaceintersect "surfaceintersect.cpp;twistedcube.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
BRLCAD_ADDEXEC(tea_nmg tea_nmg.c "libwdb;libnmg" NO_INSTALL)
BRLCAD_ADDEXEC(tube tube.c "libwdb;libnmg;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(wdb_example wdb_example.c libwdb NO_INSTALL) # NOTE: has manual page

# MORE THAN DEVELOPMENT TESTING
#
# these have reached a level of maturity where they could provide some
# value to an end user and aren't just debugging aids, but lack
# documentation or still don't have adequate usability that would
# compel them to live elsewhere in the package (e.g., src/shapes).
#
# we still DO NOT INSTALL them because they do not have documentation
# and impact our perceived quality and complexity.

BRLCAD_ADDEXEC(brickwall brickwall.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(cchannel cchannel.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(clutter "clutter.c;common.c" "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(globe globe.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(kurt kurt.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(lens lens.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(masonry masonry.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(menger menger.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(metaball metaball.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(mkbuilding "mkbuilding.c;makebuilding/makebuilding.c" libwdb NO_INSTALL)
BRLCAD_ADDEXEC(molecule molecule.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(pdb-g pdb-g.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(pipe pipe.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(pix2g pix2g.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(pyramid pyramid.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(ringworld ringworld.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(sketch sketch.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(sphflake sphflake.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(tea tea.c "libwdb;libnmg" NO_INSTALL)
BRLCAD_ADDEXEC(torii torii.c libwdb NO_INSTALL)
BRLCAD_ADDEXEC(vegetation vegetation.c "libwdb;${M_LIBRARY}" NO_INSTALL)
BRLCAD_ADDEXEC(wavy wavy.c "libwdb;libnmg" NO_INSTALL)


set(proc-db_ignore_files
  CMakeLists.txt
  twistedcube.h
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

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
