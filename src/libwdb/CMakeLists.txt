# Include directories needed by libwdb users
set(WDB_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${SYSV_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(wdb WDB_INCLUDE_DIRS "")

set(LIBWDB_SOURCES
  arbn.c
  ars.c
  bot.c
  brep.cpp
  cline.c
  constraint.c
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
  units.c
  vol.c
  wdb.c
  )

# Initialize libwdb_DEFINES in case of reconfiguration
set(libwdb_DEFINES "")

# Set libwdb compile definitions
get_property(libwdb_DEFINES GLOBAL PROPERTY libwdb_DEFINES)
LIST(APPEND libwdb_DEFINES "OBJ_BREP=1")
if(CPP_DLL_DEFINES)
  LIST(APPEND libwdb_DLL_DEFINES "ON_DLL_IMPORTS")
  set_property(GLOBAL PROPERTY libwdb_DLL_DEFINES "${libwdb_DLL_DEFINES}")
endif(CPP_DLL_DEFINES)
set_property(GLOBAL PROPERTY libwdb_DEFINES "${libwdb_DEFINES}")

BRLCAD_ADDLIB(libwdb "${LIBWDB_SOURCES}" "librt;libbn;libbu;libsysv;${OPENNURBS_LIBRARY};${REGEX_LIBRARY};${ZLIB_LIBRARY}")
set_target_properties(libwdb PROPERTIES VERSION 20.0.1 SOVERSION 20)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
