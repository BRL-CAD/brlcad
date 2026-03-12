brlcad_addexec(brledit brledit.cpp "" "")
target_include_directories(brledit BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/libtermio
)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-O3 O3_COMPILER_FLAG)
if(O3_COMPILER_FLAG)
  # If we have the O3 flag, use it
  target_compile_options(brledit PRIVATE "-O3")
endif(O3_COMPILER_FLAG)
include(CheckCCompilerFlag)

cmakefiles(
  CMakeLists.txt
  brledit.cpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
