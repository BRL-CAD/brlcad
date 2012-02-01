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

LIST(APPEND WDB_DEFINES "OBJ_BREP=1")
if(CPP_DLL_DEFINES)
  LIST(APPEND WDB_DEFINES "ON_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)

BRLCAD_ADDLIB(libwdb "${LIBWDB_SOURCES}" "librt libbn libbu libsysv ${OPENNURBS_LIBRARY} ${REGEX_LIBRARY} ${ZLIB_LIBRARY}")
SET_TARGET_PROPERTIES(libwdb PROPERTIES VERSION 19.0.1 SOVERSION 19)
if(CPP_DLL_DEFINES)
  if(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
    SET_PROPERTY(TARGET libwdb-static APPEND PROPERTY COMPILE_DEFINITIONS "OBJ_BREP=1")
    SET_PROPERTY(TARGET libwdb-static APPEND PROPERTY COMPILE_DEFINITIONS "ON_DLL_IMPORTS")
  endif(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
endif(CPP_DLL_DEFINES)

ADD_MAN_PAGE(3 libwdb.3)
CMAKEFILES(Makefile.am)
