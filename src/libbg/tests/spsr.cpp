/*                       S P S R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <cmath>
#include <vector>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bg/spsr.h"
#include "bg/trimesh.h"

static int failures = 0;

static void
expect(bool condition, const char *message)
{
    if (condition)
        return;
    bu_log("FAIL: %s\n", message);
    failures++;
}

static std::vector<struct bg_3d_spsr_sample>
sphere_samples(size_t count)
{
    const fastf_t golden_angle = M_PI * (3.0 - std::sqrt(5.0));
    std::vector<struct bg_3d_spsr_sample> samples(count);
    for (size_t i = 0; i < count; i++) {
        fastf_t y = 1.0 - 2.0 *
            (static_cast<fastf_t>(i) + 0.5) / count;
        fastf_t radius = std::sqrt(std::max(0.0, 1.0 - y * y));
        fastf_t angle = golden_angle * i;
        VSET(samples[i].point, radius * std::cos(angle), y,
            radius * std::sin(angle));
        VMOVE(samples[i].normal, samples[i].point);
    }
    return samples;
}

static void
test_boundary_modes(void)
{
    const int side = 8;
    const int sample_count = 6 * side * side;
    /* Use a solver regime known to distinguish the boundary modes instead of
     * relying on library defaults, which may change independently. */
    const fastf_t boundary_samples_per_node = 1.1;
    const fastf_t boundary_point_weight = 8.0;
    point_t *samples = static_cast<point_t *>(bu_calloc(sample_count,
        sizeof(point_t), "SPSR boundary test samples"));
    vect_t *normals = static_cast<vect_t *>(bu_calloc(sample_count,
        sizeof(vect_t), "SPSR boundary test normals"));

    int sample = 0;
    for (int face = 0; face < 6; face++) {
        const int axis = face / 2;
        const fastf_t sign = (face % 2) ? 1.0 : -1.0;
        for (int first = 0; first < side; first++) {
            for (int second = 0; second < side; second++) {
                samples[sample][axis] = sign;
                samples[sample][(axis + 1) % 3] =
                    -1.0 + 2.0 * first / (side - 1);
                samples[sample][(axis + 2) % 3] =
                    -1.0 + 2.0 * second / (side - 1);
                normals[sample][axis] = sign;
                sample++;
            }
        }
    }

    int face_counts[3] = {0, 0, 0};
    for (int boundary = BG_3D_SPSR_BOUNDARY_FREE;
        boundary <= BG_3D_SPSR_BOUNDARY_DIRICHLET; boundary++) {
        struct bg_3d_spsr_opts options = BG_3D_SPSR_OPTS_DEFAULT;
        options.depth = 5;
        options.full_depth = 3;
        options.threads = 1;
        options.samples_per_node = boundary_samples_per_node;
        options.scale = 1.2;
        options.point_weight = boundary_point_weight;
        options.btype = boundary;

        int *faces = NULL;
        int face_count = 0;
        point_t *points = NULL;
        int point_count = 0;
        int result = bg_3d_spsr(&faces, &face_count, &points,
            &point_count, samples, normals, sample_count, &options);
        bool solid = result == BRLCAD_OK && faces && points &&
            face_count > 0 && point_count > 0 &&
            !bg_trimesh_solid2(point_count, face_count,
                reinterpret_cast<fastf_t *>(points), faces, NULL);
        if (!solid) {
            bu_log("SPSR boundary %d failed: result %d, %d points, "
                "%d faces\n", boundary, result, point_count, face_count);
        }
        expect(solid, "each SPSR boundary mode produces a solid mesh");
        face_counts[boundary - BG_3D_SPSR_BOUNDARY_FREE] = face_count;
        if (faces)
            bu_free(faces, "SPSR boundary test faces");
        if (points)
            bu_free(points, "SPSR boundary test points");
    }

    expect(face_counts[0] != face_counts[1] ||
        face_counts[1] != face_counts[2],
        "SPSR boundary modes affect reconstruction");
    bu_free(samples, "SPSR boundary test samples");
    bu_free(normals, "SPSR boundary test normals");
}

struct callback_state {
    size_t calls;
    size_t hints;
    struct bg_3d_spsr_sample additional_sample;
    bool refine_once;
    bool stop;
};

