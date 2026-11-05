include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

find_package(Threads REQUIRED)
brlcad_find_package(manifold REQUIRED)

brlcad_addexec(test_brep_ppx ppx.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_ppx COMMAND test_brep_ppx)

brlcad_addexec(test_brep_assembly assembly.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_assembly COMMAND test_brep_assembly)

brlcad_addexec(test_brep_fast_budget brep_fast_budget.cpp "libbu" TEST)
brlcad_add_test(NAME brep_fast_budget COMMAND test_brep_fast_budget)

brlcad_addexec(test_brep_point_inside brep_point_inside.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_point_inside COMMAND test_brep_point_inside)

brlcad_addexec(test_brep_boolean_tolerances brep_boolean_tolerances.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_tolerances COMMAND test_brep_boolean_tolerances)

brlcad_addexec(test_brep_boolean_xor brep_boolean_xor.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_xor COMMAND test_brep_boolean_xor)
set_tests_properties(brep_boolean_xor PROPERTIES TIMEOUT 30)
brlcad_add_test(NAME brep_boolean_xor_manifold_shadow COMMAND test_brep_boolean_xor)
set_tests_properties(
  brep_boolean_xor_manifold_shadow
  PROPERTIES
    ENVIRONMENT "LIBBREP_BOOLEAN_MANIFOLD_SHADOW=1"
    FAIL_REGULAR_EXPRESSION
      "[1-9][0-9]* selection disagreements;[1-9][0-9]* orientation disagreements"
    PASS_REGULAR_EXPRESSION "Boolean Manifold shadow: [1-9][0-9]* decisions"
    TIMEOUT 120
)

brlcad_addexec(test_brep_boolean_closed_ssi brep_boolean_closed_ssi.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_closed_ssi_direct COMMAND test_brep_boolean_closed_ssi)
set_tests_properties(brep_boolean_closed_ssi_direct PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_vertex_hits brep_boolean_vertex_hits.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_vertex_hits COMMAND test_brep_boolean_vertex_hits)
set_tests_properties(brep_boolean_vertex_hits PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_resource_limit brep_boolean_resource_limit.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_resource_limit COMMAND test_brep_boolean_resource_limit)
set_tests_properties(brep_boolean_resource_limit PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_failures brep_boolean_failures.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_failures COMMAND test_brep_boolean_failures)
set_tests_properties(brep_boolean_failures PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_comb_conversion_failures brep_comb_conversion_failures.cpp "libwdb;librt;libbrep;libbu" TEST)
brlcad_add_test(NAME brep_comb_conversion_failures COMMAND test_brep_comb_conversion_failures)
set_tests_properties(brep_comb_conversion_failures PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_crofton_degenerate_partition brep_crofton_degenerate_partition.cpp "libwdb;librt;libbrep;libbu" TEST)
brlcad_add_test(NAME brep_crofton_degenerate_partition COMMAND test_brep_crofton_degenerate_partition)
set_tests_properties(brep_crofton_degenerate_partition PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_corpus brep_corpus.cpp "librt;libbrep;libwdb;libbu" TEST)
if(WIN32)
  target_link_libraries(test_brep_corpus Dbghelp)
endif()

brlcad_add_test(
  NAME brep_corpus_thin_partition_policy
  COMMAND test_brep_corpus --classification-self-test)
