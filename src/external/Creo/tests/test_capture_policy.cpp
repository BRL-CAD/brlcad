#include "common.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "bu/units.h"
#include "frontend/csg_capture.h"
#include "frontend/model_capture.h"


static int
expect(int condition, const char *message)
{
    if (condition)
        return 1;

    std::fprintf(stderr, "%s\n", message);
    return 0;
}


static int
near_equal(double left, double right)
{
    const double scale = std::fmax(1.0, std::fmax(std::fabs(left), std::fabs(right)));
    return std::fabs(left - right) <= 1.0e-12 * scale;
}


struct feature_change_fixture {
    int suppress_failure;
    int resume_failure;
    std::vector<int> suppress_calls;
    std::vector<int> resume_calls;
};


static int
fake_suppress_feature(void *context, int feature_id)
{
    struct feature_change_fixture *fixture =
        static_cast<struct feature_change_fixture *>(context);

    fixture->suppress_calls.push_back(feature_id);
    return feature_id != fixture->suppress_failure;
}


static int
fake_resume_feature(void *context, int feature_id)
{
    struct feature_change_fixture *fixture =
        static_cast<struct feature_change_fixture *>(context);

    fixture->resume_calls.push_back(feature_id);
    return feature_id != fixture->resume_failure;
}


static int
test_suppression_transaction()
{
    const std::vector<int> feature_ids = {10, 20, 30};
    std::vector<int> suppressed;
    struct feature_change_fixture fixture = {};

    if (!expect(creo_brl_suppress_features_transactionally(
                    std::vector<int>(), fake_suppress_feature, fake_resume_feature,
                    &fixture, &suppressed) == CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED,
                "empty suppression transaction was not a no-op") ||
        !expect(fixture.suppress_calls.empty() && fixture.resume_calls.empty() && suppressed.empty(),
                "empty suppression transaction invoked a feature operation"))
        return 0;

    fixture = {};
    fixture.suppress_failure = 20;
    if (!expect(creo_brl_suppress_features_transactionally(
                    feature_ids, fake_suppress_feature, fake_resume_feature,
                    &fixture, &suppressed) == CREO_BRL_FEATURE_SUPPRESSION_NOT_APPLIED,
                "partial suppression did not roll back cleanly") ||
        !expect(fixture.suppress_calls == std::vector<int>({10, 20}),
                "partial suppression attempted unexpected feature IDs") ||
        !expect(fixture.resume_calls == std::vector<int>({10}) && suppressed.empty(),
                "partial suppression did not restore only successful changes"))
        return 0;

    fixture = {};
    if (!expect(creo_brl_suppress_features_transactionally(
                    feature_ids, fake_suppress_feature, fake_resume_feature,
                    &fixture, &suppressed) == CREO_BRL_FEATURE_SUPPRESSION_APPLIED,
                "complete suppression transaction failed") ||
        !expect(suppressed == feature_ids,
                "complete suppression did not report the successful feature IDs") ||
        !expect(creo_brl_restore_features(suppressed, fake_resume_feature, &fixture),
                "capture-success cleanup failed") ||
        !expect(fixture.resume_calls == feature_ids,
                "capture-success cleanup did not restore every feature"))
        return 0;

    fixture = {};
    suppressed.clear();
    if (!expect(creo_brl_suppress_features_transactionally(
                    feature_ids, fake_suppress_feature, fake_resume_feature,
                    &fixture, &suppressed) == CREO_BRL_FEATURE_SUPPRESSION_APPLIED,
                "capture-failure setup did not suppress features") ||
        !expect(creo_brl_restore_features(suppressed, fake_resume_feature, &fixture),
                "capture-failure cleanup failed"))
        return 0;

    fixture = {};
    suppressed.clear();
    if (!expect(creo_brl_suppress_features_transactionally(
                    feature_ids, fake_suppress_feature, fake_resume_feature,
                    &fixture, &suppressed) == CREO_BRL_FEATURE_SUPPRESSION_APPLIED,
                "restoration-failure setup did not suppress features"))
        return 0;
    fixture.resume_failure = 20;
    if (!expect(!creo_brl_restore_features(suppressed, fake_resume_feature, &fixture),
                "post-capture restoration failure was not reported") ||
        !expect(fixture.resume_calls == feature_ids,
                "restoration stopped before attempting every suppressed feature"))
        return 0;

    fixture = {};
    fixture.suppress_failure = 20;
    fixture.resume_failure = 10;
    return expect(creo_brl_suppress_features_transactionally(
                      feature_ids, fake_suppress_feature, fake_resume_feature,
                      &fixture, &suppressed) == CREO_BRL_FEATURE_SUPPRESSION_RESTORE_FAILED,
                  "restoration failure was not fatal") &&
           expect(suppressed == std::vector<int>({10}),
                  "restoration failure lost the still-modified feature ID");
}


