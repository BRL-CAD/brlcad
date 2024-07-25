set(doc_legal
  bsd.txt
  bdl.txt
  lgpl.txt
  )
BRLCAD_MANAGE_FILES(doc_legal ${DOC_DIR}/legal)

add_subdirectory(embedded)

CMAKEFILES(bsd.txt bdl.txt lgpl.txt)
CMAKEFILES(README.txt)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
