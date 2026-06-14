set(tree_srcs
  tree.cpp
  )

brlcad_addexec(tree "${tree_srcs}" "libwdb;${M_LIBRARY}" FOLDER Shapes)

cmakefiles(
  CMakeLists.txt
  tree.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