static void
identity_matrix(ProMatrix matrix)
{
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column)
            matrix[row][column] = row == column ? 1.0 : 0.0;
    }
}


static int
test_component_unit_matrices()
{
    const double inch_to_mm = bu_units_conversion("in");
    const double mm_to_mm = bu_units_conversion("mm");
    ProMatrix creo_matrix;
    struct creo_brl_snapshot_matrix matrix = {};

    identity_matrix(creo_matrix);
    creo_matrix[3][0] = 2.0;
    creo_matrix[3][1] = -3.0;
    creo_matrix[3][2] = 4.0;
    if (!expect(creo_brl_component_matrix_to_mm(
                    creo_matrix, inch_to_mm, inch_to_mm, &matrix),
                "equal-unit component matrix conversion failed") ||
        !expect(near_equal(matrix.elements[0], 1.0) &&
                    near_equal(matrix.elements[5], 1.0) &&
                    near_equal(matrix.elements[10], 1.0),
                "equal-unit component matrix changed the linear transform") ||
        !expect(near_equal(matrix.elements[3], 2.0 * inch_to_mm) &&
                    near_equal(matrix.elements[7], -3.0 * inch_to_mm) &&
                    near_equal(matrix.elements[11], 4.0 * inch_to_mm),
                "equal-unit component translation was not converted to millimeters"))
        return 0;

    if (!expect(creo_brl_component_matrix_to_mm(
                    creo_matrix, mm_to_mm, inch_to_mm, &matrix),
                "inch-child/mm-parent component matrix conversion failed") ||
        !expect(near_equal(matrix.elements[0], mm_to_mm / inch_to_mm) &&
                    near_equal(matrix.elements[5], mm_to_mm / inch_to_mm) &&
                    near_equal(matrix.elements[10], mm_to_mm / inch_to_mm) &&
                    near_equal(matrix.elements[3], 2.0),
                "inch-child/mm-parent component scale was applied twice"))
        return 0;

    return expect(creo_brl_component_matrix_to_mm(
                      creo_matrix, inch_to_mm, mm_to_mm, &matrix),
                  "mm-child/inch-parent component matrix conversion failed") &&
           expect(near_equal(matrix.elements[0], inch_to_mm) &&
                      near_equal(matrix.elements[5], inch_to_mm) &&
                      near_equal(matrix.elements[10], inch_to_mm) &&
                      near_equal(matrix.elements[3], 2.0 * inch_to_mm),
                  "mm-child/inch-parent component scale was lost");
}


static int
expect_matrix(const struct creo_brl_snapshot_matrix& matrix,
              const double expected[CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT],
              const char *message)
{
    for (size_t index = 0; index < CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT; ++index) {
        if (!near_equal(matrix.elements[index], expected[index])) {
            std::fprintf(stderr, "%s at element %zu: expected %.15g, got %.15g\n",
                         message, index, expected[index], matrix.elements[index]);
            return 0;
        }
    }

    return 1;
}


