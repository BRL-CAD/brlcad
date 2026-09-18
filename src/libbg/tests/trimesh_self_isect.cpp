/*           T R I M E S H _ S E L F _ I S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file tests/trimesh_self_isect.cpp */

#include "common.h"

#include <array>
#include <limits>
#include <vector>

#include "bg/trimesh.h"
#include "bu/app.h"
#include "bu/log.h"
#include "Mathematics/MeshValidation.h"
#include "Mathematics/MeshHoleFilling.h"

static int
check_case(const char *name, const point_t *points, size_t num_points,
	const int *faces, size_t num_faces, int expected, int expected_pairs = -1)
{
    int found = bg_trimesh_self_isect(faces, num_faces, points, num_points);
    if (found != expected) {
	bu_log("%s: expected %d, got %d\n", name, expected, found);
	return 1;
    }
    if (expected < 0)
	return 0;

    std::vector<gte::Vector3<double>> vertices;
    std::vector<std::array<int32_t, 3>> triangles;
    for (size_t i = 0; i < num_points; ++i)
	vertices.push_back({points[i][0], points[i][1], points[i][2]});
    for (size_t i = 0; i < num_faces; ++i)
	triangles.push_back({faces[3*i], faces[3*i+1], faces[3*i+2]});
    if (num_faces > 0) {
	auto result = gte::MeshValidation<double>::Validate(vertices, triangles, true);
	if (result.hasSelfIntersections != (expected > 0) ||
	    (expected_pairs >= 0 && result.intersectingTrianglePairs != static_cast<size_t>(expected_pairs))) {
	    bu_log("%s: diagnostic count %zu, expected %d\n", name,
		result.intersectingTrianglePairs, expected_pairs);
	    return 1;
	}
    }
    return 0;
}

