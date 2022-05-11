configure_file(${CMAKE_CURRENT_SOURCE_DIR}/attributes.xml.in
  ${CMAKE_CURRENT_BINARY_DIR}/attributes.xml COPYONLY)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/attributes.xml")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/attr_std_table.xml
  COMMAND gen-attributes-file -o ${CMAKE_CURRENT_BINARY_DIR}/attr_std_table.xml -x std_attr_tbl
  DEPENDS gen-attributes-file ${BRLCAD_SOURCE_DIR}/src/librt/db5_attr.c
  )
add_custom_target(attr_std_table_xml DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/attr_std_table.xml)
set_target_properties(attr_std_table_xml PROPERTIES FOLDER "DocBook")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/attr_std_list.xml
  COMMAND gen-attributes-file -o ${CMAKE_CURRENT_BINARY_DIR}/attr_std_list.xml -l -x std_attr_info
  DEPENDS gen-attributes-file ${BRLCAD_SOURCE_DIR}/src/librt/db5_attr.c
  )
add_custom_target(attr_std_list_xml DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/attr_std_list.xml)
set_target_properties(attr_std_list_xml PROPERTIES FOLDER "DocBook")

set(man5_EN
  dsp.xml
  ${CMAKE_CURRENT_BINARY_DIR}/attributes.xml
  benchmark.xml
  burst_point_library.xml
  burst_shotline_files.xml
  bw.xml
  cmap.xml
  idents.xml
  nirt_formatting.xml
  pix.xml
  plot3.xml
  STEP.xml
  )
set(attribute_DEPS attr_std_list_xml attr_std_table_xml)

ADD_DOCBOOK("HTML;PHP;MAN5;PDF" man5_EN man5 "${attribute_DEPS}")
CMAKEFILES(
  CMakeLists.txt
  attributes.xml.in
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
