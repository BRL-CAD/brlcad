# a new html and xml generation file
if(TARGET librt)
  brlcad_addexec(gen-attributes-file gen-attributes-file.cpp "libbu;librt" NO_INSTALL NO_STRICT)

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/attributes.xml.in ${CMAKE_CURRENT_BINARY_DIR}/attributes.xml COPYONLY)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/attributes.xml")

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/attr_std_table.xml
    COMMAND gen-attributes-file -o ${CMAKE_CURRENT_BINARY_DIR}/attr_std_table.xml -x std_attr_tbl
    DEPENDS gen-attributes-file ${BRLCAD_SOURCE_DIR}/src/librt/db5_attr.c
  )
  add_custom_target(attr_std_table_xml DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/attr_std_table.xml)
  set_target_properties(attr_std_table_xml PROPERTIES FOLDER "DocBook")

  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/attr_std_list.xml
    COMMAND gen-attributes-file -o ${CMAKE_CURRENT_BINARY_DIR}/attr_std_list.xml -l -x std_attr_info
    DEPENDS gen-attributes-file ${BRLCAD_SOURCE_DIR}/src/librt/db5_attr.c
  )
  add_custom_target(attr_std_list_xml DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/attr_std_list.xml)
  set_target_properties(attr_std_list_xml PROPERTIES FOLDER "DocBook")

  set(attribute_DEPS attr_std_list_xml attr_std_table_xml)
  if(BRLCAD_EXTRADOCS)
    add_docbook("HTML;PHP;MAN5;PDF" ${CMAKE_CURRENT_BINARY_DIR}/attributes.xml man5 "${attribute_DEPS}" REQUIRED "librt")
  endif(BRLCAD_EXTRADOCS)
endif(TARGET librt)

if(BRLCAD_EXTRADOCS)
  add_docbook("HTML;PHP;MAN5;PDF" dsp.xml             man5 "" REQUIRED "librt")
  add_docbook("HTML;PHP;MAN5;PDF" benchmark.xml       man5 "" REQUIRED "benchmark")
  add_docbook("HTML;PHP;MAN5;PDF" bw.xml              man5 "" REQUIRED "libicv")
  add_docbook("HTML;PHP;MAN5;PDF" cmap.xml            man5 "" REQUIRED "libdm")
  add_docbook("HTML;PHP;MAN5;PDF" idents.xml          man5 "" REQUIRED "libged")
  add_docbook("HTML;PHP;MAN5;PDF" nirt_formatting.xml man5 "" REQUIRED "libanalyze")
  add_docbook("HTML;PHP;MAN5;PDF" pix.xml             man5 "" REQUIRED "libicv")
  add_docbook("HTML;PHP;MAN5;PDF" plot3.xml           man5 "" REQUIRED "libbg")
  add_docbook("HTML;PHP;MAN5;PDF" STEP.xml            man5 "" REQUIRED "step-g")
endif(BRLCAD_EXTRADOCS)

cmakefiles(
  CMakeLists.txt
  STEP.xml
  attributes.xml.in
  benchmark.xml
  bw.xml
  cmap.xml
  dsp.xml
  idents.xml
  nirt_formatting.xml
  pix.xml
  plot3.xml
  gen-attributes-file.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