static int
test_rotated_component_unit_matrices()
{
    const double inch_to_mm = bu_units_conversion("in");
    const double mm_to_mm = bu_units_conversion("mm");
    const double mm_parent_expected[CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT] = {
        0.0, -mm_to_mm / inch_to_mm, 0.0, 2.0,
        mm_to_mm / inch_to_mm, 0.0, 0.0, -3.0,
        0.0, 0.0, mm_to_mm / inch_to_mm, 4.0,
        0.0, 0.0, 0.0, 1.0
    };
    const double inch_parent_expected[CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT] = {
        0.0, -inch_to_mm, 0.0, 2.0 * inch_to_mm,
        inch_to_mm, 0.0, 0.0, -3.0 * inch_to_mm,
        0.0, 0.0, inch_to_mm, 4.0 * inch_to_mm,
        0.0, 0.0, 0.0, 1.0
    };
    ProMatrix creo_matrix;
    struct creo_brl_snapshot_matrix matrix = {};

    identity_matrix(creo_matrix);
    creo_matrix[0][0] = 0.0;
    creo_matrix[0][1] = 1.0;
    creo_matrix[1][0] = -1.0;
    creo_matrix[1][1] = 0.0;
    creo_matrix[3][0] = 2.0;
    creo_matrix[3][1] = -3.0;
    creo_matrix[3][2] = 4.0;

    if (!expect(creo_brl_component_matrix_to_mm(
                    creo_matrix, mm_to_mm, inch_to_mm, &matrix),
                "rotated inch-child/mm-parent matrix conversion failed") ||
        !expect_matrix(matrix, mm_parent_expected,
                       "rotated inch-child/mm-parent matrix was not transposed and scaled"))
        return 0;

    return expect(creo_brl_component_matrix_to_mm(
                      creo_matrix, inch_to_mm, mm_to_mm, &matrix),
                  "rotated mm-child/inch-parent matrix conversion failed") &&
           expect_matrix(matrix, inch_parent_expected,
                         "rotated mm-child/inch-parent matrix was not transposed and scaled");
}


static int
test_toolkit_derived_length_scale()
{
    static const double CUSTOM_UNIT_TO_MM = 37.25;
    struct snapshot_part_capture model_token = {};
    double scale = 0.0;

    creo_brl_shim_set_length_unit(L"custom_length", CUSTOM_UNIT_TO_MM);
    return expect(creo_brl_model_scale_to_mm(&model_token, &scale),
                  "Toolkit-convertible custom length unit was rejected") &&
           expect(near_equal(scale, CUSTOM_UNIT_TO_MM),
                  "Toolkit-derived custom length conversion was not preserved");
}


static int
test_pre_suppression_capture_failure_classification()
{
    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_settings_strings settings_strings;
    struct snapshot_part_capture capture = {};
    int model_token = 0;

    creo_brl_shim_set_model_name(L"classification_part");
    creo_brl_shim_set_length_unit(L"mm", bu_units_conversion("mm"));
    settings.flags = CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES;

    return expect(creo_brl_capture_part_definition(
                      &model_token, &settings, &settings_strings, &capture) ==
                      CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE,
                  "pre-suppression feature inspection failure was misreported as a restoration failure");
}


static int
test_feature_order_policy()
{
    const std::vector<int> base_hole = {
        CREO_BRL_CSG_SEQUENCE_OTHER,
        CREO_BRL_CSG_SEQUENCE_HOLE
    };
    const std::vector<int> hole_then_add = {
        CREO_BRL_CSG_SEQUENCE_OTHER,
        CREO_BRL_CSG_SEQUENCE_HOLE,
        CREO_BRL_CSG_SEQUENCE_MATERIAL_ADDING
    };
    const std::vector<int> multiple_holes = {
        CREO_BRL_CSG_SEQUENCE_HOLE,
        CREO_BRL_CSG_SEQUENCE_OTHER,
        CREO_BRL_CSG_SEQUENCE_HOLE,
        CREO_BRL_CSG_SEQUENCE_MATERIAL_ADDING,
        CREO_BRL_CSG_SEQUENCE_HOLE
    };

    return expect(creo_brl_csg_hole_is_eligible(base_hole, 1u),
                  "base-to-hole sequence rejected a safe replacement") &&
           expect(!creo_brl_csg_hole_is_eligible(hole_then_add, 1u),
                  "later additive material did not invalidate an earlier hole") &&
           expect(!creo_brl_csg_hole_is_eligible(multiple_holes, 0u) &&
                      !creo_brl_csg_hole_is_eligible(multiple_holes, 2u) &&
                      creo_brl_csg_hole_is_eligible(multiple_holes, 4u),
                  "multiple-hole ordering produced the wrong eligibility") &&
           expect(creo_brl_csg_feature_adds_material(PRO_FEAT_PROTRUSION) &&
                      creo_brl_csg_feature_adds_material(PRO_FEAT_UDF_THREAD) &&
                      !creo_brl_csg_feature_adds_material(PRO_FEAT_HOLE),
                  "material-adding feature classification changed") &&
           expect(creo_brl_csg_hole_action(1, 0, 1, 10.0, 5.0) ==
                      CREO_BRL_CSG_FEATURE_SUBTRACT,
                  "eligible hole was not selected for CSG subtraction") &&
           expect(creo_brl_csg_hole_action(1, 1, 1, 10.0, 5.0) ==
                      CREO_BRL_CSG_FEATURE_KEEP,
                  "facets-only mode retained CSG subtraction") &&
           expect(creo_brl_csg_hole_action(1, 1, 1, 2.0, 5.0) ==
                      CREO_BRL_CSG_FEATURE_SUPPRESS,
                  "facets-only mode disabled independent small-hole suppression") &&
           expect(creo_brl_csg_hole_action(0, 0, 1, 2.0, 5.0) ==
                      CREO_BRL_CSG_FEATURE_KEEP,
                  "disabled small-feature processing changed a hole");
}


