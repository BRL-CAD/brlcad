set(articles_HY
  about_hy.xml
  )

if (TARGET mged OR TARGET qged OR TARGET archer)
  ADD_DOCBOOK("HTML;PHP;PDF" articles_HY articles/hy articles_EN_IMAGES_cp)
endif (TARGET mged OR TARGET qged OR TARGET archer)

CMAKEFILES(
  CMakeLists.txt
  ${articles_HY}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
