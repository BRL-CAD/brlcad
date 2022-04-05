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

else (HAVE_FUZZER)

  set(FUZZ_GED_SRCS ${FUZZ_GED_SRCS} fuzz_stub.cpp)

endif (HAVE_FUZZER)


BRLCAD_ADDEXEC(fuzz_ged_test "${FUZZ_GED_SRCS}" "libbu;librt;libged" TEST)
BRLCAD_ADD_TEST(NAME fuzz_ged COMMAND fuzz_ged_test)


CMAKEFILES(
  CMakeLists.txt
  fuzz_ged.cpp
  fuzz_stub.cpp
  fuzz_shootray.cpp
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