struct attempt_fixture {
    size_t call_count;
};


static void
make_triangle_mesh(struct snapshot_mesh_capture *mesh, double marker, int is_solid)
{
    struct creo_brl_snapshot_vector vertex = {};
    struct creo_brl_snapshot_triangle triangle = {{0u, 1u, 2u}};

    mesh->vertices.clear();
    for (size_t index = 0; index < 3; ++index) {
        vertex.coordinates[0] = marker + static_cast<double>(index);
        mesh->vertices.push_back(vertex);
    }
    mesh->triangles.assign(1u, triangle);
    mesh->is_solid = is_solid;
}


static int
fake_tessellation_attempt(void *context,
                          const struct snapshot_tessellation_attempt *attempt,
                          size_t UNUSED(maximum_facet_count),
                          struct snapshot_mesh_capture *mesh)
{
    struct attempt_fixture *fixture = static_cast<struct attempt_fixture *>(context);
    const size_t call = fixture->call_count++;

    make_triangle_mesh(mesh, attempt->chord, call >= 2u);
    return call != 0u;
}


static int
test_tessellation_policy()
{
    static const size_t EXPECTED_ATTEMPTS = 20u;
    static const size_t EXPECTED_MAXIMUM_FACETS = 100000u;
    const double inch_to_mm = bu_units_conversion("in");
    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_tessellation_policy policy;
    struct snapshot_mesh_capture mesh = {};
    struct attempt_fixture fixture = {};

    settings.chord_mode = CREO_BRL_SNAPSHOT_CHORD_RELATIVE;
    settings.max_chord = 10.0;
    settings.min_angle = 0.2;
    if (!expect(creo_brl_build_tessellation_policy(&settings, inch_to_mm, 10.0 * inch_to_mm, &policy),
                "relative tessellation policy was rejected") ||
        !expect(policy.attempts.size() == EXPECTED_ATTEMPTS &&
                    policy.maximum_facet_count == EXPECTED_MAXIMUM_FACETS,
                "relative tessellation policy did not restore bounded retries") ||
        !expect(near_equal(policy.attempts.front().chord, 1.0) &&
                    near_equal(policy.attempts.back().chord, 0.1) &&
                    near_equal(policy.attempts.front().angle, 0.2) &&
                    near_equal(policy.attempts.back().angle, 0.3),
                "relative tessellation policy produced the wrong coarse-to-fine range"))
        return 0;

    if (!expect(creo_brl_run_tessellation_policy(
                    &policy, 1, fake_tessellation_attempt, &fixture, &mesh),
                "tessellation policy did not retry through a solid result") ||
        !expect(fixture.call_count == 3u && mesh.vertices.size() == 3u,
                "tessellation policy used the wrong stop condition") ||
        !expect(near_equal(mesh.vertices[0].coordinates[0], policy.attempts[2].chord),
                "failed tessellation output contaminated the accepted attempt"))
        return 0;

    fixture = {};
    if (!expect(creo_brl_run_tessellation_policy(
                    &policy, 0, fake_tessellation_attempt, &fixture, &mesh),
                "non-solid tessellation was rejected when solidity was disabled") ||
        !expect(fixture.call_count == 2u && !mesh.is_solid,
                "solidity policy used the wrong disabled stop condition"))
        return 0;

    settings.chord_mode = CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE;
    settings.max_chord = 0.1 * inch_to_mm;
    if (!expect(creo_brl_build_tessellation_policy(&settings, inch_to_mm, 10.0 * inch_to_mm, &policy),
                "absolute tessellation policy was rejected") ||
        !expect(near_equal(policy.attempts.front().chord, 0.1) &&
                    near_equal(policy.attempts.back().chord, 0.01),
                "absolute chord values were not converted to model units"))
        return 0;

    settings.max_chord = 0.0;
    return expect(!creo_brl_build_tessellation_policy(&settings, inch_to_mm, 10.0 * inch_to_mm, &policy),
                  "invalid tessellation settings produced attempts");
}


