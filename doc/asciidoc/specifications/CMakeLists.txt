set(
  specifications_EN
  BRL_CAD_g_format_V6.adoc
)

if(TARGET librt)
  if(BRLCAD_EXTRADOCS)
    add_asciidoc("HTML" specifications_EN specifications "")
  endif(BRLCAD_EXTRADOCS)
endif(TARGET librt)

cmakefiles(
  ${specifications_EN}
  BRL_CAD_g_format_V5.adoc
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
