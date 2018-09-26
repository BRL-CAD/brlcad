set(doc_legal
  bsd.txt
  bdl.txt
  lgpl.txt
  )
ADD_DOC(doc_legal legal)

add_subdirectory(embedded)

CMAKEFILES(bsd.txt bdl.txt lgpl.txt)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