static int
test_tessellation_data_transaction()
{
    point_t vertices[4] = {
        {0.0, 0.0, 0.0},
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    };
    point_t normals[4] = {
        {0.0, 0.0, -1.0},
        {0.0, -1.0, 0.0},
        {-1.0, 0.0, 0.0},
        {1.0, 1.0, 1.0}
    };
    facet_set tetrahedron[4] = {
        {0, 2, 1},
        {0, 1, 3},
        {1, 2, 3},
        {2, 0, 3}
    };
    facet_set partially_invalid[2] = {
        {0, 2, 1},
        {0, 1, 99}
    };
    ProSurfaceTessellationData surface = {};
    struct snapshot_mesh_capture mesh = {};

    surface.n_vertices = 4;
    surface.vertices = vertices;
    surface.normals = normals;
    surface.n_facets = 4;
    surface.facets = tetrahedron;
    const double inch_to_mm = bu_units_conversion("in");
    if (!expect(creo_brl_capture_tessellation_data(
                    &surface, 1, inch_to_mm, 1, 100000u, &mesh),
                "valid tessellation data was rejected") ||
        !expect(mesh.vertices.size() == 4u && mesh.triangles.size() == 4u &&
                    mesh.normals.size() == 12u && mesh.normal_indices.size() == 12u &&
                    mesh.is_solid,
                "valid tessellation data was not captured as a closed mesh"))
        return 0;

    surface.n_facets = 2;
    surface.facets = partially_invalid;
    if (!expect(!creo_brl_capture_tessellation_data(
                    &surface, 1, inch_to_mm, 1, 100000u, &mesh),
                "late invalid facet was accepted") ||
        !expect(mesh.vertices.empty() && mesh.triangles.empty() &&
                    mesh.normals.empty() && mesh.normal_indices.empty(),
                "late invalid facet left partial mesh output"))
        return 0;

    surface.n_facets = 100001;
    surface.facets = NULL;
    if (!expect(!creo_brl_capture_tessellation_data(
                    &surface, 1, inch_to_mm, 0, 100000u, &mesh),
                "facet limit did not reject oversized output"))
        return 0;

    struct creo_brl_shim_model fallback_model = {};
    fallback_model.magic = CREO_BRL_SHIM_MODEL_MAGIC;
    fallback_model.name = L"fallback_part.prt";
    fallback_model.type = PRO_MDL_PART;
    fallback_model.length_unit_name = L"mm";
    fallback_model.length_unit_to_mm = 1.0;
    fallback_model.has_bbox = 1;
    fallback_model.bbox[1][0] = 10.0;
    fallback_model.bbox[1][1] = 20.0;
    fallback_model.bbox[1][2] = 30.0;

    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_settings_strings settings_strings;
    settings.chord_mode = CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE;
    settings.max_chord = 1.0;
    settings.min_angle = 0.5;
    settings.flags = CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES;

    struct snapshot_part_capture part = {};
    part.part.flags = CREO_BRL_SNAPSHOT_PART_HAS_MESH |
                      CREO_BRL_SNAPSHOT_PART_HAS_NORMALS;
    part.vertices.resize(3u);
    part.triangles.resize(1u);
    part.normals.resize(3u);
    part.normal_indices.resize(3u);
    return expect(creo_brl_capture_part_definition(
                      &fallback_model, &settings, &settings_strings, &part) ==
                      CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS,
                  "bounding-box fallback part capture failed") &&
           expect((part.part.flags & CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK) != 0 &&
                      (part.part.flags & (CREO_BRL_SNAPSHOT_PART_HAS_MESH |
                                          CREO_BRL_SNAPSHOT_PART_HAS_NORMALS)) == 0,
                  "bounding-box fallback retained mesh flags") &&
           expect(part.vertices.empty() && part.triangles.empty() &&
                      part.normals.empty() && part.normal_indices.empty(),
                  "bounding-box fallback retained partial mesh data");
}


