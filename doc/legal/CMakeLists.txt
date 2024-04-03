set(doc_legal
  bsd.txt
  bdl.txt
  lgpl.txt
  )
BRLCAD_MANAGE_FILES(doc_legal ${DOC_DIR}/legal)

add_subdirectory(embedded)
if (EXISTS ${BRLCAD_SOURCE_DIR}/src/other)
  add_subdirectory(other)
endif (EXISTS ${BRLCAD_SOURCE_DIR}/src/other)

CMAKEFILES(bsd.txt bdl.txt lgpl.txt)
CMAKEFILES(README.txt)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
