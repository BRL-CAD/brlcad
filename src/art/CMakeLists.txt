set(ART_INCLUDE_DIRS
  ${OPTICAL_INCLUDE_DIRS}
  ${FB_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

list(REMOVE_DUPLICATES ART_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(ART_INCLUDE_DIRS)

set(RT_STD_LIBS
  Threads::Threads
  librt
  libnmg
  libdm
  liboptical
  libicv
  ${M_LIBRARY}
  )

find_package(Appleseed)
if(Appleseed_FOUND)
  message("Found Appleseed: ${Appleseed_LIBRARIES}")

  # For some reason GCC won't suppress this warning with an in-src pragma push
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-Wno-unknown-pragmas HAVE_WNO_UNKNOWN_PRAGMAS)
  if(HAVE_WNO_UNKNOWN_PRAGMAS)
    set_property(SOURCE art.cpp APPEND PROPERTY COMPILE_FLAGS "-Wno-unknown-pragmas")
    set_property(SOURCE brlcadplugin.cpp APPEND PROPERTY COMPILE_FLAGS "-Wno-unknown-pragmas")
  endif(HAVE_WNO_UNKNOWN_PRAGMAS)

  # Using the binary version of appleseed on Linux, we need a couple specific additional
  # libraries linked in
  get_filename_component(Appleseed_LIB_DIR "${Appleseed_LIBRARY}" DIRECTORY)
  file(GLOB Appleseed_BOOST_SYSTEM_LIB "${Appleseed_LIB_DIR}/libboost_system-*")
  file(GLOB Appleseed_OPENIMAGEIO_LIB "${Appleseed_LIB_DIR}/libOpenImageIO.*")

  include_directories(
    ${Appleseed_INCLUDE_DIRS}
    )

  set(ART_SRCS
    art.cpp
    brlcadplugin.cpp
    ../rt/opt.c
    )

  BRLCAD_ADDEXEC(art "${ART_SRCS}" "${RT_STD_LIBS};${Appleseed_LIBRARIES};${Appleseed_BOOST_SYSTEM_LIB};${Appleseed_OPENIMAGEIO_LIB}" TEST_USESDATA)
  ADD_TARGET_DEPS(art dm_plugins)
endif(Appleseed_FOUND)

set(art_ignore_files
  CMakeLists.txt
  art.cpp
  art.h
  brlcadplugin.cpp
  brlcadplugin.h
  )
CMAKEFILES(${art_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