set_tests_properties(brep_corpus_thin_partition_policy PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_corpus_thin_partition_divergence
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DBREP_CORPUS=$<TARGET_FILE:test_brep_corpus>
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_thin_partition/thin.g
    -DREPORT=${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_thin_partition/thin.csv
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_corpus_thin_partition.cmake)
set_tests_properties(brep_corpus_thin_partition_divergence PROPERTIES TIMEOUT 30)

# This small extraction from havoc.asc exercises a three-operand subtraction
# whose second Boolean leaves a full 3/1 source-edge partition separated by
# three tolerance-scale connector edges.  The connectors must be collapsed
# transactionally, after which the remaining narrow planar opening is capped
# using canonical topology vertices rather than independently fitted edge-end
# averages.  The formerly produced 12-face solid was topologically valid but
# semantically wrong, so keep the dense ray comparison as part of the gate.
set(BREP_BOOLEAN_ARMOR2_ASC ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean_armor2.asc)
set(BREP_BOOLEAN_ARMOR2_DB ${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_armor2.g)
if(BRLCAD_ENABLE_TCL)
  set(BREP_BOOLEAN_ASC_CONVERTER $<TARGET_FILE:asc2g>)
  set(BREP_BOOLEAN_ASC_DEPENDENCY asc2g)
else()
  set(BREP_BOOLEAN_ASC_CONVERTER $<TARGET_FILE:gcv>)
  set(BREP_BOOLEAN_ASC_DEPENDENCY gcv)
endif()

add_custom_command(
  OUTPUT ${BREP_BOOLEAN_ARMOR2_DB}
  COMMAND ${CMAKE_COMMAND} -E remove ${BREP_BOOLEAN_ARMOR2_DB}
  COMMAND ${BRLCAD_SANITIZER_BUILD_TOOL_LAUNCHER}
    ${BREP_BOOLEAN_ASC_CONVERTER} ${BREP_BOOLEAN_ARMOR2_ASC} ${BREP_BOOLEAN_ARMOR2_DB}
  DEPENDS ${BREP_BOOLEAN_ASC_DEPENDENCY} ${BREP_BOOLEAN_ARMOR2_ASC}
  VERBATIM
)
add_custom_target(brep_boolean_armor2_fixture DEPENDS ${BREP_BOOLEAN_ARMOR2_DB})
add_dependencies(test_brep_corpus brep_boolean_armor2_fixture)

function(add_brep_boolean_fixture fixture)
  set(fixture_asc ${CMAKE_CURRENT_SOURCE_DIR}/${fixture}.asc)
  set(fixture_db ${CMAKE_CURRENT_BINARY_DIR}/${fixture}.g)
  add_custom_command(
    OUTPUT ${fixture_db}
    COMMAND ${CMAKE_COMMAND} -E remove ${fixture_db}
    COMMAND ${BRLCAD_SANITIZER_BUILD_TOOL_LAUNCHER}
      ${BREP_BOOLEAN_ASC_CONVERTER} ${fixture_asc} ${fixture_db}
    DEPENDS ${BREP_BOOLEAN_ASC_DEPENDENCY} ${fixture_asc}
    VERBATIM
  )
  add_custom_target(${fixture}_fixture DEPENDS ${fixture_db})
  add_dependencies(test_brep_corpus ${fixture}_fixture)
endfunction()

function(add_brep_boolean_failure_fixture fixture stage reason)
  add_brep_boolean_fixture(${fixture})
  set(fixture_db ${CMAKE_CURRENT_BINARY_DIR}/${fixture}.g)
  brlcad_add_test(
    NAME brep_corpus_${fixture}
    COMMAND test_brep_corpus
      --database ${fixture_db}
      --component failure.r
      --output ${CMAKE_CURRENT_BINARY_DIR}/${fixture}.csv
      --rays 1
      --timeout-ms 30000
      --require-failure-stage ${stage}
      --require-failure-reason ${reason}
  )
  set_tests_properties(brep_corpus_${fixture} PROPERTIES TIMEOUT 45)
endfunction()

function(add_brep_boolean_success_fixture fixture)
  add_brep_boolean_fixture(${fixture})
  set(fixture_db ${CMAKE_CURRENT_BINARY_DIR}/${fixture}.g)
  brlcad_add_test(
    NAME brep_corpus_${fixture}
    COMMAND test_brep_corpus
      --database ${fixture_db}
      --component failure.r
      --output ${CMAKE_CURRENT_BINARY_DIR}/${fixture}.csv
      --rays 16384
      --timeout-ms 30000
      --require-semantic-pass
      --require-fidelity exact
  )
  set_tests_properties(brep_corpus_${fixture} PROPERTIES TIMEOUT 45)
endfunction()

# Standalone first-failing-node snapshots from the M35 hard set.  These
# fixtures freeze the three repeated signatures before their geometry logic is
# changed, so a later repair must produce an explicit per-region transition
# rather than silently changing or losing the failure evidence.
add_brep_boolean_failure_fixture(
  brep_boolean_m35_open_ssi_missing_mate
  edge_repair
  open_ssi_missing_mate
)
add_brep_boolean_failure_fixture(
  brep_boolean_m35_naked_edge_partition
  edge_repair
  naked_edge_partition
)
add_brep_boolean_failure_fixture(
  brep_boolean_m35_arrangement_excess_outgoing
  trim_arrangement
  arrangement_excess_outgoing
)
add_brep_boolean_success_fixture(brep_boolean_havoc_inner_loop_open_ssi)

# An offset ARB4 plane cuts a 40-km planetary-scale spheroid shell within a
# fraction of a millimeter of two trim endpoints.  Model-space tolerances must
# be converted to each face's UV metric while clipping the SSI curves, and the
# assembled trims must retain the certified 3D SSI arcs rather than refitting
# their interiors from revolution-surface UV curves.
set(BREP_BOOLEAN_OFFSET_SPHEROID_ASC
  ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean_offset_spheroid.asc)
set(BREP_BOOLEAN_OFFSET_SPHEROID_DB
  ${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_offset_spheroid.g)
add_custom_command(
  OUTPUT ${BREP_BOOLEAN_OFFSET_SPHEROID_DB}
  COMMAND ${CMAKE_COMMAND} -E remove ${BREP_BOOLEAN_OFFSET_SPHEROID_DB}
  COMMAND ${BRLCAD_SANITIZER_BUILD_TOOL_LAUNCHER}
    ${BREP_BOOLEAN_ASC_CONVERTER} ${BREP_BOOLEAN_OFFSET_SPHEROID_ASC}
    ${BREP_BOOLEAN_OFFSET_SPHEROID_DB}
  DEPENDS ${BREP_BOOLEAN_ASC_DEPENDENCY}
    ${BREP_BOOLEAN_OFFSET_SPHEROID_ASC}
  VERBATIM
)
add_custom_target(brep_boolean_offset_spheroid_fixture
  DEPENDS ${BREP_BOOLEAN_OFFSET_SPHEROID_DB})
add_dependencies(test_brep_corpus brep_boolean_offset_spheroid_fixture)

brlcad_add_test(
  NAME brep_corpus_offset_spheroid_cut
  COMMAND test_brep_corpus
    --database ${BREP_BOOLEAN_OFFSET_SPHEROID_DB}
    --component offset_spheroid_cut.c
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_offset_spheroid_cut.csv
    --rays 2048
    --timeout-ms 30000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_offset_spheroid_cut PROPERTIES TIMEOUT 45)

brlcad_add_test(
  NAME brep_corpus_armor2_source_edge_partition
  COMMAND test_brep_corpus
    --database ${BREP_BOOLEAN_ARMOR2_DB}
    --component armor2_source_edge_partition.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_armor2_source_edge_partition.csv
    --rays 262144
    --timeout-ms 30000
    --require-semantic-pass
)
set_tests_properties(brep_corpus_armor2_source_edge_partition PROPERTIES TIMEOUT 45)

# Four SSI arcs from two adjacent source faces form a transverse closed loop
# on the cutter's closed surface.  Their input ordering leaves two open chains
# after the ordinary greedy linking pass; the chains must be joined only after
# certifying that their remaining endpoints are opposite representations of
# the same periodic seam point.
set(BREP_BOOLEAN_PERIODIC_CHAIN_ASC
  ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean_periodic_chain.asc)
set(BREP_BOOLEAN_PERIODIC_CHAIN_DB
  ${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_periodic_chain.g)
add_custom_command(
  OUTPUT ${BREP_BOOLEAN_PERIODIC_CHAIN_DB}
  COMMAND ${CMAKE_COMMAND} -E remove ${BREP_BOOLEAN_PERIODIC_CHAIN_DB}
  COMMAND ${BRLCAD_SANITIZER_BUILD_TOOL_LAUNCHER}
    ${BREP_BOOLEAN_ASC_CONVERTER} ${BREP_BOOLEAN_PERIODIC_CHAIN_ASC}
    ${BREP_BOOLEAN_PERIODIC_CHAIN_DB}
  DEPENDS ${BREP_BOOLEAN_ASC_DEPENDENCY} ${BREP_BOOLEAN_PERIODIC_CHAIN_ASC}
  VERBATIM
)
add_custom_target(brep_boolean_periodic_chain_fixture
  DEPENDS ${BREP_BOOLEAN_PERIODIC_CHAIN_DB})
add_dependencies(test_brep_corpus brep_boolean_periodic_chain_fixture)

brlcad_add_test(
  NAME brep_corpus_periodic_ssi_chain
  COMMAND test_brep_corpus
    --database ${BREP_BOOLEAN_PERIODIC_CHAIN_DB}
    --component rim_tenth_hole
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_periodic_ssi_chain.csv
    --rays 4096
    --timeout-ms 30000
    --require-semantic-pass
)
set_tests_properties(brep_corpus_periodic_ssi_chain PROPERTIES TIMEOUT 45)

# The first union leaves a rectangular inner trimming loop on a closed
# cylinder.  During the second union, each cutter face meets that cylinder in
# two SSI fragments: one on either side of the parameter seam, with the other
# endpoints on the existing inner loop.  Those fragments must be paired with
# the corresponding inner- and outer-loop paths before the annular face can be
# partitioned.
set(BREP_BOOLEAN_PERIODIC_INNER_LOOP_ASC
  ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean_periodic_inner_loop.asc)
set(BREP_BOOLEAN_PERIODIC_INNER_LOOP_DB
  ${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_periodic_inner_loop.g)
add_custom_command(
  OUTPUT ${BREP_BOOLEAN_PERIODIC_INNER_LOOP_DB}
  COMMAND ${CMAKE_COMMAND} -E remove ${BREP_BOOLEAN_PERIODIC_INNER_LOOP_DB}
  COMMAND ${BRLCAD_SANITIZER_BUILD_TOOL_LAUNCHER}
    ${BREP_BOOLEAN_ASC_CONVERTER} ${BREP_BOOLEAN_PERIODIC_INNER_LOOP_ASC}
    ${BREP_BOOLEAN_PERIODIC_INNER_LOOP_DB}
  DEPENDS ${BREP_BOOLEAN_ASC_DEPENDENCY}
    ${BREP_BOOLEAN_PERIODIC_INNER_LOOP_ASC}
  VERBATIM
)
add_custom_target(brep_boolean_periodic_inner_loop_fixture
  DEPENDS ${BREP_BOOLEAN_PERIODIC_INNER_LOOP_DB})
add_dependencies(test_brep_corpus
  brep_boolean_periodic_inner_loop_fixture)

brlcad_add_test(
  NAME brep_corpus_periodic_inner_loop_partition
  COMMAND test_brep_corpus
    --database ${BREP_BOOLEAN_PERIODIC_INNER_LOOP_DB}
    --component periodic_inner_loop_union.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_periodic_inner_loop.csv
    --rays 16384
    --timeout-ms 30000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_periodic_inner_loop_partition
  PROPERTIES TIMEOUT 45)

# The first subtraction leaves inner loops on the two sloped ARB6 faces.  The
# TGC subtraction then produces one open SSI chain on each face whose two
# endpoints lie on the same inner-loop edge.  Those chains must replace the
# corresponding old hole-boundary intervals; treating them as outer-loop cuts
# leaves full source edges opposite partial adjacent-face edges and produces a
# valid but non-solid B-Rep.
brlcad_add_test(
  NAME brep_corpus_inner_loop_subtraction
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/boolean-ops.g
    --component c-a-b
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_inner_loop_subtraction.csv
    --rays 16384
    --timeout-ms 60000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_inner_loop_subtraction PROPERTIES TIMEOUT 75)

brlcad_add_test(
  NAME brep_corpus_semantic_smoke
  COMMAND test_brep_corpus
    --database ${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    --component 00004_non-trivally_disjoint_union.c
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_semantic_smoke.csv
    --rays 2048
    --timeout-ms 30000
    --require-semantic-pass
)
set_tests_properties(brep_corpus_semantic_smoke PROPERTIES TIMEOUT 45)

brlcad_add_test(
  NAME brep_corpus_dsp_no_crash
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/gaia.g
    --component terrain.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_dsp_no_crash.csv
    --rays 64
    --timeout-ms 15000
)
set_tests_properties(brep_corpus_dsp_no_crash PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_corpus_revolve_semantics
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/primitives.g
    --component revolve.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_revolve_semantics.csv
    --rays 16384
    --timeout-ms 15000
    --require-semantic-pass
)
set_tests_properties(brep_corpus_revolve_semantics PROPERTIES TIMEOUT 30)

# A transformed real-world PIPE contains a genuine sub-tolerance bend whose
# direction dot product rounds to -1.  Omitting its tolerance-coincident bend
# face is robust but must be reported as an approximation rather than exact
# conversion.
brlcad_add_test(
  NAME brep_corpus_pipe_sub_tolerance_fidelity
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/bicycle_wheel.g
    --component spki_1_1_1
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_pipe_sub_tolerance_fidelity.csv
    --rays 16384
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity approximate
)
set_tests_properties(brep_corpus_pipe_sub_tolerance_fidelity PROPERTIES TIMEOUT 30)

# Ordinary representable PIPE bends remain exact; the data-dependent fidelity
# path must not classify every PIPE conversion as approximate.
brlcad_add_test(
  NAME brep_corpus_pipe_exact_fidelity
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/primitives.g
    --component pipe.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_pipe_exact_fidelity.csv
    --rays 4096
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_pipe_exact_fidelity PROPERTIES TIMEOUT 30)

# PART cone roots can lie numerically on a tangent join plane.  The analytic
# shot path must still test both clipped spherical caps; otherwise it emits a
# zero-thickness source partition where the exact B-Rep contains material.
brlcad_add_test(
  NAME brep_corpus_part_tangent_join_semantics
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/primitives.g
    --component part.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_part_tangent_join_semantics.csv
    --rays 16384
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_part_tangent_join_semantics PROPERTIES TIMEOUT 30)

# EBM extrusion may leave several disconnected OT_SAME loops on each planar
# cap faceuse.  NMG permits that representation, but OpenNURBS requires one
# outer loop per face, so conversion must split the cap into planar faces
# without triangulating it or changing its semantics.
brlcad_add_test(
  NAME brep_corpus_ebm_multi_outer_loop_semantics
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/primitives.g
    --component ebm.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_ebm_multi_outer_loop_semantics.csv
    --rays 8192
    --timeout-ms 15000
    --require-semantic-pass
)
set_tests_properties(brep_corpus_ebm_multi_outer_loop_semantics PROPERTIES TIMEOUT 30)

# A simple planar concave NMG face is directly representable as a trimmed
# planar B-Rep face; triangulation must not be required to preserve either
# topology or ray semantics.
brlcad_add_test(
  NAME brep_corpus_concave_arb_planar_semantics
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/primitives.g
    --component concave_arb.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_concave_arb_planar_semantics.csv
    --rays 32768
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_concave_arb_planar_semantics PROPERTIES TIMEOUT 30)

# This subtraction has a linked face loop containing a model-space
# tolerance-scale arc.  Omitting that arc must also merge its endpoint nodes;
# otherwise the selected arrangement has an artificial open boundary and the
# formerly exact three-ARB result becomes indeterminate.
brlcad_add_test(
  NAME brep_corpus_truck_r14_degenerate_arc
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/truck.g
    --component r14
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_truck_r14_degenerate_arc.csv
    --rays 16384
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_truck_r14_degenerate_arc PROPERTIES TIMEOUT 30)

# The tire tree first intersects periodic surfaces whose independently pulled
# SSI curves leave tolerance-scale endpoint spikes.  A later subtraction then
# reuses those repaired faces: periodic seam assembly must preserve loop vertex
# identity, and the two output uses of each inherited source edge must be mated
# using their source topology plus complete bidirectional curve coverage.
brlcad_add_test(
  NAME brep_corpus_bicycle_rtire_source_edge_provenance
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/bicycle_wheel.g
    --component rtire.r1
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_bicycle_rtire_source_edge_provenance.csv
    --rays 4096
    --timeout-ms 120000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_bicycle_rtire_source_edge_provenance PROPERTIES TIMEOUT 150)

# The two crossing spoke families produce open SSI trims whose adaptive
# pullbacks follow the correct model-space curves with a nonuniform parameter
# speed.  Treat curve-locus agreement, rather than equal normalized
# parameters, as authoritative or the nine real overlaps become
# indeterminate.  The source tree also contains transformed spoke PIPEs with
# genuine but tolerance-coincident bends, so their omission is reported as a
# bounded approximation.
brlcad_add_test(
  NAME brep_corpus_bicycle_spokes_reparameterized_ssi
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/bicycle_wheel.g
    --component spokes.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_bicycle_spokes_reparameterized_ssi.csv
    --rays 4096
    --timeout-ms 120000
    --require-semantic-pass
    --require-fidelity approximate
)
set_tests_properties(brep_corpus_bicycle_spokes_reparameterized_ssi PROPERTIES TIMEOUT 150)

# The ETO bump meets the stepped wheel cylinder along a complete circle.
# Each selected face represents that circle as one closed edge, but the
# cylinder and revolution parameterizations place their topological seams a
# quarter turn apart.  The two boundaries must be split at both seam points
# and mated as ordinary arcs; endpoint-only stitching leaves a valid but open
# shell.
brlcad_add_test(
  NAME brep_corpus_pinewood_outer_wheel_offset_closed_seam
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/pinewood.g
    --component outer-wheel.c_01
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_pinewood_outer_wheel_offset_closed_seam.csv
    --rays 16384
    --timeout-ms 30000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_pinewood_outer_wheel_offset_closed_seam PROPERTIES TIMEOUT 45)

# These ellipsoid/TGC intersections produce full closed SSI events whose two
# parameter curves use different closed-surface seams.  Re-pulling the curves
# independently opens the split loops and silently preserves the full
# ellipsoid instead of the intersection.
foreach(star_component r4 r9)
  brlcad_add_test(
    NAME brep_corpus_star_${star_component}_closed_ssi
    COMMAND test_brep_corpus
      --database ${CMAKE_BINARY_DIR}/share/db/star.g
      --component ${star_component}
      --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_star_${star_component}_closed_ssi.csv
      --rays 16384
      --timeout-ms 15000
      --require-semantic-pass
      --require-fidelity exact
  )
  set_tests_properties(brep_corpus_star_${star_component}_closed_ssi PROPERTIES TIMEOUT 30)
endforeach()

# A TGC's height may point opposite its A/B frame normal.  Such a primitive
# used to produce a valid but inward-oriented B-Rep.  The two end cylinders in
# this union have opposite height directions; mixing their shell orientations
# caused the B-Rep ray path to merge separated partitions and lose material.
brlcad_add_test(
  NAME brep_corpus_havoc_bri5_tgc_orientation
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/havoc.g
    --component r.bri5
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_havoc_bri5_tgc_orientation.csv
    --rays 16384
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_havoc_bri5_tgc_orientation PROPERTIES TIMEOUT 30)

# General ellipsoids may use a left-handed A/B/C frame.  Its affine reflection
# reverses the NURBS sphere face and must be reflected in the B-Rep topology.
# This scene mixes both handednesses in disconnected unions, where the former
# inward shells caused distinct ray partitions to be paired across empty space.
brlcad_add_test(
  NAME brep_corpus_lgt_all_ell_orientation
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/lgt-test.g
    --component all.g
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_lgt_all_ell_orientation.csv
    --rays 16384
    --timeout-ms 30000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_lgt_all_ell_orientation PROPERTIES TIMEOUT 45)

# The TGC in this nested subtraction begins exactly on an inner planar wall.
# SSI reports the boundary circle twice: one matched model/parameter triplet
# and one lower-quality approximation of the same parameter-space locus.
# Consuming both creates tolerance-scale triangular trim loops and a nonsolid
# result.  Deduplication must retain the better triplet without erasing the
# nearby thin box-shell geometry.
brlcad_add_test(
  NAME brep_corpus_m35_r817_duplicate_closed_ssi
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/m35.g
    --component r817
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_m35_r817_duplicate_closed_ssi.csv
    --rays 16384
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_m35_r817_duplicate_closed_ssi PROPERTIES TIMEOUT 30)

# The final subtractor in this component is a distant, nonplanar ARB.  It
# cannot affect the already evaluated minuend, so exact tree evaluation must
# prove disjointness from loose primitive/tree bounds without trying to turn
# that irrelevant ARB into a B-Rep.
brlcad_add_test(
  NAME brep_corpus_m35_r1047_disjoint_invalid_subtractor
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/m35.g
    --component r1047
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_m35_r1047_disjoint_invalid_subtractor.csv
    --rays 16384
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_m35_r1047_disjoint_invalid_subtractor PROPERTIES TIMEOUT 30)

# At 16K deterministic rays this exact cylinder has one endpoint sample just
# above the p99 threshold; the percentile settles well inside the unchanged
# threshold at 64K.  Exercise the bounded sampling retry independently of the
# full corpus target.
brlcad_add_test(
  NAME brep_corpus_m35_r134_adaptive_sampling
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/m35.g
    --component r134
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_m35_r134_adaptive_sampling.csv
    --rays 16384
    --max-rays 65536
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_m35_r134_adaptive_sampling PROPERTIES TIMEOUT 30)

# The Manifold shadow previously sent this valid planar split proxy through
# the general B-Rep CDT path, whose failure handling calls bu_bomb().  The
# certified polygon path must either classify it or abstain without invoking
# CDT and without terminating the isolated corpus child.
brlcad_add_test(
  NAME brep_corpus_m35_r608_manifold_shadow_no_crash
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/m35.g
    --component r608
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_m35_r608_manifold_shadow_no_crash.csv
    --rays 64
    --timeout-ms 15000
    --manifold-shadow
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_m35_r608_manifold_shadow_no_crash
  PROPERTIES TIMEOUT 30)

# Curved split faces in this shell used to reach in-process CDT proxy
# tessellation and trigger an access violation.  Unsupported curved proxies
# must now abstain before tessellation while exact evaluation continues.
brlcad_add_test(
  NAME brep_corpus_earth_crust_manifold_shadow_no_crash
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/earth.g
    --component crust-core-subtract.c
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_earth_crust_manifold_shadow_no_crash.csv
    --rays 64
    --timeout-ms 15000
    --manifold-shadow
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_earth_crust_manifold_shadow_no_crash
  PROPERTIES TIMEOUT 30)

# This union's two ARB8 operands meet exactly on a partial planar face.  Exact
# resolution is preferred; if a bounded regularization retry is still needed,
# it must preserve CSG semantics and will report approximate fidelity.
brlcad_add_test(
  NAME brep_corpus_bldg391_l1_regularized_semantics
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/bldg391.g
    --component l1
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_bldg391_l1_regularized_semantics.csv
    --rays 16384
    --timeout-ms 15000
    --require-semantic-pass
)
set_tests_properties(brep_corpus_bldg391_l1_regularized_semantics PROPERTIES TIMEOUT 30)

# HRT and metaball conversion samples implicit surfaces into closed faceted
# B-Reps.  Their declared sampling scales loosen only the endpoint-distance
# gate; topology, chord, area, and volume checks retain the corpus limits.
brlcad_add_test(
  NAME brep_corpus_hrt_approximate_semantics
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/primitives.g
    --component hrt.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_hrt_approximate_semantics.csv
    --rays 16384
    --timeout-ms 30000
    --require-semantic-pass
    --require-fidelity approximate
)
set_tests_properties(brep_corpus_hrt_approximate_semantics PROPERTIES TIMEOUT 45)

brlcad_add_test(
  NAME brep_corpus_metaball_approximate_semantics
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/primitives.g
    --component metaball.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_metaball_approximate_semantics.csv
    --rays 16384
    --timeout-ms 15000
    --require-semantic-pass
    --require-fidelity approximate
)
set_tests_properties(brep_corpus_metaball_approximate_semantics PROPERTIES TIMEOUT 30)

# The die pips are shallowly overlapping spheres.  Generic SSI historically
# returned interior approximation circles for those near-tangent pairs, after
# which whole-face classification silently discarded four of the six spheres.
# Require the exact analytic sphere/sphere intersection and preserve all six
# trimmed faces through the disjoint-union tree.
brlcad_add_test(
  NAME brep_corpus_die_shallow_sphere_union
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/die.g
    --component dots6.c
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_die_shallow_sphere_union.csv
    --rays 16384
    --timeout-ms 30000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_die_shallow_sphere_union PROPERTIES TIMEOUT 45)

# The final xypanel subtraction leaves a mixed SSI/source-edge cycle on a
# nonplanar NURBS face.  Closing it requires the retained SSI parameter curves;
# a fresh geometric pullback alone is outside the Boolean repair tolerance.
brlcad_add_test(
  NAME brep_corpus_aet_xypanel_nonplanar_cap
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/aet.g
    --component xypanel.r
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_aet_xypanel.csv
    --rays 4096
    --timeout-ms 90000
    --require-semantic-pass
    --require-fidelity approximate
)
set_tests_properties(brep_corpus_aet_xypanel_nonplanar_cap PROPERTIES TIMEOUT 120)

function(add_brep_corpus_arrangement_regression test_name database component)
  brlcad_add_test(
    NAME ${test_name}
    COMMAND test_brep_corpus
      --database ${CMAKE_BINARY_DIR}/share/db/${database}
      --component ${component}
      --output ${CMAKE_CURRENT_BINARY_DIR}/${test_name}.csv
      --rays 4096
      --timeout-ms 30000
      --require-semantic-pass
  )
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 45)
endfunction()

# Reversed overlap intervals on closed trims must retain their parameter/UV
# endpoint pairing.  Losing that pairing over-selects a neighboring span and
# leaves the arrangement graph unbalanced.
add_brep_corpus_arrangement_regression(brep_corpus_arrangement_demo_r9m48 demo.g r9m48)
add_brep_corpus_arrangement_regression(brep_corpus_arrangement_galileo_cutouts galileo.g cutouts)
add_brep_corpus_arrangement_regression(brep_corpus_arrangement_galileo_frontcut galileo.g frontcut)
add_brep_corpus_arrangement_regression(brep_corpus_arrangement_m35_r394 m35.g r394)
add_brep_corpus_arrangement_regression(brep_corpus_arrangement_m35_r411 m35.g r411)
add_brep_corpus_arrangement_regression(brep_corpus_arrangement_star_cutouts star.g cutouts)
add_brep_corpus_arrangement_regression(brep_corpus_arrangement_star_frontcut star.g frontcut)
add_brep_corpus_arrangement_regression(brep_corpus_arrangement_star_r16 star.g r16)

add_custom_target(
  brep-corpus-matrix
  COMMAND test_brep_corpus
    --database-dir ${CMAKE_BINARY_DIR}/share/db
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_matrix.csv
    --rays 16384
    --max-rays 262144
    --timeout-ms 60000
    --manifold-shadow
  DEPENDS test_brep_corpus
  COMMENT "Evaluating the component-level share/db B-Rep corpus matrix"
  VERBATIM
)

brlcad_addexec(test_brep_boolean_box_cut brep_boolean_box_cut.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_box_cut COMMAND test_brep_boolean_box_cut)
set_tests_properties(brep_boolean_box_cut PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_shared_edge brep_boolean_shared_edge.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_shared_edge COMMAND test_brep_boolean_shared_edge)
set_tests_properties(brep_boolean_shared_edge PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_coplanar_overlap brep_boolean_coplanar_overlap.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_coplanar_overlap COMMAND test_brep_boolean_coplanar_overlap)
set_tests_properties(brep_boolean_coplanar_overlap PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_coplanar_sliver brep_boolean_coplanar_sliver.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_coplanar_sliver COMMAND test_brep_boolean_coplanar_sliver)
set_tests_properties(brep_boolean_coplanar_sliver PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_ssi_events brep_ssi_events.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_ssi_events COMMAND test_brep_ssi_events)
set_tests_properties(brep_ssi_events PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_intersection_resource_limit brep_intersection_resource_limit.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_intersection_resource_limit COMMAND test_brep_intersection_resource_limit)
set_tests_properties(brep_intersection_resource_limit PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_surface_domain brep_surface_domain.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_surface_domain COMMAND test_brep_surface_domain)
set_tests_properties(brep_surface_domain PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_curve_overlap_events brep_curve_overlap_events.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_curve_overlap_events COMMAND test_brep_curve_overlap_events)
set_tests_properties(brep_curve_overlap_events PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_curve_equivalence brep_curve_equivalence.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_curve_equivalence COMMAND test_brep_curve_equivalence)
set_tests_properties(brep_curve_equivalence PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_loop_degenerate brep_boolean_loop_degenerate.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_loop_degenerate COMMAND test_brep_boolean_loop_degenerate)
set_tests_properties(brep_boolean_loop_degenerate PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_endpoint_pair brep_boolean_endpoint_pair.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_endpoint_pair COMMAND test_brep_boolean_endpoint_pair)
set_tests_properties(brep_boolean_endpoint_pair PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_manifold_provenance brep_boolean_manifold_provenance.cpp "manifold::manifold;libbu" TEST)
brlcad_add_test(NAME brep_boolean_manifold_provenance COMMAND test_brep_boolean_manifold_provenance)
set_tests_properties(brep_boolean_manifold_provenance PROPERTIES TIMEOUT 30)

brlcad_addexec(test_brep_boolean_manifold brep_boolean_manifold.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_boolean_manifold COMMAND test_brep_boolean_manifold)
set_tests_properties(brep_boolean_manifold PROPERTIES TIMEOUT 60)

brlcad_addexec(test_brep_shape_recognition_sphere brep_shape_recognition_sphere.cpp "libbrep-static;libbu-static" TEST)
brlcad_add_test(NAME brep_shape_recognition_sphere COMMAND test_brep_shape_recognition_sphere)

brlcad_add_test(
  NAME brep_boolean
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean/brep_boolean_tests.g
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_empty_intersection
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_empty/brep_boolean_tests.g
    -DCOMBINATION=00006_non-trivally_disjoint_intersection.c
    -DEXPECT_EMPTY=ON
    -DDIRECT_BOOLEAN=ON
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_empty_intersection PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_closed_ssi
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_closed_ssi/brep_boolean_tests.g
    -DCOMBINATION=00007_large_volume_union.c
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_closed_ssi PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_shared_edge_database
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_shared_edge/brep_boolean_tests.g
    -DSHARED_EDGE_CONTACT=ON
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_shared_edge_database PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_partial_shared_face_subtraction
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_partial_shared_face_subtraction/brep_boolean_tests.g
    -DCOMBINATION:STRING=00026_planar_partially_shared_face_no_shared_volume_subtraction.c
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_partial_shared_face_subtraction PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_partial_shared_face_intersection
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_partial_shared_face_intersection/brep_boolean_tests.g
    -DCOMBINATION:STRING=00027_planar_partially_shared_face_no_shared_volume_intersection.c
    -DEXPECT_EMPTY=ON
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_partial_shared_face_intersection PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_partial_shared_face_union
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_partial_shared_face_union/brep_boolean_tests.g
    -DCOMBINATION:STRING=00030_planar_1_shared_face_shared_volume_union.c
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_partial_shared_face_union PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_full_shared_face_union
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_full_shared_face_union/brep_boolean_tests.g
    -DCOMBINATION:STRING=00028_planar_fully_shared_face_no_shared_volume_union.c
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_full_shared_face_union PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_full_shared_face_subtraction
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_CURRENT_SOURCE_DIR}/../../librt/tests/brep_boolean_tests.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_full_shared_face_subtraction/brep_boolean_tests.g
    -DCOMBINATION:STRING=00029_planar_fully_shared_face_no_shared_volume_subtraction.c
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_full_shared_face_subtraction PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_m35_r614
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_BINARY_DIR}/share/db/m35.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_m35_r614/m35.g
    -DCOMBINATION:STRING=r614
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_m35_r614 PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_m35_r854
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_BINARY_DIR}/share/db/m35.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_m35_r854/m35.g
    -DCOMBINATION:STRING=r854
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_m35_r854 PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_tree_retries
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DBREP_CORPUS=$<TARGET_FILE:test_brep_corpus>
    -DSOURCE_DB=${CMAKE_BINARY_DIR}/share/db/havoc.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_tree_retries/havoc.g
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean_tree_retry.cmake)
set_tests_properties(brep_boolean_tree_retries PROPERTIES TIMEOUT 60)

brlcad_add_test(
  NAME brep_boolean_cloud_r_bak
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_BINARY_DIR}/share/db/cube.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_cloud_r_bak/cube.g
    -DCOMBINATION:STRING=cloud.r.bak
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_cloud_r_bak PROPERTIES TIMEOUT 30)

brlcad_add_test(
  NAME brep_boolean_bounded_halfspace
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_BINARY_DIR}/share/db/cube.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_bounded_halfspace/cube.g
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean_halfspace.cmake)
set_tests_properties(brep_boolean_bounded_halfspace PROPERTIES TIMEOUT 60)

