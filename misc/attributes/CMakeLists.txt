# locally used but not needed by users of the library
include_directories(
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_SOURCE_DIR}/src/librt
  ${OPENNURBS_INCLUDE_DIR}
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )

# a new html and xml generation file
BRLCAD_ADDEXEC(gen-attributes-file gen-attributes-file.cpp "libbu;librt" NO_INSTALL NO_STRICT)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
