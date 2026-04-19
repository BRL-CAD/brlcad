brlcad_find_package(manifold REQUIRED)
brlcad_find_package(Geogram REQUIRED SYSPATTERN geogram)
set(TESS_INCLUDE_DIRS ${Geogram_INCLUDE_DIR})
set(TESS_LIBS libged manifold::manifold ${Geogram_LIBRARY})

set(
  GED_TESS_SRCS
  brep_csg.cpp
  continuation.cpp
  sample.cpp
  spsr.cpp
  main.cpp
  nmg.cpp
  util.cpp
)

set(TESS_OPENVDB_DEFS "")

if(BRLCAD_ENABLE_OPENVDB)
  set(TESS_LIBS ${TESS_LIBS} ${OpenVDB_LIBRARIES} ${IlmBase_LIBRARIES} ${Tbb_LIBRARIES})
  set(TESS_INCLUDE_DIRS ${TESS_INCLUDE_DIRS} ${OpenVDB_INCLUDE_DIRS} ${Tbb_INCLUDE_DIRS} ${IlmBase_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../bot)
  set(TESS_OPENVDB_DEFS BRLCAD_OPENVDB)
  list(APPEND GED_TESS_SRCS ../../bot/bot_openvdb.cpp)
  # Suppress warnings from OpenVDB/Boost headers and prevent the OpenVDB
  # LeafManager UINT64_MAX sentinel from triggering the LTO
  # -Werror=alloc-size-larger-than false positive.
  set_source_files_properties(
    ../../bot/bot_openvdb.cpp
    PROPERTIES COMPILE_FLAGS "-w -fno-lto"
  )
endif(BRLCAD_ENABLE_OPENVDB)

ged_subprocess_library(facetize_process
  ${GED_TESS_SRCS}
  INCLUDES ${BOT_DIR}
  SYSTEM_INCLUDES ${TESS_INCLUDE_DIRS}
  LIBS ${TESS_LIBS}
  DEFINITIONS ${TESS_OPENVDB_DEFS}
)

cmakefiles(
  CMakeLists.txt
  tessellate.h
  ${GED_TESS_SRCS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