brlcad_add_test(
  NAME brep_corpus_operators_union_absorption
  COMMAND test_brep_corpus
    --database ${CMAKE_BINARY_DIR}/share/db/operators.g
    --component union
    --output ${CMAKE_CURRENT_BINARY_DIR}/brep_corpus_operators_union_absorption.csv
    --rays 4096
    --timeout-ms 180000
    --require-semantic-pass
    --require-fidelity exact
)
set_tests_properties(brep_corpus_operators_union_absorption PROPERTIES TIMEOUT 240)

brlcad_add_test(
  NAME brep_boolean_havoc_r_fuse1_no_crash
  COMMAND ${CMAKE_COMMAND}
    -DMGED=$<TARGET_FILE:mged>
    -DSOURCE_DB=${CMAKE_BINARY_DIR}/share/db/havoc.g
    -DTEST_DB=${CMAKE_CURRENT_BINARY_DIR}/brep_boolean_havoc_r_fuse1/havoc.g
    -DCOMBINATION:STRING=r.fuse1
    -DALLOW_FAILURE=1
    -DEXPECT_FAILURE_OUTPUT:STRING=Boolean.result.is.indeterminate
    -DEXPECT_NO_RESULT=1
    -P ${CMAKE_CURRENT_SOURCE_DIR}/brep_boolean.cmake)
