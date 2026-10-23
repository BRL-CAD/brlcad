#include "common.h"

#include <cstdio>
#include <string>

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
                      &surface, 1, 1.0, 0, &mesh),
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
        &surface, 1, 1.0, 0, &mesh);

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

    return test_capture_uses_replay_vertex_tolerance() &&
                   test_capture_rejects_replay_collapsed_facet() ?
               0 : 1;
}
