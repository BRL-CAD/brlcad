brlcad_addexec(gsh "gsh.cpp" "libged;libdm;libbu;Threads::Threads")
target_include_directories(gsh BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../libtermio)
add_dependencies(gsh dm_plugins ged_plugins)

brlcad_add_test(NAME gsh_new_commands_db
  COMMAND gsh --new-cmds "${PROJECT_SOURCE_DIR}/src/libged/tests/draw/moss.g" draw all.g)

if(BUILD_TESTING AND UNIX)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    brlcad_addexec(gsh_terminal_test terminal_test.cpp "libbu;Threads::Threads" TEST)
    target_include_directories(gsh_terminal_test BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../libtermio)
    brlcad_add_test(NAME gsh_terminal_resize
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/terminal_test.py $<TARGET_FILE:gsh_terminal_test>)
    set_tests_properties(gsh_terminal_resize PROPERTIES TIMEOUT 30)
  endif()
endif()

cmakefiles(
  CMakeLists.txt
  terminal_test.cpp
  terminal_test.py
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
