add_subdirectory(conv/obj/wfobj)

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
  conv/brlcad.c
  conv/fastgen4/fastgen4_read.c
  conv/fastgen4/fastgen4_write.cpp
  conv/obj/obj_read.c
  conv/obj/obj_write.c
  conv/stl/stl_read.c
  conv/stl/stl_write.c
  conv/vrml/vrml_write.c
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
  conv/fastgen4/NOTES
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
