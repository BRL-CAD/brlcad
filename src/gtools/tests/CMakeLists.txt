brlcad_addexec(bigdb bigdb.c "libged;libwdb;librt;libbu" TEST_USESDATA)

brlcad_add_test(
  NAME brep_audit_surface_trees_all
  COMMAND
    brep-audit --surface-trees-all --surface-tree-depth=0
    ${CMAKE_SOURCE_DIR}/regress/nurbs/sph.g
)

brlcad_add_test(
  NAME brep_audit_prep_repeat
  COMMAND
    brep-audit --prep-only --prep-repeat=3 --surface-tree-depth=3
    ${CMAKE_SOURCE_DIR}/regress/nurbs/sph.g sph.brep
)

brlcad_add_test(
  NAME brep_audit_status2_inventory
  COMMAND
    ${CMAKE_COMMAND}
    -DBREP_AUDIT=$<TARGET_FILE:brep-audit>
    -DPOSITIVE_DATABASE=${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
    -DNEGATIVE_DATABASE=${CMAKE_SOURCE_DIR}/regress/nurbs/sph.g
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test_brep_status2_inventory.cmake
)

brlcad_add_test(
  NAME brep_audit_cached_prep
  COMMAND
    ${CMAKE_COMMAND}
    -DBREP_AUDIT=$<TARGET_FILE:brep-audit>
    -DDATABASE=${CMAKE_SOURCE_DIR}/regress/nurbs/sph.g
    -DOBJECT=sph.brep
    -DCACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/brep-audit-cache-prep
    -P ${CMAKE_CURRENT_SOURCE_DIR}/test_brep_cached_prep.cmake
)
set_property(
  TEST brep_audit_prep_repeat
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"peak_rss_bytes\":[1-9][0-9]*.*\"resident_before_bytes\":[1-9][0-9]*.*\"resident_prepared_bytes\":[1-9][0-9]*.*\"resident_after_free_bytes\":[1-9][0-9]*.*\"requested_adaptive_depth\":3,\"recorded_adaptive_depth\":3.*\"routing_cell_span_packs\":0.*\"routing_cell_span_pack_budget_bytes\":0,\"routing_cell_span_pack_used_bytes\":0.*\"cache_payload_serialized\":true,\"cache_variant\":[1-9][0-9]*,\"cache_payload_bytes\":[1-9][0-9]*.*\"cache_payload_compressed\":true,\"cache_payload_compressed_bytes\":[1-9][0-9]*.*\"retained_bytes\":[1-9][0-9]*.*\"retained_brep_bytes\":[1-9][0-9]*.*\"retained_surface_tree_bytes\":[1-9][0-9]*.*\"retained_routing_bytes\":[1-9][0-9]*.*\"retained_surface_tree_unused_capacity_bytes\":[0-9]+.*\"retained_routing_unused_capacity_bytes\":[0-9]+.*\"retained_routing_pack_unused_capacity_bytes\":0"
)

brlcad_add_test(
  NAME brep_audit_ray_depth_compare
  COMMAND
    brep-audit --ray-depth-compare --ray-count=4 --feature-ray-count=3
    ${CMAKE_SOURCE_DIR}/regress/nurbs/sph.g sph.brep
)

brlcad_add_test(
  NAME brep_audit_exact_ray
  COMMAND
    brep-audit --ray-depth-compare --ray=0,0,-10,0,0,1
    ${CMAKE_SOURCE_DIR}/regress/nurbs/sph.g sph.brep
)
set_property(
  TEST brep_audit_exact_ray
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"sample_kind\":\"exact\".*\"origin\":.0,0,-10..*\"direction\":.0,0,1..*\"in_point\":.*\"in_normal\":.*\"in_uv\":"
)

brlcad_add_test(
  NAME brep_audit_ray_depth_compare_multiface
  COMMAND
    brep-audit --ray-depth-compare --ray-count=4 --feature-ray-count=3
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
    recursive_child_linkage_complex_subtractions.s
)

brlcad_add_test(
  NAME brep_audit_legacy_cleanup_telemetry
  COMMAND
    brep-audit --ray-depth-compare --ray-count=2
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
    recursive_child_linkage_complex_subtractions.s
)
set_property(
  TEST brep_audit_legacy_cleanup_telemetry
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"legacy_capacity_fallbacks\":\\{\"leaf_workspace\":[0-9]+,\"hit_workspace\":[0-9]+\\},\"legacy_cleanup\":\\{\"coincident_grazing\":\\{\"rays\":[0-9]+,\"events\":[0-9]+\\}.*\"open_odd_endpoint\":\\{\"rays\":[0-9]+,\"events\":[0-9]+\\}\\},\"legacy_cleanup_action_mismatch_rays\":0"
)

brlcad_add_test(
  NAME brep_audit_seeded_verify_multiface_axes
  COMMAND
    brep-audit --ray-depth-compare --seeded-verify --ray-count=0
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
    recursive_child_linkage_complex_subtractions.s
)
set_property(
  TEST brep_audit_seeded_verify_multiface_axes
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"seeded_verifier\":\\{\"attempts\":6,\"complete\":6.*\"degree\":\\{\"attempts\":54,\"complete\":52.*\"count_mismatches\":0.*\"complement_disagreements\":0"
)

brlcad_add_test(
  NAME brep_audit_seeded_routing_multiface
  COMMAND
    brep-audit --ray-depth-compare --seeded-verify --ray-count=64
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
    recursive_child_linkage_complex_subtractions.s
)
set_property(
  TEST brep_audit_seeded_routing_multiface
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"routing_metadata\":\\{\"capability_mask\":7,\"risk_mask\":369,\"seeded_faces\":63,\"seeded_spans\":75,\"unqualified_faces\":63,\"unsupported_faces\":0,\"reparameterized_faces\":0,\"singular_spans\":0,\"periodic_faces\":4,\"inner_trim_faces\":11,\"boundary_faces\":63,\"nonmanifold_faces\":0,\"seam_faces\":2\\}.*\"routing_shadow\":\\{\"lanes\":\\[\\{\"name\":\"none\",\"rays\":134,\"verifier_complete\":128,\"prepared_selected\":88,\"root_mismatch\":0\\},\\{\"name\":\"seeded_ordinary\",\"rays\":0,\"verifier_complete\":0,\"prepared_selected\":0,\"root_mismatch\":0\\},\\{\"name\":\"complex_prepared\",\"rays\":0,\"verifier_complete\":0,\"prepared_selected\":0,\"root_mismatch\":0\\}\\].*\"decision\":\\{\"base\":\"legacy\",\"publication_authority\":\"legacy\",\"candidate_cells\":202,\"hazard_roots\":4,\"contact_roots\":0,\"legacy_transaction_incomplete\":0,\"selective_refinement\":\\{\"attempted\":0,\"complete\":0,\"max_level\":0,\"hits\":0,\"reversal_mismatches\":0,\"elapsed_ns\":0\\},\"cell_lookup_failures\":0.*\"predicted_verify_failed\":0"
)

brlcad_add_test(
  NAME brep_audit_seeded_routing_feature_corpus
  COMMAND
    brep-audit --ray-depth-compare --seeded-verify --ray-count=0
    --feature-ray-count=24
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
    recursive_child_linkage_complex_subtractions.s
)
set_property(
  TEST brep_audit_seeded_routing_feature_corpus
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"routing_shadow\":\\{\"lanes\":\\[\\{\"name\":\"none\",\"rays\":36,\"verifier_complete\":18,\"prepared_selected\":10,\"root_mismatch\":0\\},\\{\"name\":\"seeded_ordinary\",\"rays\":0,\"verifier_complete\":0,\"prepared_selected\":0,\"root_mismatch\":0\\},\\{\"name\":\"complex_prepared\",\"rays\":16,\"verifier_complete\":0,\"prepared_selected\":0,\"root_mismatch\":0\\}\\].*\"name\":\"directional_grazing\",\"rays\":16.*\"decision\":\\{\"base\":\"legacy\",\"publication_authority\":\"legacy\",\"candidate_cells\":502,\"hazard_roots\":98,\"contact_roots\":88,\"legacy_transaction_incomplete\":0,\"selective_refinement\":\\{\"attempted\":0,\"complete\":0,\"max_level\":0,\"hits\":0,\"reversal_mismatches\":0,\"elapsed_ns\":0\\},\"cell_lookup_failures\":0.*\"predicted_verify_failed\":0"
)

# The missing-root probe is intentionally shadow-only.  Masking the observed
# roots supplies positive controls without changing either solver result.  The
# correction-cell BVH limits the whole-face audit oracle to rays that first
# intersect an independently selected risk cell.  The same report keeps
# independently empty and unresolved populations separate so a broad
# possible-root signal cannot be mistaken for publication authority.  The
# status-1 cell/span degree pass must completely prove all four real empty
# associations and retain positive counterfactual roots as evidence only.
brlcad_add_test(
  NAME brep_audit_missing_root_probe
  COMMAND
    brep-audit --ray-depth-compare --seeded-verify --ray-count=32
    --feature-ray-count=12
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g multi_cylinder_01.s
)
set_property(
  TEST brep_audit_missing_root_probe
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"missing_root_probe\":\\{\"empty_correction_cells\":4,\"candidate_spans\":4,\"retained_spans\":4,\"restricted_spans\":2,\"inconclusive_spans\":0,\"workspace_overflow\":0,\"signal_rays\":4,\"independent_hit_rays\":0,\"independent_empty_rays\":4,\"unresolved_rays\":0,\"prepared_candidates\":\\{\"faces\":16,\"spans\":10,\"retained_spans\":8,\"prepared_only_spans\":6,\"prepared_only_retained_spans\":4,\"inconclusive_spans\":0,\"candidate_failures\":0,\"signal_rays\":4,\"independent_hit_rays\":0,\"independent_empty_rays\":4,\"unresolved_rays\":0\\},\"counterfactual\":\\{\"attempts\":36,\"signal_rays\":34,\"unsignaled_rays\":2,\"true_root_controls\":8,\"true_root_signals\":8,\"true_root_missed\":0,\"true_root_prepared_signals\":8,\"true_root_prepared_missed\":0,\"true_root_prepared_recovered\":0,\"empty_controls\":6,\"empty_signals\":4,\"empty_rejected\":2,\"empty_prepared_signals\":4,\"empty_prepared_rejected\":2,\"unresolved_controls\":22,\"candidate_spans\":78,\"retained_spans\":66,\"restricted_spans\":64,\"inconclusive_spans\":0,\"workspace_overflow\":0,\"prepared_candidates\":\\{\"faces\":158,\"spans\":138,\"retained_spans\":106,\"prepared_only_spans\":60,\"prepared_only_retained_spans\":40,\"inconclusive_spans\":0,\"candidate_failures\":0,\"signal_rays\":34,\"unsignaled_rays\":2\\},\"certificate\":\\{\"associations\":116,\"incomplete\":46,\"root_free\":28,\"root_positive\":42,\"association_regular_root_count\":42,\"observed_roots\":0,\"deficit_cells\":42,\"deficit_roots\":42,\"globally_unique\":false,\"publication_authority\":false,\"accounting_valid\":true,\"cached_bbox_exclusions\":0,\"hull_exclusions\":26,\"subdivision_attempts\":90,\"subdivision_boxes\":224,\"subdivision_hull_exclusions\":44,\"subdivision_exclusions\":0,\"degree_attempts\":90,\"degree_complete\":44.*\"reparameterization\",\"associations\":0.*\"determinant\",\"associations\":18.*\"boundary\",\"associations\":28.*\"complete_rays\":22,\"root_free_rays\":2,\"root_positive_rays\":26,\"incomplete_rays\":14.*\"certificate\":\\{\"associations\":4,\"incomplete\":0,\"root_free\":4,\"root_positive\":0.*\"accounting_valid\":true,\"cached_bbox_exclusions\":0,\"hull_exclusions\":2,\"subdivision_attempts\":2,\"subdivision_boxes\":2,\"subdivision_hull_exclusions\":2,\"subdivision_exclusions\":2,\"degree_attempts\":0,\"degree_complete\":0"
)

# The whole-face prepared audit deliberately observes two ordinary roots that
# do not lie in the independently intersected correction cell.  Preserve that
# distinction as a guard against accidentally turning unrelated same-ray roots
# into a production correction-cell recall requirement.  Both probes remain
# evidence-only and authorize no broad work.
brlcad_add_test(
  NAME brep_audit_missing_root_prepared_candidates
  COMMAND
    brep-audit --ray-depth-compare --seeded-verify --ray-count=32
    --feature-ray-count=12
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g 2cyl_04.s
)
set_property(
  TEST brep_audit_missing_root_prepared_candidates
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"counterfactual\":\\{\"attempts\":40,\"signal_rays\":38,\"unsignaled_rays\":2,\"true_root_controls\":8,\"true_root_signals\":6,\"true_root_missed\":2,\"true_root_prepared_signals\":8,\"true_root_prepared_missed\":0,\"true_root_prepared_recovered\":2.*\"prepared_candidates\":\\{\"faces\":144,\"spans\":224,\"retained_spans\":176,\"prepared_only_spans\":114,\"prepared_only_retained_spans\":82,\"inconclusive_spans\":0,\"candidate_failures\":0,\"signal_rays\":40,\"unsignaled_rays\":0\\},\"certificate\":\\{\"associations\":184,\"incomplete\":36,\"root_free\":95,\"root_positive\":53,\"association_regular_root_count\":53,\"observed_roots\":0,\"deficit_cells\":53,\"deficit_roots\":53,\"globally_unique\":false,\"publication_authority\":false,\"accounting_valid\":true,\"cached_bbox_exclusions\":0,\"hull_exclusions\":64,\"subdivision_attempts\":120,\"subdivision_boxes\":266,\"subdivision_hull_exclusions\":70,\"subdivision_exclusions\":24,\"degree_attempts\":96,\"degree_complete\":60.*\"reparameterization\",\"associations\":0.*\"determinant\",\"associations\":24.*\"boundary\",\"associations\":12.*\"complete_rays\":25,\"root_free_rays\":4,\"root_positive_rays\":28,\"incomplete_rays\":15.*\"certificate\":\\{\"associations\":8,\"incomplete\":0,\"root_free\":8,\"root_positive\":0.*\"accounting_valid\":true,\"cached_bbox_exclusions\":0,\"hull_exclusions\":6,\"subdivision_attempts\":2,\"subdivision_boxes\":2,\"subdivision_hull_exclusions\":2,\"subdivision_exclusions\":2,\"degree_attempts\":0,\"degree_complete\":0"
)

# A nonempty face hierarchy must also exist for a one-face BREP.  Its mapped
# status-2 intervals conservatively reduce the selected cell/span associations.
# Sphere cells incident on either collapsed profile endpoint remain explicitly
# singular and must fail closed before attempting degree proof.
brlcad_add_test(
  NAME brep_audit_missing_root_single_face_candidates
  COMMAND
    brep-audit --ray-depth-compare --seeded-verify --ray-count=32
    --feature-ray-count=12
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g sphere_001.s
)
set_property(
  TEST brep_audit_missing_root_single_face_candidates
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"counterfactual\":\\{\"attempts\":14,\"signal_rays\":14,\"unsignaled_rays\":0,\"true_root_controls\":8,\"true_root_signals\":8,\"true_root_missed\":0,\"true_root_prepared_signals\":8,\"true_root_prepared_missed\":0,\"true_root_prepared_recovered\":0.*\"candidate_spans\":64,\"retained_spans\":50,\"restricted_spans\":50.*\"prepared_candidates\":\\{\"faces\":14,\"spans\":76,\"retained_spans\":66,\"prepared_only_spans\":20,\"prepared_only_retained_spans\":16,\"inconclusive_spans\":0,\"candidate_failures\":0,\"signal_rays\":14,\"unsignaled_rays\":0\\},\"certificate\":\\{\"associations\":1320,\"incomplete\":1320,\"root_free\":0,\"root_positive\":0,\"association_regular_root_count\":0,\"observed_roots\":0,\"deficit_cells\":0,\"deficit_roots\":0,\"globally_unique\":false,\"publication_authority\":false,\"accounting_valid\":true,\"cached_bbox_exclusions\":0,\"hull_exclusions\":0,\"subdivision_attempts\":0,\"subdivision_boxes\":0,\"subdivision_hull_exclusions\":0,\"subdivision_exclusions\":0,\"degree_attempts\":0,\"degree_complete\":0.*\"singular\",\"associations\":1320.*\"incomplete_rays\":14"
)

# Remove collected raw legacy roots before the retained transaction is formed.
# This deterministically models a real legacy-missing/prepared-hit fault.  The
# audit requires direct mode to expose the injected miss, a complete positive
# certificate to recover an independently agreeing prepared/normal-legacy
# oracle, and every incomplete publication attempt to retain the entire
# injected base transaction without mixing sources.
brlcad_add_test(
  NAME brep_audit_missing_root_positive_transaction
  COMMAND
    brep-audit --ray-depth-compare --seeded-verify --routing-truth
    --routing-missing-root-injection --ray-count=32 --feature-ray-count=12
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g multi_cylinder_01.s
)
set_property(
  TEST brep_audit_missing_root_positive_transaction
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"missing_root_injection\":\\{\"valid\":true,\"rays\":[1-9][0-9]*,\"fault_injected\":[1-9][0-9]*,\"suppressed_hit_rays\":[1-9][0-9]*,\"suppressed_raw_hits\":[1-9][0-9]*,\"suppressed_route_roots\":[1-9][0-9]*,\"direct_empty_results\":[1-9][0-9]*,\"fault_exposed_rays\":[1-9][0-9]*,\"triggered_faults\":[1-9][0-9]*,\"triggered\":[1-9][0-9]*,\"optional_attempts\":[1-9][0-9]*,\"general_prepared_attempts\":[1-9][0-9]*,\"optional_complete\":[1-9][0-9]*.*\"complete_oracle_agreements\":[1-9][0-9]*,\"recoverable_faults\":[1-9][0-9]*,\"recovered_faults\":[1-9][0-9]*.*\"decision_mismatches\":0,\"transaction_mismatches\":0"
)

brlcad_add_test(
  NAME brep_audit_routing_truth_correction
  COMMAND
    brep-audit --ray-depth-compare --routing-truth --routing-publication
    --ray-count=32
    --feature-ray-count=24 --ray-pair=55
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g sphere_006.s
)
set_property(
  TEST brep_audit_routing_truth_correction
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"authoritative_correction_rays\":2,\"authoritative_correction_pairs\":1,\"trace_only_differing_rays\":0,\"verifier_complete_correction_rays\":0,\"oracle\":\\{\"class\":\"certified_boundary_contact_miss\",\"completed_rays\":2,\"completed_pairs\":1,\"unresolved_correction_rays\":0\\}.*\"name\":\"none\",\"correction_rays\":0.*\"name\":\"seeded_ordinary\",\"correction_rays\":0.*\"name\":\"complex_prepared\",\"correction_rays\":2.*\"publication\":\\{\"valid\":true,\"rays\":2,\"invalid_rays\":0,\"optional_attempts\":2,\"optional_complete\":2,\"eligible_publications\":2,\"prepared_matches\":2,\"legacy_fallback_matches\":0,\"published_correction_rays\":2,\"mismatches\":0.*\"intervals\":\\[\\{\"in\":3\\.944"
)

brlcad_add_test(
  NAME brep_audit_routing_publication_fallback
  COMMAND
    brep-audit --ray-depth-compare --routing-truth --routing-publication
    --ray-count=32 --feature-ray-count=24 --ray-pair=42
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g sphere_006.s
)
set_property(
  TEST brep_audit_routing_publication_fallback
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"authoritative_correction_rays\":0.*\"publication\":\\{\"valid\":true,\"rays\":2,\"invalid_rays\":0,\"optional_attempts\":2,\"optional_complete\":0,\"eligible_publications\":0,\"prepared_matches\":0,\"legacy_fallback_matches\":2,\"published_correction_rays\":0,\"mismatches\":0"
)

# A separate negative replay must reject its unsupported event class at the
# first retained surface box, before the expensive isolation/event stages.
# The routing-truth comparison now runs both the selective-publication lane
# and its real no-publication control in both directions, so the process-wide
# profile records four rejections.  Keep this separate from the publication
# assertion above so both observable contracts are required rather than
# combined as alternative PASS regexes.
brlcad_add_test(
  NAME brep_audit_routing_selective_class_rejection
  COMMAND
    brep-audit --ray-depth-compare --routing-truth --routing-publication
    --ray-count=32 --feature-ray-count=24 --ray-pair=46
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g sphere_006.s
)
set_property(
  TEST brep_audit_routing_selective_class_rejection
  PROPERTY ENVIRONMENT "LIBRT_BREP_ROUTE_PROFILE=1"
)
set_property(
  TEST brep_audit_routing_selective_class_rejection
  PROPERTY PASS_REGULAR_EXPRESSION
           "BREP route optional fallback: reason=9 name=event-class rays=4"
)

# A bounded routing ledger cannot authorize prepared publication after root or
# candidate capacity overflow.  This replay has 64 stored roots plus overflow;
# it must retain legacy without launching an optional transaction whose result
# the publication gate would be required to reject.
brlcad_add_test(
  NAME brep_audit_routing_unsupported_no_work
  COMMAND
    brep-audit --ray-depth-compare --routing-truth --routing-publication
    --ray-count=256 --feature-ray-count=96
    --candidate-depth=8 --reference-depth=8 --ray-pair=348
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
    recursive_child_linkage_complex_subtractions.s
)
set_property(
  TEST brep_audit_routing_unsupported_no_work
  PROPERTY PASS_REGULAR_EXPRESSION
           "\"observation_strata\":\\[.*\"name\":\"unsupported\",\"rays\":2.*\"name\":\"none\",\"correction_rays\":0.*\"publication\":\\{\"valid\":true,\"rays\":2,\"invalid_rays\":0,\"optional_attempts\":0,\"optional_complete\":0,\"eligible_publications\":0,\"prepared_matches\":0,\"legacy_fallback_matches\":2,\"published_correction_rays\":0,\"mismatches\":0"
)

# Permanent selected-pair replays for every reversal mismatch found by the
# archived 64-random/24-feature and 256-random/96-feature routing sweeps, plus
# the two ordering cases exposed while closing that archive.  The two solvers
# share legacy cleanup, so require reversal invariance in both the depth
# comparison and the separately traced prepared/legacy results.
set(
  brep_reversal_regression_cases
  "broad|2cyl_04.s|64|24|85"
  "broad|example_nonsimple_island_04.s|64|24|79"
  "broad|multi_level_subtractions.s|64|24|83"
  "broad|recursive_child_linkage_complex_subtractions.s|64|24|77"
  "broad|recursive_child_linkage_complex_subtractions.s|64|24|79"
  "broad|recursive_child_linkage_parent_child_subtraction.s|64|24|82"
  "deep|2cyl_04.s|256|96|277"
  "deep|example_nonsimple_island_04.s|256|96|285"
  "deep|example_nonsimple_island_04.s|256|96|289"
  "deep|example_nonsimple_island_04.s|256|96|290"
  "deep|multi_level_subtractions.s|256|96|320"
  "deep|recursive_child_linkage_multiply_recursive.s|256|96|346"
  "deep|recursive_child_linkage_parent_child_subtraction.s|256|96|312"
  "deep|recursive_child_linkage_parent_child_subtraction.s|256|96|319"
  "deep|recursive_child_linkage_parent_child_subtraction.s|256|96|342"
  "closure|child_encompasses_parent.s|64|24|77"
  "closure|recursive_child_linkage_multiply_recursive.s|64|24|77"
)
foreach(brep_reversal_case IN LISTS brep_reversal_regression_cases)
  string(REPLACE "|" ";" brep_reversal_fields "${brep_reversal_case}")
  list(GET brep_reversal_fields 0 brep_reversal_set)
  list(GET brep_reversal_fields 1 brep_reversal_object)
  list(GET brep_reversal_fields 2 brep_reversal_random)
  list(GET brep_reversal_fields 3 brep_reversal_feature)
  list(GET brep_reversal_fields 4 brep_reversal_pair)
  string(REGEX REPLACE "\\.s$" "" brep_reversal_object_name
    "${brep_reversal_object}")
  brlcad_add_test(
    NAME brep_audit_reversal_${brep_reversal_set}_${brep_reversal_object_name}_${brep_reversal_pair}
    COMMAND
      brep-audit --ray-depth-compare --routing-truth
      --ray-count=${brep_reversal_random}
      --feature-ray-count=${brep_reversal_feature}
      --candidate-depth=8 --reference-depth=8
      --ray-pair=${brep_reversal_pair}
      ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
      ${brep_reversal_object}
  )
  set_property(
    TEST brep_audit_reversal_${brep_reversal_set}_${brep_reversal_object_name}_${brep_reversal_pair}
    PROPERTY PASS_REGULAR_EXPRESSION
             "\"candidate_reversal_mismatches\":0,\"reference_reversal_mismatches\":0.*\"prepared_reversal_mismatches\":0,\"legacy_reversal_mismatches\":0"
  )
endforeach()

set(
  brep_routing_truth_corpus
  "example_nonsimple_island_04.s|48"
  "multi_cylinder_01.s|44"
  "multi_cylinder_03.s|42"
  "multi_cylinder_04.s|41"
  "multi_level_subtractions.s|53"
  "multiple_toplevel_islands.s|50"
  "recursive_child_linkage_multiply_recursive.s|57"
  "sphere_008.s|55"
)
foreach(brep_routing_truth_case IN LISTS brep_routing_truth_corpus)
  string(REPLACE "|" ";" brep_routing_truth_fields "${brep_routing_truth_case}")
  list(GET brep_routing_truth_fields 0 brep_routing_truth_object)
  list(GET brep_routing_truth_fields 1 brep_routing_truth_pair)
  string(REGEX REPLACE "\\.s$" "" brep_routing_truth_name "${brep_routing_truth_object}")
  brlcad_add_test(
    NAME brep_audit_routing_truth_${brep_routing_truth_name}
    COMMAND
      brep-audit --ray-depth-compare --routing-truth --routing-publication
      --ray-count=32
      --feature-ray-count=24 --ray-pair=${brep_routing_truth_pair}
      ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
      ${brep_routing_truth_object}
  )
  set_property(
    TEST brep_audit_routing_truth_${brep_routing_truth_name}
    PROPERTY PASS_REGULAR_EXPRESSION
             "\"authoritative_correction_rays\":2,\"authoritative_correction_pairs\":1,\"trace_only_differing_rays\":0,\"verifier_complete_correction_rays\":0,\"oracle\":\\{\"class\":\"certified_boundary_contact_miss\",\"completed_rays\":2,\"completed_pairs\":1,\"unresolved_correction_rays\":0\\}.*\"name\":\"none\",\"correction_rays\":0.*\"name\":\"seeded_ordinary\",\"correction_rays\":0.*\"name\":\"complex_prepared\",\"correction_rays\":2.*\"publication\":\\{\"valid\":true,\"rays\":2,\"invalid_rays\":0,\"optional_attempts\":2,\"optional_complete\":2,\"eligible_publications\":2,\"prepared_matches\":2,\"legacy_fallback_matches\":0,\"published_correction_rays\":2,\"mismatches\":0"
  )
endforeach()

# The broader 256-random/96-feature sweep exposes a distinct certified
# boundary/contact correction on the representative 63-face object.  Keep the
# original sampling parameters: feature pair indices are defined by the full
# deterministic sample sequence.
brlcad_add_test(
  NAME brep_audit_routing_truth_recursive_complex_feature
  COMMAND
    brep-audit --ray-depth-compare --routing-truth --routing-publication
    --ray-count=256
    --feature-ray-count=96 --candidate-depth=8 --reference-depth=8
    --ray-pair=339
    ${CMAKE_SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g
    recursive_child_linkage_complex_subtractions.s
)
set_property(
  TEST brep_audit_routing_truth_recursive_complex_feature
  PROPERTY
    PASS_REGULAR_EXPRESSION
      "\"authoritative_correction_rays\":2,\"authoritative_correction_pairs\":1.*\"unresolved_correction_rays\":0.*\"correction_pair_indices\":\\[339\\].*\"prepared_miss_legacy_hit\",\"rays\":2.*\"all_hazard\",\"rays\":2.*\"missed_correction_rays\":0.*\"name\":\"complex_prepared\",\"correction_rays\":2.*\"publication\":\\{\"valid\":true,\"rays\":2,\"invalid_rays\":0,\"optional_attempts\":2,\"optional_complete\":2,\"eligible_publications\":2,\"prepared_matches\":2,\"legacy_fallback_matches\":0,\"published_correction_rays\":2,\"mismatches\":0"
)

brlcad_add_test(NAME slow-bigdb_1gb COMMAND bigdb 1)
if(TEST slow-bigdb_1gb)
  set_property(TEST slow-bigdb_1gb PROPERTY SKIP_RETURN_CODE 123)
endif(TEST slow-bigdb_1gb)

brlcad_add_test(NAME slow-bigdb_5gb COMMAND bigdb 5)
if(TEST slow-bigdb_5gb)
  set_property(TEST slow-bigdb_5gb PROPERTY SKIP_RETURN_CODE 123)
endif(TEST slow-bigdb_5gb)

cmakefiles(
  CMakeLists.txt
  test_brep_cached_prep.cmake
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
