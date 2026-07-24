set(PLUGIN_LIBS libged libbu)
set(REPAIR_SRCS repair.cpp)
brlcad_find_package(manifold REQUIRED)
list(APPEND PLUGIN_LIBS manifold::manifold)

set(
  REPAIR_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(BRLCAD_ENABLE_OPENVDB)
  brlcad_find_package(OpenVDB REQUIRED)
  list(APPEND REPAIR_SRCS ../bot/bot_openvdb.cpp)
  list(APPEND PLUGIN_LIBS OpenVDB::openvdb)
  set(
    REPAIR_LOCAL_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../bot
  )
  set(REPAIR_OPENVDB_DEFS BRLCAD_OPENVDB)
  # OpenVDB's transitive headers trigger warnings not relevant to this source,
  # and its LeafManager sentinel trips a GCC LTO false positive.
  set_source_files_properties(../bot/bot_openvdb.cpp PROPERTIES COMPILE_FLAGS "-w -fno-lto")
endif(BRLCAD_ENABLE_OPENVDB)

ged_plugin_library(ged-repair
  ${REPAIR_SRCS}
  INCLUDES ${REPAIR_INCLUDE_DIRS}
  SYSTEM_INCLUDES ${REPAIR_LOCAL_INCLUDE_DIRS}
  LIBS ${PLUGIN_LIBS}
  DEFINITIONS ${REPAIR_OPENVDB_DEFS}
  PLUGIN_ONLY
)

cmakefiles(
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
