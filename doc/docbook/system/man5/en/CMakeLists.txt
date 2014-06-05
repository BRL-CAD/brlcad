configure_file(${CMAKE_CURRENT_SOURCE_DIR}/attributes.xml.in
  ${CMAKE_CURRENT_BINARY_DIR}/attributes.xml COPYONLY)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/attr_std_table.xml
  COMMAND gen-attributes-file -o ${CMAKE_CURRENT_BINARY_DIR}/attr_std_table.xml -x std_attr_tbl
  DEPENDS gen-attributes-file ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/librt/db5_attr.c
  )
add_custom_target(attr_std_table_xml DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/attr_std_table.xml)
set_target_properties(attr_std_table_xml PROPERTIES FOLDER "DocBook")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/attr_std_list.xml
  COMMAND gen-attributes-file -o ${CMAKE_CURRENT_BINARY_DIR}/attr_std_list.xml -l -x std_attr_info
  DEPENDS gen-attributes-file ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/librt/db5_attr.c
  )
add_custom_target(attr_std_list_xml DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/attr_std_list.xml)
set_target_properties(attr_std_list_xml PROPERTIES FOLDER "DocBook")

set(docbook_man5_EN
  dsp.xml
  ${CMAKE_CURRENT_BINARY_DIR}/attributes.xml
  bw.xml
  cmap.xml
  pix.xml
  plot3.xml
  )


DOCBOOK_TO_HTML(man5 docbook_man5_EN html/man5/en "${attributes_DEPS}")
DOCBOOK_TO_MAN(man5 docbook_man5_EN 5 5 man5 "${attributes_DEPS}")

if(BRLCAD_EXTRADOCS_PDF_MAN)
  DOCBOOK_TO_PDF(man5 docbook_man5_EN pdf/man5/en "${attributes_DEPS}")
endif(BRLCAD_EXTRADOCS_PDF_MAN)

add_dependencies(attributes_man5_html attr_std_list_xml attr_std_table_xml)

if(BRLCAD_EXTRADOCS_MAN)
  add_dependencies(attributes_man5_man attr_std_list_xml attr_std_table_xml)
endif(BRLCAD_EXTRADOCS_MAN)

if(BRLCAD_EXTRADOCS_PDF_MAN)
  add_dependencies(attributes_man5_pdf attr_std_list_xml attr_std_table_xml)
endif(BRLCAD_EXTRADOCS_PDF_MAN)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
