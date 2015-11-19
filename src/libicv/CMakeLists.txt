set(ICV_INCLUDE_DIRS
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIR}
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
  ppm.c
  bw.c
  dpix.c
  )

BRLCAD_ADDLIB(libicv "${LIBICV_SOURCES}" "libbu;libbn;${PNG_LIBRARY}")
set_target_properties(libicv PROPERTIES VERSION 20.0.1 SOVERSION 20)
add_subdirectory(tests)

CMAKEFILES(TODO)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
