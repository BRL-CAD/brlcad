set(ICV_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIRS}
  ${NETPBM_INCLUDE_DIR}
  )
BRLCAD_LIB_INCLUDE_DIRS(icv ICV_INCLUDE_DIRS "")

set(LIBICV_SOURCES
  fileformat.c
  rot.c
  color_space.c
  crop.c
  filter.c
  encoding.c
  operations.c
  stat.c
  size.c
  pix.c
  png.c
  ppm.c
  bw.c
  dpix.c
  )

BRLCAD_ADDLIB(libicv "${LIBICV_SOURCES}" "libbu;libbn;${PNG_LIBRARIES};${NETPBM_LIBRARY}")
if (TARGET png_stage)
  add_dependencies(libicv png_stage)
endif (TARGET png_stage)
set_target_properties(libicv PROPERTIES VERSION 20.0.1 SOVERSION 20)
if (HIDE_INTERNAL_SYMBOLS)
  if (TARGET netpbm OR HIDE_INTERNAL_SYMBOLS_EXT)
    set_property(TARGET libicv APPEND PROPERTY COMPILE_DEFINITIONS "NETPBM_DLL_IMPORTS")
  endif (TARGET netpbm OR HIDE_INTERNAL_SYMBOLS_EXT)
  if (TARGET libicv-obj)
    if (TARGET netpbm OR HIDE_INTERNAL_SYMBOLS_EXT)
      set_property(TARGET libicv-obj APPEND PROPERTY COMPILE_DEFINITIONS "NETPBM_DLL_IMPORTS")
    endif (TARGET netpbm OR HIDE_INTERNAL_SYMBOLS_EXT)
  endif (TARGET libicv-obj)
endif (HIDE_INTERNAL_SYMBOLS)

add_subdirectory(tests)

CMAKEFILES(
  CMakeLists.txt
  TODO
  icv_private.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
