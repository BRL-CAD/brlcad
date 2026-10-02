find_package_eigen(REQUIRED)
brlcad_find_package(manifold REQUIRED)
BRLCAD_Find_Package(TinyGLTF REQUIRED)

set(BOT_LIBS libged libbg libbu manifold::manifold tinygltf::tinygltf ${WINSOCK_LIB})


if(BRLCAD_ENABLE_OPENVDB)
  list(APPEND BOT_LIBS OpenVDB::openvdb)
  set(BOT_LOCAL_INCLUDE_DIRS ${BOT_LOCAL_INCLUDE_DIRS} ${OpenVDB_INCLUDE_DIRS} ${Tbb_INCLUDE_DIRS} ${IlmBase_INCLUDE_DIRS})
  set(BOT_OPENVDB_DEFS HAVE_OPENVDB_BOT_EXTERIOR BRLCAD_OPENVDB)
endif(BRLCAD_ENABLE_OPENVDB)

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
  exterior.cpp
  extrude.cpp
  info.cpp
  pick.cpp
  remesh.cpp
  repair.cpp
  smooth.cpp
  subdivide.cpp
)

if(BRLCAD_ENABLE_OPENVDB)
  # Suppress warnings from OpenVDB/Boost headers and prevent the OpenVDB
  # LeafManager UINT64_MAX sentinel from triggering the LTO
  # -Werror=alloc-size-larger-than false positive.  Because ged-bot is
  # PLUGIN_ONLY these flags are scoped to the plugin .so and do not
  # pollute the main libged target.
  set(BOT_OPENVDB_SRCS bot_flood_exterior.cpp bot_openvdb.cpp)
  if(MSVC)
    # OpenVDB templates exceed the standard COFF section limit in Debug builds.
    set(BOT_OPENVDB_COMPILE_OPTIONS /w)
    set(BOT_COMPILE_OPTIONS /bigobj)
  else()
    set(BOT_OPENVDB_COMPILE_OPTIONS -w -fno-lto)
  endif()
  set_source_files_properties(
    ${BOT_OPENVDB_SRCS}
    PROPERTIES COMPILE_OPTIONS "${BOT_OPENVDB_COMPILE_OPTIONS}"
  )
  list(APPEND BOT_SRCS ${BOT_OPENVDB_SRCS})
endif(BRLCAD_ENABLE_OPENVDB)

set(
  BOT_LOCAL_INCLUDE_DIRS
  ${EIGEN3_INCLUDE_DIR}
  ${BOT_LOCAL_INCLUDE_DIRS}
)

# altivec vectorization is broken in our bundled version of eigen.
# looks like it was fixed in upstream long time ago, so just disable
# for now with EIGEN_DONT_VECTORIZE.
# TODO: remove this / retest after next Eigen upgrade.
#
# PLUGIN_ONLY ensures ged-bot is always built as a standalone .so even
# when LIBGED_STATIC_CORE=ON.  This keeps the OpenVDB warning suppressions
# (-w -fno-lto on the OpenVDB source files) out of the main libged target.
ged_plugin_library(ged-bot
  PLUGIN_ONLY
  ${BOT_SRCS}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  SYSTEM_INCLUDES ${BOT_LOCAL_INCLUDE_DIRS}
  LIBS ${BOT_LIBS}
  DEFINITIONS "EIGEN_DONT_VECTORIZE" ${BOT_OPENVDB_DEFS}
  COMPILE_OPTIONS ${BOT_COMPILE_OPTIONS}
)

cmakefiles(
  CMakeLists.txt
  bot_flood_exterior.cpp
  bot_openvdb.cpp
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
