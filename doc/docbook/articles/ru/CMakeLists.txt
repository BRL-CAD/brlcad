set(articles_RU
  about_ru.xml
  )
ADD_DOCBOOK("HTML;PHP;PDF" articles_RU articles/ru "")

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
