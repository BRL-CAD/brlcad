brlcad_addexec(gsh "gsh.cpp" "libged;libdm;libbu;Threads::Threads")
target_include_directories(gsh BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../libtermio)
add_dependencies(gsh dm_plugins ged_plugins)

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

brlcad_add_test(
  NAME gsh_command_schema_json
  COMMAND ${CMAKE_COMMAND} -DGSH_EXECUTABLE=$<TARGET_FILE:gsh> -P ${CMAKE_CURRENT_SOURCE_DIR}/schema_test.cmake
)

if(UNIX)
  brlcad_addexec(gsh_completion_terminal_test completion_terminal_test.c "libbu" TEST)
  add_dependencies(gsh_completion_terminal_test gsh)
  brlcad_add_test(
    NAME gsh_completion_terminal_display
    COMMAND gsh_completion_terminal_test $<TARGET_FILE:gsh>
  )
  brlcad_add_test(
    NAME gsh_completion_mode_off
    COMMAND gsh_completion_terminal_test $<TARGET_FILE:gsh> off
  )
  brlcad_add_test(
    NAME gsh_completion_mode_prefix
    COMMAND gsh_completion_terminal_test $<TARGET_FILE:gsh> prefix
  )
  brlcad_add_test(
    NAME gsh_completion_dsr_input
    COMMAND gsh_completion_terminal_test $<TARGET_FILE:gsh> nodsr
  )
  set_tests_properties(
    gsh_completion_terminal_display
    gsh_completion_mode_off
    gsh_completion_mode_prefix
    gsh_completion_dsr_input
    PROPERTIES TIMEOUT 20
  )
endif()

cmakefiles(
  CMakeLists.txt
  terminal_test.cpp
  terminal_test.py
  completion_terminal_test.c
  schema_test.cmake
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
