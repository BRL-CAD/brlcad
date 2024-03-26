set(LIBGCV_PLUGINS_DIRECTORY "gcv")
add_subdirectory(plugins)

set(LIBGCV_SOURCES
  bottess.c
  facetize.c
  gcv.c
  region_end.c
  region_end_mc.c
  tri_intersect.c
  )


BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "${libgcv_deps}" "" "" SHARED_SRCS "${GCV_SO_SRCS}" STATIC_SRCS "${GCV_STATIC_SRCS}")
set_target_properties(libgcv PROPERTIES VERSION 20.0.1 SOVERSION 20)
set_property(SOURCE gcv.c APPEND PROPERTY COMPILE_DEFINITIONS "LIBGCV_PLUGINS_DIRECTORY=\"${LIBGCV_PLUGINS_DIRECTORY}\"")
set_property(SOURCE gcv.c APPEND PROPERTY COMPILE_DEFINITIONS "LIBGCV_PLUGINS_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")

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