set_tests_properties(brep_boolean_havoc_r_fuse1_no_crash PROPERTIES TIMEOUT 30)

# Replay serialized face-mesh failures without rerunning a complete BRep
# tessellation.  mesh.cpp emits this format at the exact repair failure site,
# making it useful both for diagnosis and for compact regression fixtures.
brlcad_addexec(test_brep_cdt_mesh brep_cdt_mesh.cpp "libbrep" TEST)
brlcad_add_test(
  NAME brep_cdt_mesh_single_bad_edge
  COMMAND test_brep_cdt_mesh "${CMAKE_CURRENT_SOURCE_DIR}/single_bad_edge.cdtmesh"
)

# A cyclic rotation of the same co-circular boundary must not change the CDT
# diagonal merely because its cpolyedge allocations have different addresses.
brlcad_addexec(test_brep_cdt_boundary_start brep_cdt_boundary_start.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_boundary_start COMMAND test_brep_cdt_boundary_start)

# An interior sample exactly on a constrained face boundary must not be passed
# to the triangulator a second time as a Steiner point.
brlcad_addexec(test_brep_cdt_boundary_steiner brep_cdt_boundary_steiner.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_boundary_steiner COMMAND test_brep_cdt_boundary_steiner)

# A closed source solid may not retain redundant open local-remesh islands
# after either retained-solid or same-source-face coverage validation.
brlcad_addexec(test_brep_cdt_spurious_components brep_cdt_spurious_components.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_spurious_components COMMAND test_brep_cdt_spurious_components)

