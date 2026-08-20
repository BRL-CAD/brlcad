# src/rt/tests/CMakeLists.txt
#
# CTest registrations for the rt option-parsing regression tests.
#
# The test_rt_opt binary is defined in the parent CMakeLists.txt
# (src/rt/CMakeLists.txt) because brlcad_addexec's internal cmakefiles
# tracker requires all source files to be under the invoking directory.
#
# To generate an lcov coverage report, run:
#   bash src/rt/tests/run_rt_opt_lcov.sh
#

set(RT_TEST_DB "${CMAKE_SOURCE_DIR}/src/libged/tests/draw/moss.g")

set(
  RT_OPT_TESTS
  size
  width
  height
  output
  output_double
  framebuffer
  azimuth
  elevation
  perspective
  eye_backoff
  view_aspect
  matflag
  cell_width
  cell_height
  subgrid
  cut_plane
  ambient
  light_model
  haze
  incremental
  topdown
  stereo
  hypersample
  jitter
  units
  use_air
  tolerance
  opencl
  command
  density_file
  start_frame
  end_frame
  single_pixel
  query_pixel
  objects_file
  plus_text
  rand_table
  verbose
  rt_debug
  optical_debug
  nmg_debug
  bu_debug
  cpus
  benchmark
  report_overlaps
  no_overlap
  white_bg
  bgcolor
  help
  unknown_opt
  optind_boundary
  multiple_opts
)

foreach(tname ${RT_OPT_TESTS})
  brlcad_add_test(NAME rt_opt_${tname} COMMAND test_rt_opt ${tname})
endforeach(tname)
brlcad_add_test(NAME rt_opt_short_only COMMAND test_rt_opt --short-only)

brlcad_add_test(
  NAME rtsil_output_file
  COMMAND
    "${CMAKE_COMMAND}"
    "-DRTSIL=$<TARGET_FILE:rtsil>"
    "-DDB=${RT_TEST_DB}"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_rtsil_output.cmake"
)
# Since the test is a CMake script, we need to ensure the target
# dependencies ourselves
add_dependencies(unit rtsil)
add_dependencies(check rtsil)

brlcad_add_test(
  NAME rtxray_bw_output
  COMMAND
    "${CMAKE_COMMAND}"
    "-DRTXRAY=$<TARGET_FILE:rtxray>"
    "-DDB=${RT_TEST_DB}"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_rtxray_bw_output.cmake"
)

brlcad_add_test(
  NAME rt_disk_framebuffer_size
  COMMAND
    "${CMAKE_COMMAND}"
    "-DRT=$<TARGET_FILE:rt>"
    "-DDB=${RT_TEST_DB}"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_rt_disk_framebuffer_size.cmake"
)
# Since the test is a CMake script, we need to ensure the target
# dependencies ourselves
add_dependencies(unit rt)
add_dependencies(check rt)


brlcad_add_test(
  NAME rt_unbuffered_background
  COMMAND
    "${CMAKE_COMMAND}"
    "-DRT=$<TARGET_FILE:rt>"
    "-DDB=${RT_TEST_DB}"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_rt_unbuffered_background.cmake"
)

brlcad_add_test(
  NAME rt_command_view
  COMMAND
    "${CMAKE_COMMAND}"
    "-DRT=$<TARGET_FILE:rt>"
    "-DDB=${RT_TEST_DB}"
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/test_rt_command_view.cmake"
)

# Housekeeping
cmakefiles(
  CMakeLists.txt
  test_rt_command_view.cmake
  test_rt_disk_framebuffer_size.cmake
  test_rt_unbuffered_background.cmake
  test_rt_opt.c
  test_rtsil_output.cmake
  test_rtxray_bw_output.cmake
  run_rt_opt_lcov.sh
  run_rt_opt_short_compare.sh
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
