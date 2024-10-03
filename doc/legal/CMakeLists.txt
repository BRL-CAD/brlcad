set(doc_legal bsd.txt bdl.txt lgpl.txt)
brlcad_manage_files(doc_legal ${DOC_DIR}/legal)

add_subdirectory(embedded)

cmakefiles(bsd.txt bdl.txt lgpl.txt)
cmakefiles(README.txt)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
