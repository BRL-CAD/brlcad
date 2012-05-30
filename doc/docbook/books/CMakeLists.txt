# Set some settings specific to books

set(XSL_FO_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/brlcad-fo-stylesheet.xsl")

add_subdirectory(en)
CMAKEFILES(README)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
