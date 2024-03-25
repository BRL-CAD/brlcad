set(articles_RU
  about_ru.xml
  )
if (TARGET mged OR TARGET qged OR TARGET archer)
  if (BRLCAD_EXTRADOCS)
    ADD_DOCBOOK("HTML;PHP;PDF" articles_RU articles/ru "")
  endif (BRLCAD_EXTRADOCS)
endif (TARGET mged OR TARGET qged OR TARGET archer)

CMAKEFILES(
  ${articles_RU}
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
