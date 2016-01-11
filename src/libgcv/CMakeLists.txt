add_subdirectory(plugins/obj/wfobj)

include_directories(
  ${WFOBJ_INCLUDE_DIRS}
)

# Include directories needed by libgcv users
set(GCV_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_LIB_INCLUDE_DIRS(gcv GCV_INCLUDE_DIRS "")


set(LIBGCV_SOURCES
  bottess.c
  facetize.c
  filter.c
  region_end.c
  region_end_mc.c
  tri_intersect.c
  bot_solidity.c
  )

set(LIBGCV_PLUGIN_SOURCES
  plugins/brlcad.c
  plugins/fastgen4/fastgen4_read.c
  plugins/fastgen4/fastgen4_write.cpp
  plugins/obj/obj_read.c
  plugins/obj/obj_write.c
  plugins/stl/stl_read.c
  plugins/stl/stl_write.c

  plugins/vrml/vrml_read/file_util.cpp
  plugins/vrml/vrml_read/node.cpp
  plugins/vrml/vrml_read/node_type.cpp
  plugins/vrml/vrml_read/parser.cpp
  plugins/vrml/vrml_read/string_util.cpp
  plugins/vrml/vrml_read/transform_node.cpp
  plugins/vrml/vrml_read/vrml_read.cpp

  plugins/vrml/vrml_write.c
  )

list(APPEND LIBGCV_SOURCES ${LIBGCV_PLUGIN_SOURCES})

BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "libwfobj;librt;libwdb;libbu" SO_SRCS "${GCV_SO_SRCS}" STATIC_SRCS "${GCV_STATIC_SRCS}")
set_target_properties(libgcv PROPERTIES VERSION 20.0.1 SOVERSION 20)

enable_testing()
BRLCAD_ADDEXEC(test_bottess test_bottess.c libgcv NO_INSTALL)
if(CPP_DLL_DEFINES)
  set_property(TARGET test_bottess APPEND PROPERTY COMPILE_DEFINITIONS "BOTTESS_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)
add_test(NAME bottess_test COMMAND test_bottess)

set(GCV_IGNORE_FILES
  NOTES
  plugins/fastgen4/NOTES
  soup.h
  tri_intersect.h
  plugins/vrml/vrml_read/file_util.h
  plugins/vrml/vrml_read/node.h
  plugins/vrml/vrml_read/node_type.h
  plugins/vrml/vrml_read/parser.h
  plugins/vrml/vrml_read/string_util.h
  plugins/vrml/vrml_read/transform_node.h
  )
CMAKEFILES(${GCV_IGNORE_FILES})

DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/CTestTestfile.cmake)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
