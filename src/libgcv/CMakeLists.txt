# Include directories needed by libgcv users
set(GCV_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${NMG_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(gcv GCV_INCLUDE_DIRS "")

if(MSVC)
  set(LIBGCV_PLUGINS_PATH "${BIN_DIR}")
else(MSVC)
  set(LIBGCV_PLUGINS_PATH "${LIB_DIR}")
endif(MSVC)
set(LIBGCV_PLUGINS_DIRECTORY "libgcv_plugins")
add_subdirectory(plugins)

set(LIBGCV_SOURCES
  bottess.c
  facetize.c
  gcv.c
  region_end.c
  region_end_mc.c
  tri_intersect.c
  )


BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "librt;libwdb;libnmg;libbu" SHARED_SRCS "${GCV_SO_SRCS}" STATIC_SRCS "${GCV_STATIC_SRCS}")
set_target_properties(libgcv PROPERTIES VERSION 20.0.1 SOVERSION 20)
set_property(SOURCE gcv.c APPEND PROPERTY COMPILE_DEFINITIONS "LIBGCV_PLUGINS_DIRECTORY=\"${LIBGCV_PLUGINS_DIRECTORY}\"")
set_property(SOURCE gcv.c APPEND PROPERTY COMPILE_DEFINITIONS "LIBGCV_PLUGINS_PATH=\"${LIBGCV_PLUGINS_PATH}\"")
set_property(SOURCE gcv.c APPEND PROPERTY COMPILE_DEFINITIONS "LIBGCV_PLUGIN_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")

add_subdirectory(tests)

set(GCV_IGNORE_FILES
  CMakeLists.txt
  NOTES
  soup.h
  tri_intersect.h
  )
CMAKEFILES(${GCV_IGNORE_FILES})


DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/CTestTestfile.cmake)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
