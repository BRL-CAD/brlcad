
enable_testing()
add_test(bottess_test ${CMAKE_BINARY_DIR}/bin/test_bottess)

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
  region_end.c
  region_end_mc.c
  tri_intersect.c
  )

BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "librt;libbu")
SET_TARGET_PROPERTIES(libgcv PROPERTIES VERSION 19.0.1 SOVERSION 19)

add_subdirectory(wfobj)

BRLCAD_ADDEXEC(test_bottess test_bottess.c libgcv NO_INSTALL)
if(CPP_DLL_DEFINES)
  SET_PROPERTY(TARGET test_bottess APPEND PROPERTY COMPILE_DEFINITIONS "BOTTESS_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)

CMAKEFILES(NOTES Makefile.am soup.h tri_intersect.h)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/CTestTestfile.cmake)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/wfobj/CTestTestfile.cmake)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
