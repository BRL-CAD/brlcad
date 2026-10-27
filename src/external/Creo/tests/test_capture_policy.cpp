#include "common.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "bu/units.h"
#include "frontend/model_capture.h"
#include "frontend/snapshot_writer.h"


int
creo_brl_capture_feature_suppression(
    ProMdl,
    double,
    const struct creo_brl_snapshot_settings *,
    struct snapshot_part_capture *,
    std::vector<int> *,
    int *)
{
    return 0;
}


void
creo_brl_resume_suppressed_features(ProMdl, const std::vector<int>&)
{
}


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

    struct snapshot_part_capture part = {};
    part.part.flags = CREO_BRL_SNAPSHOT_PART_HAS_MESH |
                      CREO_BRL_SNAPSHOT_PART_HAS_NORMALS;
    part.vertices.resize(3u);
    part.triangles.resize(1u);
    part.normals.resize(3u);
    part.normal_indices.resize(3u);
    creo_brl_mark_bbox_fallback(&part);
    return expect((part.part.flags & CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK) != 0 &&
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

    int appended_children = 0;
    const int child_results[] = {
        CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS,
        CREO_BRL_SNAPSHOT_CAPTURE_SKIPPED
    };
    for (size_t child = 0; child < 2u; ++child) {
        const int disposition = creo_brl_capture_child_result(child_results[child]);

        if (disposition == CREO_BRL_CAPTURE_CHILD_APPEND)
            ++appended_children;
    }

    struct snapshot_assembly_capture parent = {};
    struct snapshot_assembly_capture empty_child = {};
    parent.members.resize(1u);
    scene.assemblies.push_back(parent);
    scene.assemblies.push_back(empty_child);

    return expect(appended_children == 1,
                  "skipped child hid a valid sibling") &&
           expect(creo_brl_capture_child_result(CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE) ==
                      CREO_BRL_CAPTURE_CHILD_FATAL,
                  "fatal child capture error was omitted") &&
           expect(creo_brl_finalize_assembly_capture(&scene, 1u) ==
                      CREO_BRL_SNAPSHOT_CAPTURE_SKIPPED,
                  "empty subassembly was not skipped") &&
           expect(scene.assemblies.size() == 1u &&
                      scene.assemblies[0].members.size() == 1u,
                  "empty subassembly removal discarded a valid sibling") &&
           expect(creo_brl_finalize_assembly_capture(&scene, 0u) ==
                      CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS,
                  "nonempty assembly was not retained") &&
           expect(creo_brl_finalize_assembly_capture(&scene, 1u) ==
                      CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE,
                  "invalid assembly finalization was not fatal");
}


static int
test_assembly_metadata_capture()
{
    ProMassProperty mass_properties = {};
    struct snapshot_assembly_capture capture = {};

    mass_properties.density = 7.5;
    mass_properties.mass = 125.0;
    mass_properties.volume = 16.5;
    mass_properties.surface_area = 48.0;
    return expect(creo_brl_set_assembly_mass_properties(&mass_properties, &capture),
                  "assembly mass properties were not captured") &&
           expect((capture.assembly.flags &
                       CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES) != 0,
                  "assembly mass-property flag was not captured") &&
           expect(near_equal(capture.assembly.density, mass_properties.density) &&
                      near_equal(capture.assembly.mass, mass_properties.mass) &&
                      near_equal(capture.assembly.volume, mass_properties.volume) &&
                      near_equal(capture.assembly.surface_area,
                                 mass_properties.surface_area),
                  "assembly mass-property values changed during capture") &&
           expect(!creo_brl_set_assembly_mass_properties(NULL, &capture),
                  "missing assembly mass properties were accepted");
}


static int
run_named_test(const std::string& name)
{
    if (name == "rotated-mixed-units")
        return test_rotated_component_unit_matrices();
    if (name == "toolkit-derived-units")
        return test_toolkit_derived_length_scale();
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

    return test_component_unit_matrices() &&
                   test_tessellation_policy() &&
                   test_tessellation_data_transaction() &&
                   test_scene_results_and_region_ids() &&
                   test_assembly_metadata_capture() &&
                   test_capture_uses_replay_vertex_tolerance() &&
                   test_capture_rejects_replay_collapsed_facet() ?
               0 : 1;
}
