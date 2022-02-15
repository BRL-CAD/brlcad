include_directories(
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

# First check whether fuzzing support is available.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-fsanitize=fuzzer HAVE_FUZZER)

if (HAVE_FUZZER)

  add_executable(fuzz_test fuzz_ged.cpp)
  target_link_libraries(fuzz_test PUBLIC libbu librt libged)
  target_compile_options(fuzz_test PRIVATE -fsanitize=fuzzer)
  target_link_options(fuzz_test PRIVATE -fsanitize=fuzzer)
  set_target_properties(fuzz_test PROPERTIES FOLDER "BRL-CAD Regression Tests")

  configure_file(run.sh "${CMAKE_CURRENT_BINARY_DIR}/fuzz" COPYONLY)
  install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/fuzz" DESTINATION ${BIN_DIR})
  configure_file(run.sh ${CMAKE_BINARY_DIR}/bin/fuzz COPYONLY)

  #BRLCAD_ADD_TEST(NAME fuzz COMMAND ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/fuzz run)

  add_custom_target(fuzz
    COMMAND ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/fuzz run
    )
  set_target_properties(fuzz PROPERTIES FOLDER "BRL-CAD Regression Tests")

endif (HAVE_FUZZER)

CMAKEFILES(
  CMakeLists.txt
  fuzz_ged.cpp
  fuzz_test1.cxx
  run.sh
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
