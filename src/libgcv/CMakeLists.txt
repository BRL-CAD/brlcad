
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
  plugin.c
  region_end.c
  region_end_mc.c
  tri_intersect.c
  bot_solidity.c
  )

list(APPEND LIBGCV_SOURCES plugin/setup.cpp)

BRLCAD_ADDLIB(libgcv "${LIBGCV_SOURCES}" "librt;libbu")
SET_TARGET_PROPERTIES(libgcv PROPERTIES VERSION 20.0.1 SOVERSION 20)

BRLCAD_ADDEXEC(gcv gcv.c libgcv)

add_subdirectory(wfobj)

enable_testing()
BRLCAD_ADDEXEC(test_bottess test_bottess.c libgcv NO_INSTALL)
if(CPP_DLL_DEFINES)
  SET_PROPERTY(TARGET test_bottess APPEND PROPERTY COMPILE_DEFINITIONS "BOTTESS_DLL_IMPORTS")
endif(CPP_DLL_DEFINES)
add_test(NAME bottess_test COMMAND test_bottess)

CMAKEFILES(NOTES plugin.h soup.h tri_intersect.h bot_solidity.h)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/CTestTestfile.cmake)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/wfobj/CTestTestfile.cmake)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