static int
accept_candidate(struct bg_3d_spsr_refinement_response *response,
    const struct bg_3d_spsr_refinement_request *request, void *data)
{
    struct callback_state *state =
        static_cast<struct callback_state *>(data);
    state->calls++;
    state->hints = request->hint_count;
    response->validation.ray_count = 100;
    if (state->stop) {
	response->validation.passed = 0;
	response->stop_refinement = 1;
    } else if (state->refine_once && state->calls == 1) {
	response->samples = &state->additional_sample;
	response->sample_count = 1;
	response->validation.passed = 0;
    } else {
	response->validation.passed = 1;
    }
    return BRLCAD_OK;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);

    test_boundary_modes();

    std::vector<struct bg_3d_spsr_sample> samples = sphere_samples(256);
    struct bg_3d_spsr_adaptive_opts options =
        BG_3D_SPSR_ADAPTIVE_OPTS_DEFAULT;
    options.solver.depth = 5;
    options.max_refinement_passes = 0;

    int *faces = NULL;
    int face_count = 0;
    point_t *vertices = NULL;
    int vertex_count = 0;
    struct bg_3d_spsr_report report = {};
    int ret = bg_3d_spsr_adaptive(&faces, &face_count, &vertices,
        &vertex_count, samples.data(), samples.size(), &options,
        NULL, NULL, &report);
    expect(ret == BRLCAD_OK, "fixed structured reconstruction succeeds");
    expect(face_count > 0 && vertex_count > 0,
        "fixed structured reconstruction returns a mesh");
    expect(report.solve_count == 1 && report.validation.passed,
        "fixed structured reconstruction reports one successful solve");
    if (faces)
        bu_free(faces, "SPSR test faces");
    if (vertices)
        bu_free(vertices, "SPSR test vertices");

    struct callback_state state = {};
    options.max_refinement_passes = 3;
    options.target_feature_size = 0.1;
    options.max_points = 512;
    faces = NULL;
    vertices = NULL;
    face_count = 0;
    vertex_count = 0;
    ret = bg_3d_spsr_adaptive(&faces, &face_count, &vertices,
        &vertex_count, samples.data(), samples.size(), &options,
        accept_candidate, &state, &report);
    expect(ret == BRLCAD_OK, "callback can accept an adaptive candidate");
    expect(state.calls == 1, "accepted candidate stops adaptive solving");
    expect(report.validation.ray_count == 100,
        "adaptive report preserves callback validation");
    if (faces)
        bu_free(faces, "SPSR test faces");
    if (vertices)
        bu_free(vertices, "SPSR test vertices");

    state = {};
    state.stop = true;
    faces = NULL;
    vertices = NULL;
    face_count = 0;
    vertex_count = 0;
    ret = bg_3d_spsr_adaptive(&faces, &face_count, &vertices,
	&vertex_count, samples.data(), samples.size(), &options,
	accept_candidate, &state, &report);
    expect(ret == BRLCAD_ERROR,
	"callback can stop an unproductive adaptive solve");
    expect(state.calls == 1 && report.solve_count == 1 &&
	report.termination == BG_3D_SPSR_CALLBACK_STOP,
	"callback stop preserves the final validation report");

    state = {};
    state.refine_once = true;
    VSET(state.additional_sample.point, 1.0, 0.0, 0.0);
    VSET(state.additional_sample.normal, 1.0, 0.0, 0.0);
    faces = NULL;
    vertices = NULL;
    face_count = 0;
    vertex_count = 0;
    ret = bg_3d_spsr_adaptive(&faces, &face_count, &vertices,
	&vertex_count, samples.data(), samples.size(), &options,
	accept_candidate, &state, &report);
    expect(ret == BRLCAD_OK, "adaptive refinement can add a source sample");
    expect(state.calls == 2 && report.solve_count == 2,
	"new source sample triggers a second solve");
    expect(report.accepted_sample_count == 1 &&
	report.final_sample_count == samples.size() + 1,
	"adaptive report counts incorporated samples");
    if (faces)
	bu_free(faces, "SPSR test faces");
    if (vertices)
	bu_free(vertices, "SPSR test vertices");

    options.solver.threads = 1;
    options.max_refinement_passes = 0;
    ret = bg_3d_spsr_adaptive(&faces, &face_count, &vertices,
        &vertex_count, samples.data(), samples.size(), &options,
        NULL, NULL, NULL);
    expect(ret == BRLCAD_OK, "explicit serial reconstruction succeeds");
    if (faces)
        bu_free(faces, "SPSR test faces");
    if (vertices)
        bu_free(vertices, "SPSR test vertices");

    expect(bg_3d_spsr_adaptive(NULL, &face_count, &vertices,
        &vertex_count, samples.data(), samples.size(), &options,
        NULL, NULL, NULL) == BRLCAD_ERROR,
        "invalid output arguments are rejected");

    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