static int
test_capture_uses_replay_vertex_tolerance()
{
    static const double NEARBY_VERTEX_OFFSET_MM = 0.0001;
    point_t vertices[12] = {
        {0.0, 0.0, 0.0},
        {0.0, 10.0, 0.0},
        {10.0, 0.0, 0.0},
        {NEARBY_VERTEX_OFFSET_MM, 0.0, 0.0},
        {10.0, NEARBY_VERTEX_OFFSET_MM, 0.0},
        {0.0, 0.0, 10.0},
        {10.0, 0.0, NEARBY_VERTEX_OFFSET_MM},
        {0.0, 10.0, NEARBY_VERTEX_OFFSET_MM},
        {NEARBY_VERTEX_OFFSET_MM, 0.0, 10.0},
        {0.0, 10.0, -NEARBY_VERTEX_OFFSET_MM},
        {0.0, NEARBY_VERTEX_OFFSET_MM, 0.0},
        {-NEARBY_VERTEX_OFFSET_MM, 0.0, 10.0}
    };
    facet_set facets[4] = {
        {0, 1, 2},
        {3, 4, 5},
        {6, 7, 8},
        {9, 10, 11}
    };
    ProSurfaceTessellationData surface = {};
    struct snapshot_mesh_capture mesh = {};

    surface.n_vertices = 12;
    surface.vertices = vertices;
    surface.n_facets = 4;
    surface.facets = facets;

    return expect(creo_brl_capture_tessellation_data(
                      &surface, 1, 1.0, 0, 100000u, &mesh),
                  "near-coincident closed tessellation was rejected") &&
           expect(mesh.vertices.size() == 4u && mesh.triangles.size() == 4u &&
                      mesh.is_solid,
                  "capture did not apply replay-equivalent mesh acceptance");
}


static int
test_capture_rejects_replay_collapsed_facet()
{
    static const double SHORT_EDGE_MM = 0.0001;
    point_t vertices[4] = {
        {0.0, 0.0, 0.0},
        {SHORT_EDGE_MM, 0.0, 0.0},
        {0.0, 10.0, 0.0},
        {0.0, 0.0, 10.0}
    };
    facet_set facets[4] = {
        {0, 2, 1},
        {0, 1, 3},
        {1, 2, 3},
        {2, 0, 3}
    };
    ProSurfaceTessellationData surface = {};
    struct snapshot_mesh_capture mesh = {};
    int captured = 0;

    surface.n_vertices = 4;
    surface.vertices = vertices;
    surface.n_facets = 4;
    surface.facets = facets;
    captured = creo_brl_capture_tessellation_data(
        &surface, 1, 1.0, 0, 100000u, &mesh);

    return expect(captured && mesh.triangles.size() == 2u,
                  "capture discarded valid facets while removing collapsed facets") &&
           expect(!mesh.is_solid,
                  "capture accepted a solid mesh that replay opens by collapsing a short facet");
}


static struct snapshot_part_capture
named_part(const char *name)
{
    struct snapshot_part_capture capture = {};

    capture.strings.model_name = name;
    return capture;
}


