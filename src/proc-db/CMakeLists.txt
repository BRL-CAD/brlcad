# DEVELOPMENT DEMO & TESTING TOOLS
#
# these are development demonstrations and testing tools that are not
# intended for general use.  new tools should start as NO_INSTALL
# until they reach a state where they provide some conceivable value
# to an end user.  if a tool is fully developed, it should migrate to
# src/shapes or some similar location.
brlcad_addexec(bottest bottest.c libwdb NO_INSTALL)
brlcad_addexec(brep_cobb brep_cobb.cpp "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
brlcad_addexec(brep_cube "brep_cube.cpp;twistedcube.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
target_include_directories(brep_cube BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
brlcad_addexec(brep_invalid "brep_invalid.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
brlcad_addexec(brep_simple "brep_simple.cpp;twistedcube.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL) # NOTE: has manual page
target_include_directories(brep_simple BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
brlcad_addexec(brep_surface "brep_surface.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
brlcad_addexec(brep_trimmed "brep_trimmed.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
brlcad_addexec(breplicator breplicator.cpp "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
brlcad_addexec(csgbrep csgbrep.cpp "libwdb;libnmg" NO_STRICT NO_INSTALL)
brlcad_addexec(donuts donuts.c "libwdb" NO_STRICT NO_INSTALL)
brlcad_addexec(gtimes gtimes.c "libged;librt;libbu" NO_INSTALL)
brlcad_addexec(nmgmodel nmgmodel.c "libwdb;libnmg" NO_INSTALL)
brlcad_addexec(pipetest pipetest.c libwdb NO_INSTALL)
brlcad_addexec(spltest spltest.c "libwdb;libnmg" NO_INSTALL)
brlcad_addexec(surfaceintersect "surfaceintersect.cpp;twistedcube.cpp" "libwdb;${OPENNURBS_LIBRARIES}" NO_STRICT NO_INSTALL)
target_include_directories(surfaceintersect BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
brlcad_addexec(tea_nmg tea_nmg.c "libwdb;libnmg" NO_INSTALL)
target_include_directories(tea_nmg BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
brlcad_addexec(tube tube.c "libwdb;libnmg;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(wdb_example wdb_example.c libwdb NO_INSTALL) # NOTE: has manual page

# MORE THAN DEVELOPMENT TESTING
#
# these have reached a level of maturity where they could provide some
# value to an end user and aren't just debugging aids, but lack
# documentation or still don't have adequate usability that would
# compel them to live elsewhere in the package (e.g., src/shapes).
#
# we still DO NOT INSTALL them because they do not have documentation
# and impact our perceived quality and complexity.

brlcad_addexec(brickwall brickwall.c libwdb NO_INSTALL)
brlcad_addexec(cchannel cchannel.c "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(clutter "clutter.c;common.c" "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(globe globe.c libwdb NO_INSTALL)
brlcad_addexec(kurt kurt.c "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(lens lens.c "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(masonry masonry.c "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(menger menger.c "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(metaball metaball.c "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(mkbuilding "mkbuilding.c;makebuilding/makebuilding.c" libwdb NO_INSTALL)
target_include_directories(mkbuilding BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
brlcad_addexec(molecule molecule.c libwdb NO_INSTALL)
brlcad_addexec(pdb-g pdb-g.c libwdb NO_INSTALL)
brlcad_addexec(pipe pipe.c "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(pix2g pix2g.c libwdb NO_INSTALL)
brlcad_addexec(pyramid pyramid.c "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(ringworld ringworld.c libwdb NO_INSTALL)
brlcad_addexec(sketch sketch.c libwdb NO_INSTALL)
brlcad_addexec(sphflake sphflake.c "libwdb;${M_LIBRARY}" NO_INSTALL)
brlcad_addexec(tea tea.c "libwdb;libnmg" NO_INSTALL)
target_include_directories(tea BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
brlcad_addexec(torii torii.c libwdb NO_INSTALL)
brlcad_addexec(vegetation vegetation.c "libwdb;${M_LIBRARY}" NO_INSTALL)
target_include_directories(vegetation BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
brlcad_addexec(wavy wavy.c "libwdb;libnmg" NO_INSTALL)

set(
  proc-db_ignore_files
  CMakeLists.txt
  twistedcube.h
  metaballs.pl
  mkbuilding.h
  sgi.sh
  spiral.pl
  surfaceintersect.h
  tea.h
  vegetation.h
)
cmakefiles(${proc-db_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
