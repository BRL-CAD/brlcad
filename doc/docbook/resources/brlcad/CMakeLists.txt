file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/)

# Rework CMake binary and source paths for DocBook templates - if we
# have spaces in the paths, they will cause a problem.
string(REPLACE " " "%20" DOCBOOK_BINARY_DIR "${CMAKE_BINARY_DIR}")
string(REPLACE " " "%20" DOCBOOK_SOURCE_DIR "${CMAKE_SOURCE_DIR}")

# Because the core resources for Docbook are not different between the various
# build configurations, we maintain one copy of the "set up" files for all
# configurations rather than building the entire resources setup per-config.

# Common xsl for all outputs
configure_file(brlcad-common.xsl.in ${CMAKE_CURRENT_BINARY_DIR}/brlcad-common.xsl)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/brlcad-common.xsl")

# Fonts definition
configure_file(brlcad-fonts.xsl.in ${CMAKE_CURRENT_BINARY_DIR}/brlcad-fonts.xsl)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/brlcad-fonts.xsl")

# Wordpress
configure_file(wordpress.xsl.in ${CMAKE_CURRENT_BINARY_DIR}/wordpress.xsl)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/wordpress.xsl")

# Make sure distcheck knows about all files in directory
set(ignore_files
  CMakeLists.txt
  brlcad-article-fo-stylesheet.xsl.in
  brlcad-article-xhtml-stylesheet.xsl.in
  brlcad-book-fo-stylesheet.xsl.in
  brlcad-book-xhtml-stylesheet.xsl.in
  brlcad-common.xsl.in
  brlcad-fo-stylesheet.xsl.in
  brlcad-fonts.xsl.in
  brlcad-gendata.xsl
  brlcad-lesson-fo-stylesheet.xsl.in
  brlcad-lesson-xhtml-stylesheet.xsl.in
  brlcad-man-fo-stylesheet.xsl.in
  brlcad-man-stylesheet.xsl.in
  brlcad-man-xhtml-stylesheet.xsl.in
  brlcad-presentation-fo-stylesheet.xsl.in
  brlcad-presentation-xhtml-stylesheet.xsl.in
  brlcad-specification-fo-stylesheet.xsl.in
  brlcad-specification-xhtml-stylesheet.xsl.in
  brlcad-xhtml-header-navigation.xsl
  brlcad-xhtml-stylesheet.xsl.in
  center-table-print.xsl
  images/brlcad-logo-669966.svg
  images/brlcad-logo-6699cc.svg
  images/brlcad-logo-blue.svg
  images/brlcad-logo-cc6666.svg
  images/brlcad-logo-cc9966.svg
  images/brlcad-logo-green.svg
  images/brlcad-logo-limegreen.svg
  images/brlcad-logo-red.svg
  images/logo-vm-gears.png
  images/logo-vm-gears.svg
  presentation.xsl.in
  tutorial-cover-template.xsl.in
  tutorial-template.xsl.in
  wordpress.xsl.in
  )

CMAKEFILES(${ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

