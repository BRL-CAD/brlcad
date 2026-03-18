include(${BRLCAD_CMAKE_DIR}/AsciiDoc.cmake)

verbose_add_subdirectory(asciidoc system)
verbose_add_subdirectory(asciidoc articles)
verbose_add_subdirectory(asciidoc books)
verbose_add_subdirectory(asciidoc devguides)
verbose_add_subdirectory(asciidoc lessons)
verbose_add_subdirectory(asciidoc presentations)
verbose_add_subdirectory(asciidoc specifications)

cmakefiles(
  CMakeLists.txt
  README.adoc
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
