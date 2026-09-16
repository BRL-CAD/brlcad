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

  # Some compilers (notably MSVC's cl.exe and link.exe) only *warn* about an
  # unrecognized flag and still exit successfully, so a plain exit-code check
  # would report a false positive and then leak the unusable flag onto the real
  # targets.  Treat any "flag not understood" diagnostic as a failure so the
  # probe reflects genuine support on every toolchain.  This mirrors the
  # patterns CMake's own CheckCXXCompilerFlag module uses internally.
  set(_fuzzer_fail_regex
    "[Uu]nknown argument"               # clang
    "[Uu]nrecogni[sz]ed .*option"       # gcc / binutils ld
    "ignoring unknown option"           # MSVC cl D9002
    "command line warning D9002"        # MSVC cl (explicit)
    "LNK4044"                           # MSVC link (unrecognized option)
  )

  foreach(_prefix IN LISTS _fuzzer_flag_prefixes)
    set(_no_link_flag "${_prefix}=fuzzer-no-link")
    set(_full_flag "${_prefix}=fuzzer")

    # MAKE_C_IDENTIFIER maps both leading '-' and '/' to '_', so the two flag
    # spellings would otherwise collide on a single cache entry and the second
    # prefix would silently reuse the first's result.  Tag the variable name
    # with the prefix style to keep the probes independent.
    string(SUBSTRING "${_prefix}" 0 1 _prefix_char)
    if(_prefix_char STREQUAL "/")
      set(_prefix_tag "slash")
    else()
      set(_prefix_tag "dash")
    endif()

    string(MAKE_C_IDENTIFIER "HAVE_CXX_${_prefix_tag}_${_no_link_flag}" _have_no_link_var)
    string(MAKE_C_IDENTIFIER "HAVE_CXX_${_prefix_tag}_${_full_flag}" _have_full_var)

    cmake_push_check_state(RESET)
      set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
      set(CMAKE_REQUIRED_FLAGS "${_no_link_flag}")
      check_cxx_source_compiles("int fuzzer_flag_probe(void) { return 0; }" ${_have_no_link_var}
	FAIL_REGEX ${_fuzzer_fail_regex})
    cmake_pop_check_state()

    cmake_push_check_state(RESET)
      set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
      set(CMAKE_REQUIRED_FLAGS "${_no_link_flag}")
      set(CMAKE_REQUIRED_LINK_OPTIONS "${_full_flag}")
      check_cxx_source_compiles("${CHECK_FUZZER_CODE}" ${_have_full_var}
	FAIL_REGEX ${_fuzzer_fail_regex})
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
