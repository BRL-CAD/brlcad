set(RT_TEST_LIBS "librt;${M_LIBRARY}")

if(TARGET librt-static)
  brlcad_addexec(rt_static_init static_init.cpp "librt-static;${M_LIBRARY}" TEST)
  brlcad_add_test(NAME rt_static_init COMMAND rt_static_init)
  if(BUILD_TESTING AND TARGET rt_static_init)
    set_tests_properties(rt_static_init PROPERTIES TIMEOUT 10)
  endif()
endif()

brlcad_addexec(bundle bundle.c "${RT_TEST_LIBS}" TEST)
brlcad_add_test(NAME rt_bundle COMMAND bundle)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")
distclean("${CMAKE_CURRENT_BINARY_DIR}/bundle.plot3")

brlcad_addexec(rt_pattern rt_pattern.c "${RT_TEST_LIBS}" TEST)
brlcad_add_test(NAME rt_pattern_5 COMMAND rt_pattern 5)
brlcad_add_test(NAME rt_pattern_6 COMMAND rt_pattern 6)
set_property(
  DIRECTORY
  APPEND
  PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3"
           "${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_6.plot3"
)
distclean("${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_5.plot3")
distclean("${CMAKE_CURRENT_BINARY_DIR}/rt_pattern_test_6.plot3")

brlcad_addexec(rt_dvec dvec.cpp "${RT_TEST_LIBS}" NO_STRICT TEST)
brlcad_add_test(NAME rt_dvec COMMAND rt_dvec)

brlcad_addexec(rt_ssi ssi.cpp "${RT_TEST_LIBS}" TEST)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
# BRLCAD_ADD_TEST(NAME rt_ssi COMMAND rt_ssi)

# NURBS testing
brlcad_addexec(rt_nurbs_tester nurbs_tests.cpp "librt;libbrep;libbu;${M_LIBRARY}" TEST)

# disabled prior to 7.22.2 release due to unresolved failures in the implementation
#BRLCAD_ADD_TEST(NAME NURBS-get_closest_point-distinct_points nurbs_tester ${CMAKE_CURRENT_SOURCE_DIR}/nurbs_surfaces.g 1)

# materialX testing
#set(MATERIALX_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXCore.lib;${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXFormat.lib;${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXGenOsl.lib;${CMAKE_CURRENT_SOURCE_DIR}/../../../MaterialXSource/lib/MaterialXGenShader.lib)
set(USING_MATERIALX NO)
if(USING_MATERIALX)
  brlcad_addexec(materialx_test materialx_test.cpp "${RT_TEST_LIBS}" TEST)
  brlcad_add_test(NAME rt_mtlx_test COMMAND materialx_test)
else(USING_MATERIALX)
  cmakefiles(materialx_test.cpp)
endif(USING_MATERIALX)

# diff testing
brlcad_addexec(rt_diff diff.c "${RT_TEST_LIBS}" TEST)

brlcad_addexec(rt_default_object default_object.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_default_object COMMAND rt_default_object)

brlcad_addexec(rt_bound_internal bound_internal.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_bound_internal COMMAND rt_bound_internal)

brlcad_addexec(rt_tree_prune tree_prune.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_tree_prune COMMAND rt_tree_prune)

brlcad_addexec(rt_reprep_prune reprep_prune.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_reprep_prune COMMAND rt_reprep_prune)

brlcad_addexec(rt_lcomb_large lcomb_large.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_lcomb_large COMMAND rt_lcomb_large)

