# declare all files regardless of compilation
set(art_ignore_files
  CMakeLists.txt
  art.h
  art.cpp
  brlcadplugin.h
  brlcadplugin.cpp
  tile.h
  tile.cpp
  )
CMAKEFILES(${art_ignore_files})

find_package(Appleseed)
if(NOT Appleseed_FOUND)
  message("'art' appleseed ray tracing is DISABLED")
  return()
endif(NOT Appleseed_FOUND)
message("Found Appleseed: ${Appleseed_LIBRARIES}")

set(Boost_USE_STATIC_LIBS OFF)
find_package(Boost COMPONENTS chrono date_time filesystem regex system thread wave)
if(NOT Boost_FOUND)
  message("'art' appleseed ray tracing is DISABLED")
  return()
endif(NOT Boost_FOUND)
message("Found Boost: ${Boost_LIBRARIES}")

string(REPLACE "\\" "/" APPLESEED_CONF_ROOT ${Appleseed_ROOT}) # use forward slashes
CONFIG_H_APPEND(BRLCAD "#define APPLESEED_ROOT \"${APPLESEED_CONF_ROOT}\"\n")

set(ART_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${OPTICAL_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${Appleseed_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES ART_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(ART_INCLUDE_DIRS)


set(ART_LIBS
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


set(ART_SRCS
  ../rt/do.c
  ../rt/opt.c
  ../rt/usage.cpp
  art.cpp
  brlcadplugin.cpp
  tile.cpp
  )

add_definitions(-DBOOST_ALL_DYN_LINK)
BRLCAD_ADDEXEC(art "${ART_SRCS}" "${ART_LIBS}" FOLDER Rt)


# copy precompiled appleseed.dll into same dir as art.exe

if (CMAKE_SHARED_LIBRARY_SUFFIX STREQUAL ".dll")
  # assume the dll is in appleseed's bin dir
  string(REPLACE ".lib" ".dll" Appleseed_DLL ${Appleseed_LIBRARY})
  string(REPLACE "lib" "bin" Appleseed_DLL ${Appleseed_DLL})
  add_custom_command(TARGET art	PRE_BUILD COMMAND ${CMAKE_COMMAND} -E echo "Installing appleseed.dll to $<TARGET_FILE_DIR:art>")
  add_custom_command(TARGET art PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${Appleseed_DLL}" $<TARGET_FILE_DIR:art>)
endif (CMAKE_SHARED_LIBRARY_SUFFIX STREQUAL ".dll")


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
