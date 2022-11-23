add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD=1
  )

# locally used but not needed by users of the library
set(GENATTRS_DIRS
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_SOURCE_DIR}/src/librt
  ${OPENNURBS_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(GENATTRS_DIRS)

# a new html and xml generation file
BRLCAD_ADDEXEC(gen-attributes-file gen-attributes-file.cpp "libbu;librt" NO_INSTALL NO_STRICT)

CMAKEFILES(gen-attributes-files.cpp.in)
CMAKEFILES(CMakeLists.txt)
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
