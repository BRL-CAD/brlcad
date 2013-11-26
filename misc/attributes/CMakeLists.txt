# locally used but not needed by users of the library
include_directories(
  ${BRLCAD_SOURCE_DIR}/include
  ${BRLCAD_SOURCE_DIR}/src/librt
  ${OPENNURBS_INCLUDE_DIR}
  ${BRLCAD_BOOST_INCLUDE_DIR}
  ${BU_INCLUDE_DIRS}
  )

# a new html and xml generation file
configure_file(${CMAKE_SOURCE_DIR}/misc/attributes/gen-attributes-files.cpp.in
  ${CMAKE_BINARY_DIR}/misc/attributes/gen-attributes-files.cpp @ONLY)
BRLCAD_ADDEXEC(gen-attributes-files
  ${CMAKE_BINARY_DIR}/misc/attributes/gen-attributes-files.cpp "libbu;librt" NO_INSTALL NO_STRICT)

set(gen_attr_xml_fils
  ${CMAKE_BINARY_DIR}/auto-gen-attr-std-tab-inc.xml
  ${CMAKE_BINARY_DIR}/auto-gen-attr-usr-tab-inc.xml
  ${CMAKE_BINARY_DIR}/auto-gen-attr-std-list-inc.xml
)
set(gen_attr_html_fils
  ${CMAKE_BINARY_DIR}/auto-gen-brlcad-attributes.html
)

set(ofils ${gen_attr_fils} ${gen_attr_html_fils})
set(attr_xml_dir "${CMAKE_BINARY_DIR}")
add_custom_command(
  TARGET gen-attributes-files
  POST_BUILD
  COMMAND gen-attributes-files
  COMMAND ${CMAKE_COMMAND} ARGS -E copy auto-gen-attr-std-tab-inc.xml ${attr_xml_dir}
  COMMAND ${CMAKE_COMMAND} ARGS -E copy auto-gen-attr-usr-tab-inc.xml ${attr_xml_dir}
  COMMAND ${CMAKE_COMMAND} ARGS -E copy auto-gen-attr-std-list-inc.xml ${attr_xml_dir}
)

# special install of


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
