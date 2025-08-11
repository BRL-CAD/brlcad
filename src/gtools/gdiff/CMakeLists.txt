set(
  gdiff_srcs
  diff3_summary.c
  diff_summary.c
  gdiff.c
  merge.c
  utility.c
)

brlcad_addexec(gdiff "${gdiff_srcs}" "librt")
target_include_directories(gdiff BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

cmakefiles(
  CMakeLists.txt
  gdiff.h
  tests/00_moss-control.asc
  tests/01_moss-diff.asc
  tests/02_moss-3diff.asc
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
