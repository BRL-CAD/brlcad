set(
  edit_cases
  annot
  arb8
  arb_operation_matrix
  arbn
  ars
  bot
  bot_operation_matrix
  brep
  bspline
  cline
  comb
  datum
  dsp
  ebm
  edit_desc
  edit_str_get_params
  ehy
  ell
  epa
  eto
  extrude
  fuzz
  half
  hrt
  hyp
  knob_operation_matrix
  lifecycle_matrix
  metaball
  metaball_operation_matrix
  nmg
  operation_matrix
  param_matrix
  part
  pipe
  raster_operation_matrix
  rec
  revolve
  rhc
  rpc
  sketch
  sph
  superell
  tgc
  tor
  vol
)

set(edit_sources main.cpp)
set(edit_test_declarations "")
set(edit_test_entries "")
foreach(edit_case IN LISTS edit_cases)
  list(APPEND edit_sources "${edit_case}.cpp")
  string(APPEND edit_test_declarations "int rt_edit_test_${edit_case}(void);\n")
  string(APPEND edit_test_entries "    {\"${edit_case}\", rt_edit_test_${edit_case}},\n")
endforeach()

configure_file(cases.inc.in "${CMAKE_CURRENT_BINARY_DIR}/cases.inc" @ONLY)
distclean("${CMAKE_CURRENT_BINARY_DIR}/cases.inc")

brlcad_addexec(rt_edit_test "${edit_sources}" "librt;libnmg;libwdb;libbrep;${WINSOCK_LIB}" TEST)
# The cases have translation-unit-local fixture helpers with shared names.
set_target_properties(rt_edit_test PROPERTIES UNITY_BUILD OFF)
target_include_directories(rt_edit_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

foreach(edit_case IN LISTS edit_cases)
  brlcad_add_test(NAME rt_edit_test_${edit_case} COMMAND rt_edit_test ${edit_case})
endforeach()
brlcad_add_test(NAME rt_edit_test_inventory COMMAND rt_edit_test --check-inventory)

set(distcheck_files CMakeLists.txt cases.inc.in operations.inc test_utils.h ${edit_sources})
cmakefiles(${distcheck_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
