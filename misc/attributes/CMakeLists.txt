add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD=1
  )

# a new html and xml generation file
if (TARGET librt)
  BRLCAD_ADDEXEC(gen-attributes-file gen-attributes-file.cpp "libbu;librt" NO_INSTALL NO_STRICT)
  target_include_directories(gen-attributes-file BEFORE PRIVATE ${BRLCAD_SOURCE_DIR}/src/librt)
endif (TARGET librt)

CMAKEFILES(gen-attributes-files.cpp.in)
CMAKEFILES(CMakeLists.txt)
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
