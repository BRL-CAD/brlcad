find_package_eigen(REQUIRED)
brlcad_find_package(manifold REQUIRED)

set(BOT_LIBS libged libbg libbu manifold::manifold)

brlcad_find_package(Geogram REQUIRED SYSPATTERN geogram)
set(BOT_LOCAL_INCLUDE_DIRS ${BOT_LOCAL_INCLUDE_DIRS} ${Geogram_INCLUDE_DIR})
set(BOT_LIBS ${BOT_LIBS} ${Geogram_LIBRARY})

if(BRLCAD_ENABLE_OPENMESH)
  brlcad_find_package(OpenMesh)
  if(OPENMESH_INCLUDE_DIRS)
    set(BOT_LOCAL_INCLUDE_DIRS ${BOT_LOCAL_INCLUDE_DIRS} ${OPENMESH_INCLUDE_DIRS})
    set(BOT_LIBS ${BOT_LIBS} ${OPENMESH_LIBRARIES})
    add_compile_definitions(BUILD_OPENMESH_TOOLS)
    add_compile_definitions(_USE_MATH_DEFINES)
  else(OPENMESH_INCLUDE_DIRS)
    message("BRLCAD_ENABLE_OPENMESH enabled, but OpenMesh not found - skipping")
  endif(OPENMESH_INCLUDE_DIRS)
endif(BRLCAD_ENABLE_OPENMESH)

if(BRLCAD_ENABLE_OPENVDB)
  set(BOT_LIBS ${BOT_LIBS} ${OpenVDB_LIBRARIES} ${IlmBase_LIBRARIES} ${Tbb_LIBRARIES})
  # disable warnings due to unquellable issues in Boost and IlmBase/half headers
  set_property(SOURCE remesh.cpp APPEND PROPERTY COMPILE_FLAGS "${OpenVDB_DEFINITIONS} -w")
  set(BOT_LOCAL_INCLUDE_DIRS ${BOT_LOCADL_INCLUDE_DIRS} ${OpenVDB_INCLUDE_DIRS} ${Tbb_INCLUDE_DIRS} ${IlmBase_INCLUDE_DIRS})
endif(BRLCAD_ENABLE_OPENVDB)

# TODO - investigate https://github.com/nissmar/VSA to see if it could be
# a viable way to simplify large, complex BoT primitives...

set(
  BOT_SRCS
  bot_condense.c
  bot.cpp
  bot_decimate.c
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
  dump/bot_dump.cpp
  dump/dxf.c
  dump/gltf.cpp
  dump/obj.c
  dump/sat.c
  dump/stl.c
  edbot.c
  check.cpp
  decimate.cpp
  extrude.cpp
  remesh.cpp
  repair.cpp
  smooth.cpp
  subdivide.cpp
  ../../libbu/fort.c
)

ged_plugin_library(ged-bot ${BOT_SRCS})
target_include_directories(
  ged-bot
  BEFORE
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${EIGEN3_INCLUDE_DIR} ${BRLCAD_SOURCE_DIR}/src/libbu
  ) # for fort.h
target_include_directories(ged-bot SYSTEM PRIVATE ${BOT_LOCAL_INCLUDE_DIRS})
target_link_libraries(ged-bot ${BOT_LIBS})

# altivec vectorization is broken in our bundled version of eigen.
# looks like it was fixed in upstream long time ago, so just disable
# for now.  TODO: remove this / retest after next Eigen upgrade.
set_property(TARGET ged-bot APPEND PROPERTY COMPILE_DEFINITIONS "EIGEN_DONT_VECTORIZE")


cmakefiles(
  CMakeLists.txt
  ${BOT_SRCS}
  dump/ged_bot_dump.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
