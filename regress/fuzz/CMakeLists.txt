include_directories(
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )

# First check whether fuzzing support is available.
include(CheckCXXCompilerFlag)
set(CMAKE_REQUIRED_FLAGS -fsanitize=fuzzer)
check_cxx_compiler_flag(-fsanitize=fuzzer HAVE_FUZZER)

set(FUZZ_GED_SRCS fuzz_ged.cpp)

if (HAVE_FUZZER)

  target_compile_options(fuzz_ged_test PRIVATE -fsanitize=fuzzer)
  target_link_options(fuzz_ged_test PRIVATE -fsanitize=fuzzer)
  set_target_properties(fuzz_ged_test PROPERTIES FOLDER "BRL-CAD Regression Tests/fuzz")

else (HAVE_FUZZER)

  set(FUZZ_GED_SRCS ${FUZZ_GED_SRCS} fuzz_stub.cpp)

endif (HAVE_FUZZER)


add_executable(fuzz_ged_test ${FUZZ_GED_SRCS})
target_link_libraries(fuzz_ged_test PUBLIC libbu librt libged)


#BRLCAD_ADD_TEST(NAME fuzz COMMAND ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/fuzz run)

#add_custom_target(fuzz
#  COMMAND ${SH_EXEC} ${CMAKE_BINARY_DIR}/bin/fuzz run
#  )
#set_target_properties(fuzz PROPERTIES FOLDER "BRL-CAD Regression Tests/fuzz")


CMAKEFILES(
  CMakeLists.txt
  fuzz_ged.cpp
  fuzz_stub.cpp
  fuzz_test1.cxx
  run.sh
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
