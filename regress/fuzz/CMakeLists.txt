include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

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

function(check_libfuzzer_flags out_supported out_no_link_flag out_full_flag)
  set(${out_supported} OFF PARENT_SCOPE)
  set(${out_no_link_flag} "" PARENT_SCOPE)
  set(${out_full_flag} "" PARENT_SCOPE)

  set(_fuzzer_flag_prefixes "-fsanitize" "/fsanitize")

  foreach(_prefix IN LISTS _fuzzer_flag_prefixes)
    set(_no_link_flag "${_prefix}=fuzzer-no-link")
    set(_full_flag "${_prefix}=fuzzer")

    string(MAKE_C_IDENTIFIER "HAVE_CXX_${_no_link_flag}" _have_no_link_var)
    string(MAKE_C_IDENTIFIER "HAVE_CXX_${_full_flag}" _have_full_var)

    cmake_push_check_state(RESET)
      set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
      set(CMAKE_REQUIRED_FLAGS "${_no_link_flag}")
      check_cxx_source_compiles("int fuzzer_flag_probe(void) { return 0; }" ${_have_no_link_var})
    cmake_pop_check_state()

    cmake_push_check_state(RESET)
      set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
      set(CMAKE_REQUIRED_FLAGS "${_no_link_flag}")
      set(CMAKE_REQUIRED_LINK_OPTIONS "${_full_flag}")
      check_cxx_source_compiles("${CHECK_FUZZER_CODE}" ${_have_full_var})
    cmake_pop_check_state()

    if(${_have_no_link_var} AND ${_have_full_var})
      set(${out_supported} ON PARENT_SCOPE)
      set(${out_no_link_flag} "${_no_link_flag}" PARENT_SCOPE)
      set(${out_full_flag} "${_full_flag}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
endfunction()

check_libfuzzer_flags(FUZZER_SUPPORTED FUZZER_NO_LINK_FLAG FUZZER_FULL_FLAG)

if(FUZZER_SUPPORTED)
  set(FUZZER_COMPILE_OPTIONS "${FUZZER_NO_LINK_FLAG}")
  set(FUZZER_EXE_COMPILE_OPTIONS "${FUZZER_NO_LINK_FLAG}")
  set(FUZZER_LINK_OPTIONS "${FUZZER_FULL_FLAG}")
  message(STATUS "libFuzzer driver is available: ${FUZZER_FULL_FLAG}")
else()
  message(STATUS "libFuzzer driver is not available; building fuzz targets with stub main().")
endif()

if(TARGET libged)
  set(FUZZ_GED_SRCS fuzz_ged.cpp)
  set(FUZZ_SHOOTRAY_SRCS fuzz_shootray.cpp)

  if(NOT FUZZER_SUPPORTED)
    list(APPEND FUZZ_GED_SRCS fuzz_stub.cpp)
    list(APPEND FUZZ_SHOOTRAY_SRCS fuzz_stub.cpp)
  endif()

  brlcad_addexec(fuzz_ged_test "${FUZZ_GED_SRCS}" "libbu;librt;libged" TEST)
  brlcad_addexec(fuzz_shootray_test "${FUZZ_SHOOTRAY_SRCS}" "libbu;librt;libged" TEST)

  if(FUZZER_SUPPORTED)
    foreach(tgt fuzz_ged_test fuzz_shootray_test)
      target_compile_options(${tgt} PRIVATE ${FUZZER_EXE_COMPILE_OPTIONS})
      target_link_options(${tgt} PRIVATE ${FUZZER_LINK_OPTIONS})
    endforeach()

    BRLCAD_ADD_TEST(NAME fuzz_ged_smoke COMMAND fuzz_ged_test -runs=1 -max_total_time=5)
    BRLCAD_ADD_TEST(NAME fuzz_shootray_smoke COMMAND fuzz_shootray_test -runs=1 -max_total_time=5)
  else()
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
