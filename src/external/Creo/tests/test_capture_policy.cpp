#include "common.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "frontend/model_capture.h"


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
    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_tessellation_policy policy;
    struct snapshot_mesh_capture mesh = {};
    struct attempt_fixture fixture = {};

    settings.chord_mode = CREO_BRL_SNAPSHOT_CHORD_RELATIVE;
    settings.max_chord = 10.0;
    settings.min_angle = 0.2;
    if (!expect(creo_brl_build_tessellation_policy(&settings, 25.4, 254.0, &policy),
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
    settings.max_chord = 2.54;
    if (!expect(creo_brl_build_tessellation_policy(&settings, 25.4, 254.0, &policy),
                "absolute tessellation policy was rejected") ||
        !expect(near_equal(policy.attempts.front().chord, 0.1) &&
                    near_equal(policy.attempts.back().chord, 0.01),
                "absolute chord values were not converted to model units"))
        return 0;

    settings.max_chord = 0.0;
    return expect(!creo_brl_build_tessellation_policy(&settings, 25.4, 254.0, &policy),
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
    if (!expect(creo_brl_capture_tessellation_data(
                    &surface, 1, 25.4, 1, 100000u, &mesh),
                "valid tessellation data was rejected") ||
        !expect(mesh.vertices.size() == 4u && mesh.triangles.size() == 4u &&
                    mesh.normals.size() == 12u && mesh.normal_indices.size() == 12u &&
                    mesh.is_solid,
                "valid tessellation data was not captured as a closed mesh"))
        return 0;

    surface.n_facets = 2;
    surface.facets = partially_invalid;
    if (!expect(!creo_brl_capture_tessellation_data(
                    &surface, 1, 25.4, 1, 100000u, &mesh),
                "late invalid facet was accepted") ||
        !expect(mesh.vertices.empty() && mesh.triangles.empty() &&
                    mesh.normals.empty() && mesh.normal_indices.empty(),
                "late invalid facet left partial mesh output"))
        return 0;

    surface.n_facets = 100001;
    surface.facets = NULL;
    if (!expect(!creo_brl_capture_tessellation_data(
                    &surface, 1, 25.4, 0, 100000u, &mesh),
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


static int
run_named_test(const std::string& name)
{
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

    return test_tessellation_policy() &&
                   test_tessellation_data_transaction() &&
                   test_capture_uses_replay_vertex_tolerance() &&
                   test_capture_rejects_replay_collapsed_facet() ?
               0 : 1;
}
