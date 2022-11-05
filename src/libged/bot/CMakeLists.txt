set(BOT_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(BOT_LOCAL_INCLUDE_DIRS
  ${BRLCAD_SOURCE_DIR}/src/libbu # for fort.h
  )

set(BOT_LIBS libged libbg libbu)

if (BRLCAD_ENABLE_OPENMESH)
  set(BOT_LOCAL_INCLUDE_DIRS ${BOT_LOCAL_INCLUDE_DIRS} ${OPENMESH_INCLUDE_DIRS})
  set(BOT_LIBS ${BOT_LIBS} ${OPENMESH_LIBRARIES})
  add_compile_definitions(BUILD_OPENMESH_TOOLS)
  add_compile_definitions(_USE_MATH_DEFINES)
endif (BRLCAD_ENABLE_OPENMESH)

if (BRLCAD_ENABLE_OPENVDB)
  set(BOT_LOCAL_INCLUDE_DIRS
    ${BOT_LOCAL_INCLUDE_DIRS}
    ${OpenVDB_INCLUDE_DIRS}
    ${Tbb_INCLUDE_DIRS}
    ${IlmBase_INCLUDE_DIRS}
    )
  set(BOT_LIBS
    ${BOT_LIBS}
    ${OpenVDB_LIBRARIES}
    ${IlmBase_LIBRARIES}
    ${Tbb_LIBRARIES}
    )
  # disable warnings due to unquellable issues in Boost and IlmBase/half headers
  set_property(SOURCE remesh.cpp APPEND PROPERTY COMPILE_FLAGS "${OpenVDB_DEFINITIONS} -w")
endif (BRLCAD_ENABLE_OPENVDB)

# We want to include 3rd party libraries with -isystem - use
# BRLCAD_LIB_INCLUDE_DIRS for that purpose
BRLCAD_LIB_INCLUDE_DIRS(bot BOT_INCLUDE_DIRS BOT_LOCAL_INCLUDE_DIRS)

set(BOT_SRCS
  bot_condense.c
  bot.cpp
  bot_decimate.c
  bot_dump.c
  bot_exterior.c
  bot_face_fuse.c
  bot_face_sort.c
  bot_flip.c
  bot_fuse.c
  bot_merge.c
  bot_smooth.c
  bot_split.c
  bot_sync.c
  bot_vertex_fuse.c
  edbot.c
  check.cpp
  decimate.cpp
  extrude.cpp
  remesh.cpp
  smooth.cpp
  subdivide.cpp
  ../../libbu/fort.c
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-bot SHARED ${BOT_SRCS})
target_link_libraries(ged-bot ${BOT_LIBS})
set_property(TARGET ged-bot APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
VALIDATE_STYLE(ged-bot ${BOT_SRCS})
PLUGIN_SETUP(ged-bot ged)

CMAKEFILES(
  CMakeLists.txt
  ${BOT_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