brlcad_addexec(test_brep_cdt_local_defects brep_cdt_local_defects.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_local_defects COMMAND test_brep_cdt_local_defects)

brlcad_addexec(test_brep_cdt_healing brep_cdt_healing.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_healing COMMAND test_brep_cdt_healing)

# Explicit B-Rep edges wholly below min(BN_TOL_DIST, absmin) may be welded in
# derived mesh state.  Guard whole-curve selection, bounded non-transitive
# components, deterministic endpoint choice, adjacent edge propagation, and
# seam-copy provenance during zero-length constraint removal.
brlcad_addexec(test_brep_cdt_subtolerance brep_cdt_subtolerance.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_subtolerance COMMAND test_brep_cdt_subtolerance)

# A conical pole is one chart vertex even though its native rectangular UV
# domain has two periodic seam copies.  Exercise scale, rotation, translation,
# explicit pole identity, and chart/native evaluation round trips.
brlcad_addexec(test_brep_cdt_cone_chart brep_cdt_cone_chart.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_cone_chart COMMAND test_brep_cdt_cone_chart)

# A closed two-pole surface is a diamond after cutting its periodic seam.
# Verify explicit pole identities, chart/native round trips, constrained
# meridians, scale independence, six-direction geometric coverage, and
# certified solid assembly.
brlcad_addexec(test_brep_cdt_polar_chart brep_cdt_polar_chart.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_polar_chart COMMAND test_brep_cdt_polar_chart)

