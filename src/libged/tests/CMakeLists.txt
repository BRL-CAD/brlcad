brlcad_addexec(ged_test_tops test_tops.c libged TEST)
if(TARGET ged_test_tops)
  add_dependencies(ged_test_tops ged_plugins)
endif(TARGET ged_test_tops)
brlcad_add_test(NAME ged_test_tops_moss COMMAND ged_test_tops "${CMAKE_CURRENT_SOURCE_DIR}/ged_tests.g")

brlcad_addexec(ged_test_list test_list.c libged TEST)
if(TARGET ged_test_list)
  add_dependencies(ged_test_list ged_plugins)
endif(TARGET ged_test_list)
brlcad_add_test(NAME ged_test_list COMMAND ged_test_list 2)

brlcad_addexec(ged_test_gqa test_gqa.c libged TEST)
if(TARGET ged_test_gqa)
  add_dependencies(ged_test_gqa ged_plugins)
endif(TARGET ged_test_gqa)

brlcad_addexec(ged_test_plugins plugins.cpp libged TEST)
if(TARGET ged_test_plugins)
  add_dependencies(ged_test_plugins ged_plugins)
endif(TARGET ged_test_plugins)

brlcad_addexec(ged_test_material test_material.c libged TEST)
if(TARGET ged_test_material)
  add_dependencies(ged_test_material ged_plugins)
endif(TARGET ged_test_material)
brlcad_add_test(NAME ged_test_material COMMAND ged_test_material "${CMAKE_CURRENT_BINARY_DIR}/ged_test_material.g")
if(TARGET ged_test_material)
  set_property(TEST ged_test_material APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif(TARGET ged_test_material)
distclean(${CMAKE_CURRENT_BINARY_DIR}/ged_test_material.g)

brlcad_addexec(ged_test_select test_select.c libged TEST)
if(TARGET ged_test_select)
  add_dependencies(ged_test_select ged_plugins)
endif(TARGET ged_test_select)

brlcad_addexec(ged_test_search test_search.c libged TEST)
if(TARGET ged_test_search)
  add_dependencies(ged_test_search ged_plugins)
endif(TARGET ged_test_search)
brlcad_add_test(NAME ged_test_search COMMAND ged_test_search "${CMAKE_CURRENT_SOURCE_DIR}/search_tests.g")

# db_search testing
# TODO - tests the C api, but uses libged - either need to limit our dependencies
# to librt, or rename test.
brlcad_addexec(rt_search_test search_tests.cpp "librt;libbu;libged" TEST)
brlcad_add_test(NAME rt_search_tests COMMAND rt_search_test "${CMAKE_CURRENT_SOURCE_DIR}/rt_search_tests.g")

brlcad_addexec(ged_check_prim_cmds check_prim_cmds.cpp libged TEST)
if(TARGET ged_check_prim_cmds)
  add_dependencies(ged_check_prim_cmds  ged_plugins)
endif(TARGET ged_check_prim_cmds)
brlcad_add_test(NAME ged_test_prim_cmds_exist COMMAND ged_test_prim_cmds)

add_subdirectory(bot)
add_subdirectory(draw)

# Note: it is particularly important that the lint tests in particular are kept
# in a separate file, since its bad input examples stand an excellent chance of
# breaking commands.
cmakefiles(
  lint_tests.g
  rt_search_tests.g
  search_tests.g
  obb_tests.g
  ged_tests.g
  pnts_in_out.g
)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
