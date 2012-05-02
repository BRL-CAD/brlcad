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

get_property(WDB_DEFINES GLOBAL PROPERTY WDB_DEFINES)
LIST(APPEND WDB_DEFINES "OBJ_BREP=1")
if(CPP_DLL_DEFINES)
  LIST(APPEND WDB_DEFINES "ON_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)
set_property(GLOBAL PROPERTY WDB_DEFINES "${WDB_DEFINES}")

BRLCAD_ADDLIB(libwdb "${LIBWDB_SOURCES}" "librt;libbn;libbu;libsysv;${OPENNURBS_LIBRARY};${REGEX_LIBRARY};${ZLIB_LIBRARY}")
set_target_properties(libwdb PROPERTIES VERSION 19.0.1 SOVERSION 19)
if(CPP_DLL_DEFINES)
  if(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
    SET_PROPERTY(TARGET libwdb-static APPEND PROPERTY COMPILE_DEFINITIONS "OBJ_BREP=1")
    SET_PROPERTY(TARGET libwdb-static APPEND PROPERTY COMPILE_DEFINITIONS "ON_DLL_IMPORTS")
  endif(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
endif(CPP_DLL_DEFINES)

ADD_MAN_PAGES(3 libwdb.3)
CMAKEFILES(Makefile.am)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
