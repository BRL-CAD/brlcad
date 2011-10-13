# Set some settings specific to books

SET(XSL_FO_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/brlcad-fo-stylesheet.xsl")

ADD_SUBDIRECTORY(en)
CMAKEFILES(README)