int
main(int UNUSED(argc), const char *argv[])
{
    bu_setprogname(argv[0]);
    int failures = 0;
    const point_t base[] = {
	{0, 0, 0}, {2, 0, 0}, {0, 2, 0},
	{0.5, 0.5, -1}, {0.5, 0.5, 1}
    };
    const int crossing[] = {0, 1, 2, 0, 3, 4};
    failures += check_case("shared vertex with crossing", base, 5, crossing, 2, 1, 1);

    const point_t large_extent_touch[] = {
	{1.0e-11, 0, 0}, {1.0e7 + 1.0e-11, 0, 0},
	{1.0e-11, 1.0e7, 0}, {-1.0e7 + 1.0e-11, 0, 1.0e7},
	{1.0e-11, -1.0e7, 1.0e7}
    };
    const int vertex_last[] = {1, 2, 0, 3, 4, 0};
    failures += check_case("shared vertex with large extent", large_extent_touch, 5,
	vertex_last, 2, 0, 0);
    const point_t large_extent_cross[] = {
	{1.0e-11, 0, 0}, {1.0e7 + 1.0e-11, 0, 0},
	{1.0e-11, 1.0e7, 0}, {1.0e-5 + 1.0e-11, 1.0e-5, -1},
	{1.0e-5 + 1.0e-11, 1.0e-5, 1}
    };
    failures += check_case("small crossing on large triangle", large_extent_cross, 5,
	vertex_last, 2, 1, 1);

    // Two thin wing facets sharing only vertex 1.  GTE's computed contact
    // points differ from that vertex by roundoff, not by a crossing.
    const point_t thin_wing_joint[] = {
	{226.67034893030407, 280, 21.576417093095706},
	{219.99057362097182, 420, 31.951862388809637},
	{219.55966619917172, 420, 32.033775058413987},
	{226.6426768041579, 280, 21.073177335689227},
	{219.96830407125381, 420, 31.49880938180566}
    };
    const int wing_faces[] = {0, 1, 2, 3, 4, 1};
    failures += check_case("thin wing shared vertex", thin_wing_joint, 5,
	wing_faces, 2, 0, 0);

    const point_t thin_wing_roundoff[] = {
	{112.50942906974174, 341.25, 24.0039606218491},
	{90.007543255793394, 455, 31.929848271198175},
	{89.150655159118429, 455, 32.1073840972605},
	{112.49057093025826, 341.25, 23.721088529596759},
	{89.992456744206606, 455, 31.703550597396301}
    };
    failures += check_case("thin wing vertex roundoff", thin_wing_roundoff, 5,
	wing_faces, 2, 0, 0);

    // Their boxes overlap, and GTE's separating-axis test reports contact,
    // but its full intersection query finds these thin faces disjoint.
    const point_t thin_wing_disjoint[] = {
	{151.56909011106083, 113.75, 9.3593771810054793},
	{123.7207397950647, 227.5, 18.298201672124883},
	{144.34086309424214, 113.75, 10.742335472713284},
	{129.91636295233786, 227.5, 17.11280885066105},
	{108.26363579361488, 341.25, 24.866240520316623},
	{103.10061649588725, 341.25, 25.854067871536483}
    };
    const int disjoint_faces[] = {0, 1, 2, 3, 4, 5};
    failures += check_case("thin disjoint faces", thin_wing_disjoint, 6,
	disjoint_faces, 2, 0, 0);

    const point_t shared_edge[] = {
	{0, 0, 0}, {2, 0, 0}, {0, 2, 0}, {1, 0, 1}
    };
    const int edge_faces[] = {0, 1, 2, 0, 1, 3};
    failures += check_case("valid shared edge", shared_edge, 4, edge_faces, 2, 0, 0);

    const point_t thin_edge[] = {
	{0, 0, 0}, {2, 0, 0}, {0, 2, 0}, {1, 0, 1.0e-10}
    };
    failures += check_case("thin nonplanar shared edge", thin_edge, 4, edge_faces, 2, 0, 0);

    const point_t overlapping_edge[] = {
	{0, 0, 0}, {2, 0, 0}, {0, 2, 0}, {1, 0.3, 0}
    };
    failures += check_case("shared edge with overlap", overlapping_edge, 4, edge_faces, 2, 1, 1);

    const point_t shared_vertex[] = {
	{0, 0, 0}, {2, 0, 0}, {0, 2, 0}, {-1, 0, 0}, {0, -1, 0}
    };
    const int vertex_faces[] = {0, 1, 2, 0, 3, 4};
    failures += check_case("valid shared vertex", shared_vertex, 5, vertex_faces, 2, 0, 0);

    const point_t planar_overlap[] = {
	{0, 0, 0}, {2, 0, 0}, {0, 2, 0}, {1, 0.2, 0}, {0.2, 1, 0}
    };
    failures += check_case("coplanar shared vertex overlap", planar_overlap, 5,
	vertex_faces, 2, 1, 1);

    const point_t coincident[] = {
	{0, 0, 0}, {1, 0, 0}, {0, 1, 0},
	{0, 0, 0}, {1, 0, 0}, {0, 1, 0},
	{0, 0, 0}, {1, 0, 0}, {0, 1, 0}
    };
    const int coincident_faces[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    failures += check_case("three coincident faces", coincident, 9, coincident_faces, 3, 1, 3);

    const point_t unconnected_touch[] = {
	{0, 0, 0}, {1, 0, 0}, {0, 1, 0},
	{0, 0, 0}, {-1, 0, 0}, {0, -1, 0}
    };
    const int two_faces[] = {0, 1, 2, 3, 4, 5};
    failures += check_case("unconnected contact", unconnected_touch, 6, two_faces, 2, 1, 1);

    const point_t cube[] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
    };
    const int cube_faces[] = {
	0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
	0, 1, 5, 0, 5, 4, 1, 2, 6, 1, 6, 5,
	2, 3, 7, 2, 7, 6, 3, 0, 4, 3, 4, 7
    };
    failures += check_case("closed cube", cube, 8, cube_faces, 12, 0, 0);

    point_t overlapping_cubes[16];
    int overlapping_faces[72];
    for (size_t i = 0; i < 8; ++i) {
	for (size_t axis = 0; axis < 3; ++axis) {
	    overlapping_cubes[i][axis] = cube[i][axis];
	    overlapping_cubes[i+8][axis] = cube[i][axis] + 0.5;
	}
    }
    for (size_t i = 0; i < 36; ++i) {
	overlapping_faces[i] = cube_faces[i];
	overlapping_faces[i+36] = cube_faces[i] + 8;
    }
    failures += check_case("overlapping closed cubes", overlapping_cubes, 16,
	overlapping_faces, 24, 1);

    const int bad_index[] = {0, 1, 5};
    failures += check_case("invalid index", base, 5, bad_index, 1, -1);
    const int degenerate[] = {0, 0, 1};
    failures += check_case("degenerate face", base, 5, degenerate, 1, -1);
    point_t nonfinite[3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    nonfinite[1][0] = std::numeric_limits<double>::quiet_NaN();
    const int triangle[] = {0, 1, 2};
    failures += check_case("nonfinite point", nonfinite, 3, triangle, 1, -1);
    failures += check_case("empty mesh", nullptr, 0, nullptr, 0, 0);

    // A single open triangle gets a coincident cap.  Permissive filling may
    // keep it; the explicitly requested self-intersection check rolls back.
    std::vector<gte::Vector3<double>> fill_vertices = {{0, 0, 0}, {2, 0, 0}, {0, 2, 0}};
    const std::vector<std::array<int32_t, 3>> original = {{0, 1, 2}};
    for (bool strict : {false, true}) {
	auto faces = original;
	gte::MeshHoleFilling<double>::Parameters params;
	params.method = gte::MeshHoleFilling<double>::TriangulationMethod::EarClipping3D;
	params.validateOutput = strict;
	params.requireNoSelfIntersections = strict;
	gte::MeshHoleFilling<double>::FillHoles(fill_vertices, faces, params);
	if (faces.size() != (strict ? original.size() : original.size() + 1)) {
	    bu_log("hole filling self-intersection option failed (strict=%d)\n", strict);
	    ++failures;
	}
    }

    if (failures)
	bu_log("%d self intersection cases failed\n", failures);
    return failures ? 1 : 0;
}
