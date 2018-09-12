set(specifications_EN
  BRL_CAD_g_format_V5.xml
  )
ADD_DOCBOOK("HTML;PHP;PDF" specifications_EN specifications/en "")

CMAKEFILES(BRL_CAD_g_format_V6.xml)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
