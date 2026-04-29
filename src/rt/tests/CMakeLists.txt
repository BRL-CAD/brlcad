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

# Housekeeping
cmakefiles(
  CMakeLists.txt
  test_rt_opt.c
  run_rt_opt_lcov.sh
  run_rt_opt_short_compare.sh
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
