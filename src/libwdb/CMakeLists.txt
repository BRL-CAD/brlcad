set(
  LIBWDB_SOURCES
  annot.c
  arbn.c
  ars.c
  bot.c
  brep.cpp
  cline.c
  constraint.c
  material.c
  datum.c
  dsp.c
  dynamic_geometry.c
  ebm.c
  export.c
  extrude.c
  id.c
  mater.c
  nmg.c
  nurb.c
  pipe.c
  reg.c
  sketch.c
  submodel.c
  script.c
  units.c
  vol.c
  wdb.c
)

# Note - libwdb_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
brlcad_addlib(libwdb "${LIBWDB_SOURCES}" "${libwdb_deps};${OPENNURBS_LIBRARIES}" "" "")
set_target_properties(libwdb PROPERTIES VERSION 20.0.1 SOVERSION 20)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
