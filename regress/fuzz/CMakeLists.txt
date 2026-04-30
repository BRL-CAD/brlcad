include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
include(CheckLinkerFlag)

set(FUZZER_COMPILE_OPTIONS)
set(FUZZER_EXE_COMPILE_OPTIONS)
set(FUZZER_LINK_OPTIONS)
set(FUZZER_SUPPORTED OFF)

set(CHECK_FUZZER_CODE "
#include <stdint.h>
#include <stddef.h>
extern \"C\" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  (void)data;
  (void)size;
  return 0;
}
")

# 1. Check for compile-only fuzzer support
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer-no-link")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
check_cxx_source_compiles("int main() { return 0; }" HAVE_FSANITIZE_FUZZER_NO_LINK)
cmake_pop_check_state()
unset(CMAKE_TRY_COMPILE_TARGET_TYPE)

# 2. Check for full fuzzer linking support
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer")
check_cxx_source_compiles("${CHECK_FUZZER_CODE}" HAVE_FSANITIZE_FUZZER)
cmake_pop_check_state()

# 3. Apply variables if supported
if(HAVE_FSANITIZE_FUZZER AND HAVE_FSANITIZE_FUZZER_NO_LINK)
  set(FUZZER_SUPPORTED ON)
  
  # -no-link is kept for instrumenting the underlying libraries under test
  set(FUZZER_COMPILE_OPTIONS -fsanitize=fuzzer-no-link) 
  # Executables MUST be compiled with full fuzzer flags to inject main() on MSVC
  set(FUZZER_EXE_COMPILE_OPTIONS -fsanitize=fuzzer)

  # Check if linker actually accepts the flag without warnings
  check_linker_flag(CXX "-fsanitize=fuzzer" HAVE_LINKER_FSANITIZE_FUZZER)
  if(HAVE_LINKER_FSANITIZE_FUZZER)
    set(FUZZER_LINK_OPTIONS -fsanitize=fuzzer)
  endif()
endif()

if(FUZZER_SUPPORTED)
  message(STATUS "libFuzzer driver is available.")
else()
  message(STATUS "libFuzzer driver is not available; building fuzz targets with stub main().")
endif()

# 4. Target setup and generation
if(TARGET libged)
  set(FUZZ_GED_SRCS fuzz_ged.cpp)
  set(FUZZ_SHOOTRAY_SRCS fuzz_shootray.cpp)

  # Append stub if libFuzzer is missing
  if(NOT FUZZER_SUPPORTED)
    list(APPEND FUZZ_GED_SRCS fuzz_stub.cpp)
    list(APPEND FUZZ_SHOOTRAY_SRCS fuzz_stub.cpp)
  endif()

  brlcad_addexec(fuzz_ged_test "${FUZZ_GED_SRCS}" "libbu;librt;libged" TEST)
  brlcad_addexec(fuzz_shootray_test "${FUZZ_SHOOTRAY_SRCS}" "libbu;librt;libged" TEST)

  if(FUZZER_SUPPORTED)
    foreach(tgt fuzz_ged_test fuzz_shootray_test)
      target_compile_options(${tgt} PRIVATE ${FUZZER_EXE_COMPILE_OPTIONS})

      # make sure safe to add linker flag
      if(FUZZER_LINK_OPTIONS)
	target_link_options(${tgt} PRIVATE ${FUZZER_LINK_OPTIONS})
      endif()
    endforeach()

    # Add limited-run smoke tests
    BRLCAD_ADD_TEST(NAME fuzz_ged_smoke COMMAND fuzz_ged_test -runs=1 -max_total_time=5)
    BRLCAD_ADD_TEST(NAME fuzz_shootray_smoke COMMAND fuzz_shootray_test -runs=1 -max_total_time=5)
  else()
    # Add standard tests relying on the stub main()
    BRLCAD_ADD_TEST(NAME fuzz_ged COMMAND fuzz_ged_test)
    BRLCAD_ADD_TEST(NAME fuzz_shootray COMMAND fuzz_shootray_test)
  endif()

endif(TARGET libged)

cmakefiles(
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
