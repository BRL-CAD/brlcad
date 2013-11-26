# ensure no stale copy of auto-generated file 'attributes.xml' exists
set(attr_xml_fil ${CMAKE_CURRENT_SOURCE_DIR}/attributes.xml)
if(EXISTS ${attr_xml_fil})
  file(REMOVE ${attr_xml_fil})
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/attributes.xml.in
  ${attr_xml_fil} @ONLY)

set(docbook_man5_EN
  dsp.xml
  attributes.xml
  bw.xml
  cmap.xml
  pix.xml
  plot3.xml
  )

# special dependencies (gen-attributes-files.cpp is in misc/attributes)
set(attributes_DEPS ${gen_attr_xml_fils} gen-attributes-files)

DOCBOOK_TO_HTML(man5 docbook_man5_EN html/man5/en "${attributes_DEPS}")
DOCBOOK_TO_MAN(man5 docbook_man5_EN 5 5 man5 "${attributes_DEPS}")

if(BRLCAD_EXTRADOCS_PDF_MAN)
  DOCBOOK_TO_PDF(man5 docbook_man5_EN pdf/man5/en "${attributes_DEPS}")
endif(BRLCAD_EXTRADOCS_PDF_MAN)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
