function(GET_SOURCE_DIRECTORIES result subdir)
  set(dirlist "")
  file(GLOB cmakelist_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${subdir}/*/CMakeLists.txt)

  foreach(file ${cmakelist_files})
    get_filename_component(parent_dir "${file}" DIRECTORY)
    list(APPEND dirlist ${parent_dir})
  endforeach(file ${cmakelist_files})

  set (${result} ${dirlist} PARENT_SCOPE)
endfunction(GET_SOURCE_DIRECTORIES result)

set(LIBGCV_PLUGIN_LIBRARIES "")
GET_SOURCE_DIRECTORIES(plugin_directories plugins)

foreach(plugin_directory ${plugin_directories})
  add_subdirectory(${plugin_directory})
  get_filename_component(plugin_name "${plugin_directory}" NAME)
  list(APPEND LIBGCV_PLUGIN_LIBRARIES gcv_${plugin_name}-static)
endforeach(plugin_directory ${plugin_directories})


#################################################################################


# Include directories needed by libgcv users
set(GCV_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(gcv GCV_INCLUDE_DIRS "")


set(LIBGCV_SOURCES
  bot_solidity.c
  bottess.c
  facetize.c
  filter.c
  region_end.c
  region_end_mc.c
  tri_intersect.c
  plugins/brlcad.c
  )


BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "librt;libwdb;libbu;${LIBGCV_PLUGIN_LIBRARIES}" SO_SRCS "${GCV_SO_SRCS}" STATIC_SRCS "${GCV_STATIC_SRCS}")
set_target_properties(libgcv PROPERTIES VERSION 20.0.1 SOVERSION 20)


enable_testing()
BRLCAD_ADDEXEC(test_bottess test_bottess.c libgcv NO_INSTALL)
if(CPP_DLL_DEFINES)
  set_property(TARGET test_bottess APPEND PROPERTY COMPILE_DEFINITIONS "BOTTESS_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)
add_test(NAME bottess_test COMMAND test_bottess)


set(GCV_IGNORE_FILES
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