if(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  brlcad_addexec(rt_binary_attribute binary_attribute.c "${RT_TEST_LIBS}" TEST)
  brlcad_add_test(NAME rt_binary_attribute COMMAND rt_binary_attribute)
endif(BRLCAD_ENABLE_BINARY_ATTRIBUTES)

# size testing
brlcad_addexec(db5_size db5_size.c "${RT_TEST_LIBS}" TEST)

# datum testing
brlcad_addexec(rt_datum rt_datum.c "${RT_TEST_LIBS}" TEST)
brlcad_add_test(NAME rt_datum_basic COMMAND rt_datum)

brlcad_addexec(rt_datum_annot_semantics datum_annot_semantics.c
  "librt;libwdb;libbv;libbu;${M_LIBRARY}" TEST)
if(TARGET osifont_osifont-lgpl3fe_ttf_cp)
  add_dependencies(rt_datum_annot_semantics osifont_osifont-lgpl3fe_ttf_cp)
endif()
brlcad_add_test(NAME rt_datum_annot_semantics COMMAND rt_datum_annot_semantics)

# perturb testing
#BRLCAD_ADDEXEC(rt_perturb rt_perturb.c "librt;libwdb" TEST)
#BRLCAD_ADD_TEST(NAME rt_perturb_basic COMMAND rt_perturb)

# cyclic path testing
brlcad_addexec(rt_cyclic cyclic.c "${RT_TEST_LIBS}" TEST)
brlcad_add_test(NAME rt_cyclic_basic COMMAND rt_cyclic ${CMAKE_CURRENT_SOURCE_DIR}/cyclic_tests.g)

brlcad_addexec(rt_cache cache.cpp "${RT_TEST_LIBS}" TEST)
brlcad_add_test(NAME rt_cache_serial_single_object COMMAND rt_cache 1)
brlcad_add_test(NAME rt_cache_parallel_single_object COMMAND rt_cache 2)
brlcad_add_test(NAME rt_cache_serial_multiple_identical_objects COMMAND rt_cache 3 10)
brlcad_add_test(NAME rt_cache_parallel_multiple_identical_objects  COMMAND rt_cache 4 10)
brlcad_add_test(NAME rt_cache_serial_multiple_different_objects COMMAND rt_cache 5 10)
brlcad_add_test(NAME rt_cache_parallel_multiple_different_objects  COMMAND rt_cache 6 10)
brlcad_add_test(NAME rt_cache_parallel_multiple_different_objects_hierarchy_1  COMMAND rt_cache 7 10)

# lod testing
brlcad_addexec(rt_lod lod.c "librt;libbg;${M_LIBRARY}" TEST)

# bv_polygon <-> sketch testing
brlcad_addexec(rt_bv_poly_sketch bv_poly_sketch.c "librt;libbv;${M_LIBRARY}" TEST)

# arb8 testing
brlcad_addexec(rt_arb8 arb8_tests.c "librt;libwdb;libbg;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_arb8_tests COMMAND rt_arb8)

# search stress and correctness testing
brlcad_addexec(rt_search_stress search_stress.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_search_stress COMMAND rt_search_stress)

# Tessellation timing utility (no automated test, just a tool)
brlcad_addexec(rt_tess_timing tess_timing.c "librt;libnmg;libwdb;libbu;${M_LIBRARY}" NO_INSTALL)

brlcad_addexec(bot_match bot_match.cpp "librt;libbg;${M_LIBRARY}" TEST)

# boolweave testing
brlcad_addexec(rt_boolweave rt_boolweave.c "${RT_TEST_LIBS}" TEST)
brlcad_add_test(NAME rt_boolweave COMMAND rt_boolweave)

# ft_shot vs ft_vshot: validates the sph vectorized shot matches the
# scalar shot, and times the vector callback (short 0.1s/phase budget)
brlcad_addexec(rt_vshot vshot_test.cpp "${RT_TEST_LIBS}" TEST)
brlcad_add_test(NAME rt_vshot COMMAND rt_vshot 0.1)

# Primitive-generic vshot benchmark: shot*N vs vshot(N) across batch
# sizes with coherent + incoherent rays (long-running tool, not a test)
brlcad_addexec(rt_vshot_bench vshot_bench.cpp "${RT_TEST_LIBS}" NO_INSTALL)

# Tests for primitive editing
add_subdirectory(edit)

# prim_tess: NMG tessellation quality tests
brlcad_addexec(rt_prim_tess prim_tess.c "librt;libnmg;libwdb;libbg;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_prim_tess COMMAND rt_prim_tess)

# Solid primitive-to-BRep topology
brlcad_addexec(rt_primitive_brep primitive_brep.cpp "librt;libbrep;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_primitive_brep COMMAND rt_primitive_brep)

# BREP archive I/O and concurrent import
brlcad_addexec(rt_brep_io brep_io.cpp "librt;libbrep;libbu;Threads::Threads;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_brep_io COMMAND rt_brep_io)

# stand-alone Crofton estimator tests
brlcad_addexec(rt_crofton crofton.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_crofton COMMAND rt_crofton)

# standalone DSP surface area + volume checks
brlcad_addexec(rt_dsp_sa_vol_check dsp_sa_vol_check.c "librt;libbu" TEST)
brlcad_add_test(NAME rt_dsp_sa_vol_check COMMAND rt_dsp_sa_vol_check)
# "-c" compares against Crofton
brlcad_add_test(NAME rt_dsp_sa_vol_compare COMMAND rt_dsp_sa_vol_check -c)


brlcad_addexec(rt_nonuniform_curve nonuniform_curve.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_nonuniform_curve COMMAND rt_nonuniform_curve)

brlcad_addexec(rt_nonuniform_ray nonuniform_ray.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_nonuniform_ray COMMAND rt_nonuniform_ray)

brlcad_addexec(rt_pipe_shotline pipe_shotline.c "librt;libwdb;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_pipe_shotline COMMAND rt_pipe_shotline)

# in-memory test for primitives
brlcad_addexec(rt_invalid_inmem invalid_inmem.c "librt;libbn;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME rt_test_invalid_in_memory COMMAND rt_invalid_inmem)

brlcad_addexec(rt_metaball_adjust metaball_adjust.c "${RT_TEST_LIBS}" TEST)
brlcad_add_test(NAME rt_metaball_adjust COMMAND rt_metaball_adjust)

set(
  distcheck_files
  CMakeLists.txt
  arb_intersect.g
  binary_attribute.c
  brep_io.cpp
  bound_internal.c
  crofton.c
  default_object.c
  datum_annot_semantics.c
  dsp_raytrace_check.c
  dsp_sa_vol_check.c
  primitive_brep.cpp
  reprep_prune.c
  brep_boolean_tests.g
  cyclic_tests.g
  extreme_ssi_test.g
  invalid_inmem.c
  lcomb_large.c
  matrix_tests.g
  metaball_adjust.c
  nonuniform_curve.c
  nonuniform_ray.c
  pipe_shotline.c
  nurbs_surfaces.g
  rt_datum.c
  rt_perturb.c
  search_stress.c
  static_init.cpp
  sketch.g
  prim_tess.c
  tess_timing.c
  tree_prune.c
)

cmakefiles(${distcheck_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
