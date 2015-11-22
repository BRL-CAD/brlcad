set(docbook_specifications_EN
  BRL_CAD_g_format_V5.xml
  )
set(docbook_specifications_EN_V6
  BRL_CAD_g_format_V6.xml
  )

DOCBOOK_TO_HTML(specification docbook_specifications_EN html/specifications/en "")
DOCBOOK_TO_PHP(specification docbook_specifications_EN html/specifications/en "")
DOCBOOK_TO_PDF(specification docbook_specifications_EN pdf/specifications/en "")

DOCBOOK_TO_HTML(specification docbook_specifications_EN_V6 html/specifications/en "")
DOCBOOK_TO_PHP(specification docbook_specifications_EN_V6 html/specifications/en "")
DOCBOOK_TO_PDF(specification docbook_specifications_EN_V6 pdf/specifications/en "")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
