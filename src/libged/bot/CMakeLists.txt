find_package_eigen(REQUIRED)
BRLCAD_Find_Package(TinyGLTF REQUIRED)

set(BOT_LIBS libged libbg libbu tinygltf::tinygltf OpenVDB::openvdb ${WINSOCK_LIB})

# TODO - investigate https://github.com/nissmar/VSA to see if it could be
# a viable way to simplify large, complex BoT primitives...

set(
  BOT_SRCS
  bot_condense.c
  bot.cpp
  bot_decimate.c
  bot_face_fuse.c
  bot_face_sort.c
  bot_flip.c
  bot_flood_exterior.cpp
  bot_fuse.c
  bot_merge.c
  bot_openvdb.cpp
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
  exterior.cpp
  extrude.cpp
  info.cpp
  pick.cpp
  remesh.cpp
  repair.cpp
  smooth.cpp
  subdivide.cpp
)

# The OpenVDB LeafManager sentinel can trigger a false positive under LTO.
check_cxx_flag(fno-lto VARS BOT_OPENVDB_NO_LTO_FLAG)
set_property(
  SOURCE bot_flood_exterior.cpp bot_openvdb.cpp
  APPEND PROPERTY COMPILE_OPTIONS ${OPENVDB_NO_WARNINGS_FLAG} ${BOT_OPENVDB_NO_LTO_FLAG}
)

# altivec vectorization is broken in our bundled version of eigen.
# looks like it was fixed in upstream long time ago, so just disable
# for now with EIGEN_DONT_VECTORIZE.
# TODO: remove this / retest after next Eigen upgrade.
#
# Keep the OpenVDB compile options scoped to the plugin, including when
# LIBGED_STATIC_CORE is enabled.
ged_plugin_library(ged-bot
  PLUGIN_ONLY
  ${BOT_SRCS}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  SYSTEM_INCLUDES ${EIGEN3_INCLUDE_DIR}
  LIBS ${BOT_LIBS}
  DEFINITIONS "EIGEN_DONT_VECTORIZE"
)

cmakefiles(
  CMakeLists.txt
  bot_openvdb.h
  concurrentqueue.h
  dump/ged_bot_dump.h
  ged_bot.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
