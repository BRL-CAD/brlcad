# Define the commands to expand the third party components we will need to handle
# DocBook processing from their archives into the build directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docbook-schema)
distclean(${CMAKE_CURRENT_BINARY_DIR}/docbook-schema)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fonts/dejavu-lgc)
distclean(${CMAKE_CURRENT_BINARY_DIR}/fonts/dejavu-lgc)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fonts/stix)
distclean(${CMAKE_CURRENT_BINARY_DIR}/fonts/stix)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/offo)
distclean(${CMAKE_CURRENT_BINARY_DIR}/offo/source)
distclean(${CMAKE_CURRENT_BINARY_DIR}/offo/binary)
distclean(${CMAKE_CURRENT_BINARY_DIR}/offo/README)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/standard/svg)
distclean(${CMAKE_CURRENT_BINARY_DIR}/standard/svg)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/standard/xsl)
distclean(${CMAKE_CURRENT_BINARY_DIR}/standard/xsl)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/docbook-schema/schema.sentinel
  COMMAND ${CMAKE_COMMAND} -E tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/docbook-5.0.tar.bz2
  COMMAND
    ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/docbookxi.nvdl
    ${CMAKE_CURRENT_BINARY_DIR}/docbook-schema/docbookxi.nvdl
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/docbook-schema/schema.sentinel
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/docbookxi.nvdl ${CMAKE_CURRENT_SOURCE_DIR}/docbook-5.0.tar.bz2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(schema-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/docbook-schema/schema.sentinel)
settargetfolder(schema-expand "DocBook")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fonts/dejavu-lgc/dejavu-lgc.sentinel
  COMMAND ${CMAKE_COMMAND} -E tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/fonts/dejavu-lgc-fonts.tar.bz2
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/fonts/dejavu-lgc/dejavu-lgc.sentinel
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/dejavu-lgc-fonts.tar.bz2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fonts
)
add_custom_target(fonts-dejavu-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fonts/dejavu-lgc/dejavu-lgc.sentinel)
settargetfolder(fonts-dejavu-expand "DocBook")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fonts/stix/stix.sentinel
  COMMAND ${CMAKE_COMMAND} -E tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/fonts/stix-fonts.tar.bz2
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/fonts/stix/stix.sentinel
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/stix-fonts.tar.bz2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fonts
)
add_custom_target(fonts-stix-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fonts/stix/stix.sentinel)
settargetfolder(fonts-stix-expand "DocBook")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/offo/offo2_0.sentinel
  COMMAND ${CMAKE_COMMAND} -E tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/offo/offo-2.0.tar.bz2
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/offo/offo2_0.sentinel
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/offo/offo-2.0.tar.bz2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/
)
add_custom_target(offo-2-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/offo/offo2_0.sentinel)
settargetfolder(offo-2-expand "DocBook")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/standard/svg/svg.sentinel
  COMMAND ${CMAKE_COMMAND} -E tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/standard/w3_svg_dtd.tar.bz2
  COMMAND
    ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/standard/met-fonts.xsl
    ${CMAKE_CURRENT_BINARY_DIR}/standard/svg/met-fonts.xsl
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/standard/svg/svg.sentinel
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/standard/met-fonts.xsl ${CMAKE_CURRENT_SOURCE_DIR}/standard/w3_svg_dtd.tar.bz2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/standard
)
add_custom_target(svg-dtd-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/standard/svg/svg.sentinel)
settargetfolder(svg-dtd-expand "DocBook")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/standard/xsl/xsl.sentinel
  COMMAND ${CMAKE_COMMAND} -E tar xjf ${CMAKE_CURRENT_SOURCE_DIR}/standard/docbook-xsl-ns.tar.bz2
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/standard/xsl/xsl.sentinel
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/standard/docbook-xsl-ns.tar.bz2
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/standard
)
add_custom_target(xsl-expand DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/standard/xsl/xsl.sentinel)
settargetfolder(xsl-expand "DocBook")

# Build up the set of targets defining resources, and write it to the cache
# immediately so targets in other directories can see it
set(
  DOCBOOK_RESOURCE_FILES
  schema-expand
  fonts-dejavu-expand
  fonts-stix-expand
  offo-2-expand
  svg-dtd-expand
  xsl-expand
)
set(DOCBOOK_RESOURCE_FILES "${DOCBOOK_RESOURCE_FILES}" CACHE STRING "DocBook target list")
mark_as_advanced(DOCBOOK_RESOURCE_FILES)

set(
  ignore_files
  CMakeLists.txt
  README.schema
  docbook-5.0.tar.bz2
  docbook5xi.rnc
  docbookxi.nvdl
  fonts/README
  fonts/dejavu-lgc-fonts.tar.bz2
  fonts/dejavu_COPYING.txt
  fonts/stix-fonts.tar.bz2
  fonts/stix_COPYING.txt
  fonts/y14.5.tar.bz2
  fonts/y14.5_COPYING.txt
  offo/README.offo
  offo/licenses.txt
  offo/offo-2.0.tar.bz2
  standard/README.svg
  standard/README.xsl
  standard/docbook-xsl-ns.tar.bz2
  standard/met-fonts.xsl
  standard/w3_svg_dtd.tar.bz2
)
cmakefiles(${ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
