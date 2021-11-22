# Include directories needed by libwdb users
set(WDB_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${NMG_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${OPENNURBS_INCLUDE_DIRS}
  )

BRLCAD_LIB_INCLUDE_DIRS(wdb WDB_INCLUDE_DIRS "")


set(LIBWDB_SOURCES
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

BRLCAD_ADDLIB(libwdb "${LIBWDB_SOURCES}" "librt;libnmg;libbn;libbu;${OPENNURBS_LIBRARIES}")
set_target_properties(libwdb PROPERTIES VERSION 20.0.1 SOVERSION 20)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
