# declare all files regardless of compilation
set(art_ignore_files
  CMakeLists.txt
  art.h
  art.cpp
  brlcadplugin.h
  brlcadplugin.cpp
  )
CMAKEFILES(${art_ignore_files})

find_package(Appleseed)
if(NOT Appleseed_FOUND)
  message("'art' appleseed ray tracing is DISABLED")
  return()
endif(NOT Appleseed_FOUND)
message("Found Appleseed: ${Appleseed_LIBRARIES}")

find_package(Boost COMPONENTS atomic chrono date_time python regex serialization system thread wave)
if(NOT Boost_FOUND)
  message("'art' appleseed ray tracing is DISABLED")
  return()
endif(NOT Boost_FOUND)
message("Found Boost: ${Boost_LIBRARIES}")


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

get_filename_component(Appleseed_LIB_DIR "${Appleseed_LIBRARY}" DIRECTORY)
file(GLOB Appleseed_BOOST_SYSTEM_LIBS "${Appleseed_LIB_DIR}/libboost_system-*")
file(GLOB Appleseed_OPENIMAGEIO_LIBS "${Appleseed_LIB_DIR}/libOpenImageIO.*")

set(ART_LIBS
  Threads::Threads
  libged
  librt
  libnmg
  libdm
  liboptical
  libicv
  ${Appleseed_LIBRARIES}
  ${Appleseed_BOOST_SYSTEM_LIBS}
  ${Appleseed_OPENIMAGEIO_LIBS}
  ${M_LIBRARY}
  )
#  ${Boost_LIBRARIES}

set(ART_SRCS
  art.cpp
  brlcadplugin.cpp
  ../rt/opt.c
  ../rt/usage.cpp
  )

BRLCAD_ADDEXEC(art "${ART_SRCS}" "${ART_LIBS}" FOLDER Rt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
