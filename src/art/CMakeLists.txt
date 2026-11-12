# declare all files regardless of compilation
set(
  art_ignore_files
  CMakeLists.txt
  art.h
  art.cpp
  materials.h
  materials.cpp
  brlcadplugin.h
  brlcadplugin.cpp
  tile.h
  tile.cpp
  intersections_test.cpp
)
cmakefiles(${art_ignore_files})

if(NOT Appleseed_ROOT)
  set(Appleseed_ROOT "${CMAKE_BINARY_DIR}")
endif()
find_package(Appleseed)
if(NOT Appleseed_FOUND)
  message("'art' appleseed ray tracing is DISABLED")
  return()
endif(NOT Appleseed_FOUND)
message("Found Appleseed: ${Appleseed_LIBRARIES}")

if(NOT Boost_ROOT)
  set(Boost_ROOT "${Appleseed_ROOT}")
endif()
set(Boost_USE_STATIC_LIBS OFF)
find_package(
  Boost CONFIG
  COMPONENTS regex
)
if(NOT Boost_FOUND)
  message("'art' appleseed ray tracing is DISABLED")
  return()
endif(NOT Boost_FOUND)
message("Found Boost: ${Boost_LIBRARIES}")

string(REPLACE "\\" "/" APPLESEED_CONF_ROOT ${Appleseed_ROOT}) # use forward slashes
config_h_append(BRLCAD "#define APPLESEED_ROOT \"${APPLESEED_CONF_ROOT}\"\n")

set(
  ART_LIBS
  Threads::Threads
  libged
  librt
  libnmg
  libdm
  liboptical
  libicv
  ${Appleseed_LIBRARIES}
  ${Boost_LIBRARIES}
  ${M_LIBRARY}
)

set(ART_INCLUDE_DIRS ${Appleseed_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})

set(
  ART_SRCS
  ../rt/do.c
  ../rt/opt.c
  ../rt/grid.c
  ../rt/usage.cpp
  art.cpp
  materials.cpp
  brlcadplugin.cpp
  tile.cpp
)

add_definitions(-DBOOST_ALL_DYN_LINK)
brlcad_addexec(art "${ART_SRCS}" "${ART_LIBS}" FOLDER Rt)
# The shared rt sources rely on translation-unit isolation for C globals.
set_target_properties(art PROPERTIES
  UNITY_BUILD OFF
  ART_RENDERER_PREFIX "${Appleseed_ROOT}"
)
target_include_directories(art BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(art SYSTEM PRIVATE ${ART_INCLUDE_DIRS})

if(BUILD_TESTING)
  brlcad_addexec(art_intersection_test "intersections_test.cpp;brlcadplugin.cpp" "${ART_LIBS};libwdb" TEST)
  set_target_properties(art_intersection_test PROPERTIES UNITY_BUILD OFF)
  target_include_directories(art_intersection_test SYSTEM PRIVATE ${ART_INCLUDE_DIRS})
  target_include_directories(art_intersection_test PRIVATE "${CMAKE_SOURCE_DIR}/regress/art")
endif()

# copy precompiled appleseed.dll into same dir as art.exe

if(CMAKE_SHARED_LIBRARY_SUFFIX STREQUAL ".dll")
  # assume the dll is in appleseed's bin dir
  string(REPLACE ".lib" ".dll" Appleseed_DLL ${Appleseed_LIBRARY})
  string(REPLACE "lib" "bin" Appleseed_DLL ${Appleseed_DLL})
  add_custom_command(
    TARGET art
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Installing appleseed.dll to $<TARGET_FILE_DIR:art>"
  )
  add_custom_command(
    TARGET art
    PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${Appleseed_DLL}" $<TARGET_FILE_DIR:art>
  )
endif(CMAKE_SHARED_LIBRARY_SUFFIX STREQUAL ".dll")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
