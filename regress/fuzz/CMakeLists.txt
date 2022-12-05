set(FUZZ_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(FUZZ_INCLUDE_DIRS)

include(CheckCXXCompilerFlag)

# We're fuzzing, so let the compiler know it doesn't matter if we're doing
# anything with the results of computations
check_cxx_compiler_flag(-Wno-unused-but-set-variable HAVE_NO_UNUSED_BUT_SET_VARIABLE)

# We may need the fuzzer option to be a library
set(FUZZER_LIBRARY)

# Check whether fuzzing support is available.
set(CMAKE_REQUIRED_FLAGS -fsanitize=fuzzer)
check_cxx_compiler_flag(-fsanitize=fuzzer HAVE_FUZZER)
if (NOT HAVE_FUZZER)
  set(LLVM_FUZZER_SRC "
#include <stdint.h>
#include <stddef.h>
extern \\\"C\\\" int LLVMFuzzerTestOneInput(const uint8_t *, size_t) {
   __builtin_trap();
   return 0;
}
")
  cmake_push_check_state()
  set(CMAKE_REQUIRED_LIBRARIES "-fsanitize=fuzzer")
  check_cxx_source_compiles("${LLVM_FUZZER_SRC}" HAVE_LLVMFUZZER)
  cmake_pop_check_state()
  if (HAVE_LLVMFUZZER)
    set(FUZZER_LIBRARY "-fsanitize=fuzzer")
  endif (HAVE_LLVMFUZZER)
endif (NOT HAVE_FUZZER)

set(FUZZ_GED_SRCS fuzz_ged.cpp)
set(FUZZ_SHOOTRAY_SRCS fuzz_shootray.cpp)

if (NOT HAVE_FUZZER AND NOT HAVE_LLVMFUZZER)
  # provide simple main() so they at least compile and run
  set(FUZZ_GED_SRCS ${FUZZ_GED_SRCS} fuzz_stub.cpp)
  set(FUZZ_SHOOTRAY_SRCS ${FUZZ_SHOOTRAY_SRCS} fuzz_stub.cpp)
endif (NOT HAVE_FUZZER AND NOT HAVE_LLVMFUZZER)

BRLCAD_ADDEXEC(fuzz_ged_test "${FUZZ_GED_SRCS}" "libbu;librt;libged;${FUZZER_LIBRARY}" TEST)
BRLCAD_ADDEXEC(fuzz_shootray_test "${FUZZ_SHOOTRAY_SRCS}" "libbu;librt;libged;${FUZZER_LIBRARY}" TEST)

# 20220909CY If I'm not mistaken, these don't really make sense as CTest
# entries - usually fuzz testing is intended to run for a long time...
#BRLCAD_ADD_TEST(NAME fuzz_ged COMMAND fuzz_ged_test)
#BRLCAD_ADD_TEST(NAME fuzz_shootray COMMAND fuzz_shootray_test)

if (HAVE_FUZZER OR HAVE_LLVMFUZZER)
  target_compile_options(fuzz_ged_test PRIVATE -fsanitize=fuzzer)
  target_compile_options(fuzz_shootray_test PRIVATE -fsanitize=fuzzer)
endif (HAVE_FUZZER OR HAVE_LLVMFUZZER)

if (HAVE_FUZZER)
  target_link_options(fuzz_ged_test PRIVATE -fsanitize=fuzzer)
  target_link_options(fuzz_shootray_test PRIVATE -fsanitize=fuzzer)
endif (HAVE_FUZZER)

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