static int
test_scene_results_and_region_ids()
{
    struct snapshot_scene_capture scene = {};
    struct snapshot_part_capture first = named_part("first.prt");
    struct snapshot_part_capture reused = named_part("first.prt");
    struct snapshot_part_capture second = named_part("second.prt");
    uint32_t index = 0;

    scene.next_region_id = -12;
    if (!expect(creo_brl_commit_part_capture(&scene, &first, &index) ==
                    CREO_BRL_SCENE_PART_ADDED && index == 0u &&
                    scene.parts[0].part.region_id == 1,
                "non-positive initial region ID was not normalized") ||
        !expect(creo_brl_commit_part_capture(&scene, &reused, &index) ==
                    CREO_BRL_SCENE_PART_REUSED && index == 0u &&
                    scene.next_region_id == 2,
                "reused part consumed another region ID") ||
        !expect(creo_brl_commit_part_capture(&scene, &second, &index) ==
                    CREO_BRL_SCENE_PART_ADDED && index == 1u &&
                    scene.parts[1].part.region_id == 2,
                "unique parts did not receive consecutive region IDs"))
        return 0;

    scene = {};
    scene.next_region_id = (std::numeric_limits<int32_t>::max)();
    first = named_part("maximum.prt");
    reused = named_part("maximum.prt");
    second = named_part("overflow.prt");
    if (!expect(creo_brl_commit_part_capture(&scene, &first, &index) ==
                    CREO_BRL_SCENE_PART_ADDED &&
                    scene.parts[0].part.region_id == (std::numeric_limits<int32_t>::max)(),
                "maximum region ID was not allocated") ||
        !expect(creo_brl_commit_part_capture(&scene, &reused, &index) ==
                    CREO_BRL_SCENE_PART_REUSED,
                "region overflow check rejected a reused definition") ||
        !expect(creo_brl_commit_part_capture(&scene, &second, &index) ==
                    CREO_BRL_SCENE_PART_COMMIT_ERROR,
                "region ID overflow was not reported"))
        return 0;

    struct creo_brl_shim_model valid_part = {};
    valid_part.magic = CREO_BRL_SHIM_MODEL_MAGIC;
    valid_part.name = L"valid.prt";
    valid_part.type = PRO_MDL_PART;
    valid_part.length_unit_name = L"mm";
    valid_part.length_unit_to_mm = 1.0;
    valid_part.has_bbox = 1;
    valid_part.bbox[1][0] = 10.0;
    valid_part.bbox[1][1] = 10.0;
    valid_part.bbox[1][2] = 10.0;

    struct creo_brl_shim_model skipped_part = {};
    skipped_part.magic = CREO_BRL_SHIM_MODEL_MAGIC;
    skipped_part.name = L"skipped.prt";
    skipped_part.type = PRO_MDL_PART;
    skipped_part.length_unit_name = L"mm";
    skipped_part.length_unit_to_mm = 1.0;
    skipped_part.has_bbox = 0;

    struct creo_brl_shim_component empty_subassembly_children[1] = {};
    empty_subassembly_children[0].model = &skipped_part;
    identity_matrix(empty_subassembly_children[0].matrix);

    struct creo_brl_shim_model empty_subassembly = {};
    empty_subassembly.magic = CREO_BRL_SHIM_MODEL_MAGIC;
    empty_subassembly.name = L"empty_sub.asm";
    empty_subassembly.type = PRO_MDL_ASSEMBLY;
    empty_subassembly.length_unit_name = L"mm";
    empty_subassembly.length_unit_to_mm = 1.0;
    empty_subassembly.components = empty_subassembly_children;
    empty_subassembly.component_count = 1u;

    struct creo_brl_shim_component root_children[3] = {};
    root_children[0].model = &valid_part;
    identity_matrix(root_children[0].matrix);
    root_children[1].model = &skipped_part;
    identity_matrix(root_children[1].matrix);
    root_children[2].model = &empty_subassembly;
    identity_matrix(root_children[2].matrix);

    struct creo_brl_shim_model root_assembly = {};
    root_assembly.magic = CREO_BRL_SHIM_MODEL_MAGIC;
    root_assembly.name = L"root.asm";
    root_assembly.type = PRO_MDL_ASSEMBLY;
    root_assembly.length_unit_name = L"mm";
    root_assembly.length_unit_to_mm = 1.0;
    root_assembly.components = root_children;
    root_assembly.component_count = 3u;

    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_settings_strings settings_strings;
    settings.chord_mode = CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE;
    settings.max_chord = 1.0;
    settings.min_angle = 0.5;
    settings.flags = CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES;

    scene = {};
    if (!expect(creo_brl_frontend_capture_model_scene(
                    &root_assembly, PRO_MDL_ASSEMBLY, &settings, &settings_strings, &scene) ==
                    CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS,
                "assembly scene capture with skipped children failed") ||
        !expect(scene.assemblies.size() == 1u &&
                    scene.assemblies[0].members.size() == 1u &&
                    scene.parts.size() == 1u,
                "skipped child or empty subassembly was not pruned cleanly") ||
        !expect(creo_brl_frontend_capture_model_scene(
                    &empty_subassembly, PRO_MDL_ASSEMBLY, &settings, &settings_strings, &scene) ==
                    CREO_BRL_SNAPSHOT_CAPTURE_EMPTY_MODEL,
                "completely empty root assembly did not report EMPTY_MODEL"))
        return 0;

    struct creo_brl_shim_model invalid_part = {};
    invalid_part.magic = CREO_BRL_SHIM_MODEL_MAGIC;
    invalid_part.name = NULL;
    invalid_part.type = PRO_MDL_PART;
    invalid_part.length_unit_name = L"mm";
    invalid_part.length_unit_to_mm = 1.0;
    root_children[1].model = &invalid_part;
    scene = {};
    return expect(creo_brl_frontend_capture_model_scene(
                      &root_assembly, PRO_MDL_ASSEMBLY, &settings, &settings_strings, &scene) ==
                      CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE,
                  "fatal child capture error was omitted");
}


