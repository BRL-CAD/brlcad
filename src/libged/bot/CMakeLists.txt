set(BOT_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

set(BOT_LOCAL_INCLUDE_DIRS
  ${BRLCAD_SOURCE_DIR}/src/libbu # for fort.h
  ${OPENMESH_INCLUDE_DIRS}
  )

BRLCAD_LIB_INCLUDE_DIRS(bot BOT_INCLUDE_DIRS BOT_LOCAL_INCLUDE_DIRS)
add_compile_definitions(BUILD_OPENMESH_TOOLS)
if(MSVC)
  add_compile_definitions(_USE_MATH_DEFINES)
endif(MSVC)

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
  extrude.cpp
  remesh.cpp
  openmesh_tools.cpp
  ../../libbu/fort.c
  )

add_definitions(-DGED_PLUGIN)
ged_plugin_library(ged-bot SHARED ${BOT_SRCS})
target_link_libraries(ged-bot libged libbg libbu ${OPENMESH_LIBRARIES})
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