# A trimmed cylinder is unwrapped in physical angle-height coordinates.  Keep
# full periodic seams explicit despite fit offsets and equivalent-bound
# p-curves.
brlcad_addexec(test_brep_cdt_cylinder_chart brep_cdt_cylinder_chart.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_cdt_cylinder_chart COMMAND test_brep_cdt_cylinder_chart)

# Conversion-quality tessellation is a fail-closed transaction: status and
# diagnostics must describe every attempt, tolerance changes invalidate all
# derived state, and rebuilding the same input must produce identical arrays.
brlcad_addexec(test_brep_cdt_contract brep_cdt_contract.cpp "libbrep;librt" TEST)
brlcad_add_test(
  NAME brep_cdt_contract
  COMMAND test_brep_cdt_contract "${CMAKE_CURRENT_SOURCE_DIR}/cyl_hole.g" CUBE.s
)

# Fast display tessellation must be deterministic across worker counts, leave
# its const BRep input untouched, and stop cleanly at configured limits.
brlcad_addexec(test_brep_fast brep_fast.cpp "libbrep;librt" TEST)
brlcad_add_test(
  NAME brep_fast
  COMMAND test_brep_fast "${CMAKE_CURRENT_SOURCE_DIR}/cyl_hole.g" CUBE.s
)
brlcad_add_test(
  NAME brep_fast_closed
  COMMAND test_brep_fast "${CMAKE_CURRENT_SOURCE_DIR}/cyl_hole.g" PEG_ROT_LONG.s
)

