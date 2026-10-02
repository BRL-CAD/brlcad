brlcad_addexec(ged_test_pipe_route pipe_route.c "libged;libwdb;librt;libbu" TEST)
if(TARGET ged_test_pipe_route)
  add_dependencies(ged_test_pipe_route ged_plugins)
endif(TARGET ged_test_pipe_route)
brlcad_add_test(NAME ged_test_pipe_route COMMAND ged_test_pipe_route "${CMAKE_CURRENT_BINARY_DIR}/ged_test_pipe_route.g")
if(TARGET ged_test_pipe_route)
  set_property(TEST ged_test_pipe_route APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
endif(TARGET ged_test_pipe_route)
distclean(${CMAKE_CURRENT_BINARY_DIR}/ged_test_pipe_route.g)

cmakefiles(
  CMakeLists.txt
  pipe_route.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
