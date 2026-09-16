if(SH_EXEC AND TARGET tree AND TARGET mged)
  brlcad_add_test(
    NAME regress-tree
    COMMAND ${SH_EXEC} "${CMAKE_CURRENT_SOURCE_DIR}/tree.sh" ${CMAKE_SOURCE_DIR} "$<TARGET_FILE:tree>" "$<TARGET_FILE:mged>"
    )
  brlcad_regression_test(regress-tree "tree;mged" TEST_DEFINED STAND_ALONE)
endif(SH_EXEC AND TARGET tree AND TARGET mged)

cmakefiles(
  CMakeLists.txt
  tree.sh
  broadleaf.json
  conifer.json
)

set(
  tree_outfiles
  tree.log
  tree-mesh.g
  tree-json.g
  tree-csg.g
  tree-append.g
  tree-demo.g
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${tree_outfiles}")
distclean(${tree_outfiles})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