# Display curve trees stop once a trim feature is below model-space display
# tolerance, before their independent working-memory guard is needed.
brlcad_addexec(test_brep_curvetree_display brep_curvetree_display.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_curvetree_display COMMAND test_brep_curvetree_display)

# Fast display recovery must retain thin nonlinear loops, reconstruct periodic
# caps and bands, repair malformed pcurves from their 3-D edges, and report
# exact zero-area imports separately from triangulation failures.
brlcad_addexec(test_brep_fast_repair brep_fast_repair.cpp "libbrep" TEST)
brlcad_add_test(NAME brep_fast_repair COMMAND test_brep_fast_repair)

# Standalone CDT hole-triangulation tests: verify that bg_nested_poly_triangulate
# produces no triangles whose centroid falls inside a hole polygon, for two
# faces from the NIST MBE PMI sample that are known to have multiple holes.
brlcad_addexec(test_brep_cdt_hole_face17 brep_cdt_hole_face17.c "libbg;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME brep_cdt_hole_face17 COMMAND test_brep_cdt_hole_face17)

brlcad_addexec(test_brep_cdt_hole_face27 brep_cdt_hole_face27.c "libbg;libbu;${M_LIBRARY}" TEST)
brlcad_add_test(NAME brep_cdt_hole_face27 COMMAND test_brep_cdt_hole_face27)

brlcad_addexec(test_brep_cdt_invalid brep_cdt_invalid.cpp "libbrep;libbu" TEST)
brlcad_add_test(NAME brep_cdt_invalid COMMAND test_brep_cdt_invalid)
if(BUILD_TESTING AND TARGET test_brep_cdt_invalid)
  set_tests_properties(brep_cdt_invalid PROPERTIES TIMEOUT 10)
endif()

brlcad_addexec(test_pullback_context pullback_context.cpp "libbrep;Threads::Threads" TEST)
brlcad_add_test(NAME brep_pullback_context COMMAND test_pullback_context)

cmakefiles(
  CMakeLists.txt
  brep_boolean_box_cut.cpp
  brep_boolean_armor2.asc
  brep_boolean_periodic_inner_loop.asc
  brep_boolean_periodic_chain.asc
  brep_boolean_coplanar_overlap.cpp
  brep_boolean_coplanar_sliver.cpp
  brep_corpus.cpp
  brep_corpus_thin_partition.cmake
  brep_boolean_endpoint_pair.cpp
  brep_boolean_failures.cpp
  brep_boolean_loop_degenerate.cpp
  brep_boolean_manifold.cpp
  brep_boolean_manifold_provenance.cpp
  brep_boolean_resource_limit.cpp
  brep_boolean_shared_edge.cpp
  brep_boolean_tolerances.cpp
  brep_boolean_tree_retry.cmake
  brep_boolean_xor.cpp
  brep_boolean_vertex_hits.cpp
  brep_comb_conversion_failures.cpp
  brep_curve_equivalence.cpp
  brep_curve_overlap_events.cpp
  brep_intersection_resource_limit.cpp
  brep_surface_domain.cpp
  brep_shape_recognition_sphere.cpp
  brep_ssi_events.cpp
  ayam_hyperbolid.3dm
  assembly.cpp
  brep_point_inside.cpp
  brep_boolean.cmake
  brep_boolean_halfspace.cmake
  brep_cdt_invalid.cpp
  brep_cdt_hole_face17.c
  brep_cdt_hole_face27.c
  brep_cdt_boundary_start.cpp
  brep_cdt_boundary_steiner.cpp
  brep_cdt_contract.cpp
  brep_cdt_cone_chart.cpp
  brep_cdt_cylinder_chart.cpp
  brep_cdt_polar_chart.cpp
  brep_cdt_local_defects.cpp
  brep_cdt_healing.cpp
  brep_cdt_spurious_components.cpp
  brep_cdt_subtolerance.cpp
  brep_fast.cpp
  brep_fast_budget.cpp
  brep_curvetree_display.cpp
  brep_fast_repair.cpp
  brep_cdt_mesh.cpp
  cyl_hole.g
  cyls_holes.g
  freecad_surface.stp
  nurbs_csg.g
  plate_mode_samples.g
  plate_mode_samples_creo.stp
  plate_mode_samples_rhino.stp
  pullback_context.cpp
  single_bad_edge.cdtmesh
  test_curve_intersect.cpp
  test_point_intersect.cpp
  vsp.stp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