static int
test_assembly_metadata_capture()
{
    struct creo_brl_shim_model part_child = {};
    part_child.magic = CREO_BRL_SHIM_MODEL_MAGIC;
    part_child.name = L"child.prt";
    part_child.type = PRO_MDL_PART;
    part_child.length_unit_name = L"mm";
    part_child.length_unit_to_mm = 1.0;
    part_child.has_bbox = 1;
    part_child.bbox[1][0] = 5.0;
    part_child.bbox[1][1] = 5.0;
    part_child.bbox[1][2] = 5.0;

    struct creo_brl_shim_component children[1] = {};
    children[0].model = &part_child;
    identity_matrix(children[0].matrix);

    struct creo_brl_shim_model assembly_model = {};
    assembly_model.magic = CREO_BRL_SHIM_MODEL_MAGIC;
    assembly_model.name = L"meta.asm";
    assembly_model.type = PRO_MDL_ASSEMBLY;
    assembly_model.length_unit_name = L"mm";
    assembly_model.length_unit_to_mm = 1.0;
    assembly_model.has_mass_properties = 1;
    assembly_model.mass_properties.density = 7.5;
    assembly_model.mass_properties.mass = 125.0;
    assembly_model.mass_properties.volume = 16.5;
    assembly_model.mass_properties.surface_area = 48.0;
    assembly_model.components = children;
    assembly_model.component_count = 1u;

    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_settings_strings settings_strings;
    settings.chord_mode = CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE;
    settings.max_chord = 1.0;
    settings.min_angle = 0.5;
    settings.flags = CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES;

    struct snapshot_scene_capture scene = {};
    if (!expect(creo_brl_frontend_capture_model_scene(
                    &assembly_model, PRO_MDL_ASSEMBLY, &settings, &settings_strings, &scene) ==
                    CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS &&
                scene.assemblies.size() == 1u,
                "assembly metadata scene capture failed"))
        return 0;

    const struct snapshot_assembly_capture& capture = scene.assemblies[0];
    if (!expect((capture.assembly.flags &
                 CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES) != 0,
                "assembly mass-property flag was not captured") ||
        !expect(near_equal(capture.assembly.density, assembly_model.mass_properties.density) &&
                near_equal(capture.assembly.mass, assembly_model.mass_properties.mass) &&
                near_equal(capture.assembly.volume, assembly_model.mass_properties.volume) &&
                near_equal(capture.assembly.surface_area,
                           assembly_model.mass_properties.surface_area),
                "assembly mass-property values changed during capture"))
        return 0;

    assembly_model.has_mass_properties = 0;
    scene = {};
    return expect(creo_brl_frontend_capture_model_scene(
                      &assembly_model, PRO_MDL_ASSEMBLY, &settings, &settings_strings, &scene) ==
                      CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS &&
                  (scene.assemblies[0].assembly.flags &
                   CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES) == 0,
                  "missing assembly mass properties were accepted");
}


static int
run_named_test(const std::string& name)
{
    if (name == "rotated-mixed-units")
        return test_rotated_component_unit_matrices();
    if (name == "toolkit-derived-units")
        return test_toolkit_derived_length_scale();
    if (name == "capture-failure-classification")
        return test_pre_suppression_capture_failure_classification();
    if (name == "mesh-near-vertices")
        return test_capture_uses_replay_vertex_tolerance();
    if (name == "mesh-collapsed-facet")
        return test_capture_rejects_replay_collapsed_facet();

    std::fprintf(stderr, "unknown capture-policy test case: %s\n", name.c_str());
    return 0;
}


int
main(int argument_count, char *arguments[])
{
    if (argument_count == 2)
        return run_named_test(arguments[1]) ? 0 : 1;
    if (argument_count != 1)
        return 1;

    return test_suppression_transaction() &&
                   test_component_unit_matrices() &&
                   test_tessellation_policy() &&
                   test_tessellation_data_transaction() &&
                   test_feature_order_policy() &&
                   test_scene_results_and_region_ids() &&
                   test_assembly_metadata_capture() &&
                   test_capture_uses_replay_vertex_tolerance() &&
                   test_capture_rejects_replay_collapsed_facet() ?
               0 : 1;
}
