/*                     T E S T _ J T _ T O P O L O G Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "jt_topology.h"

#include "bu/app.h"

#include <iostream>
#include <vector>

namespace {

/* TST-023: single triangle plus its cover face.  In the dual representation a
 * lone triangle is two valence-three vertices (the triangle and its cover) joined
 * by three degree-two faces (the primal triangle's three vertices).  The cover
 * vertex carries flag bit 1 so it is dropped from the exported polygons, leaving
 * one triangular polygon whose ring indexes the three primal coordinate records. */
int test_single_triangle()
{
    jt::TopologySymbols symbols;
    symbols.vertex_valences = {3, 3};   /* two valence-3 dual vertices */
    symbols.vertex_groups = {7, 7};
    symbols.vertex_flags = {0, 1};      /* second vertex is the cover face */
    symbols.face_degrees[0] = {2, 2};   /* three degree-2 faces across contexts 0/1 */
    symbols.face_degrees[1] = {2};
    symbols.face_attribute_masks[0] = {0, 0, 0};  /* one mask per degree-2 face */

    jt::TopologyMesh mesh;
    std::string error;
    if (!jt::decode_topology(symbols, mesh, error)) {
	std::cerr << error << '\n';
	return 1;
    }
    /* Exactly one exported polygon: the cover vertex is suppressed. */
    if (mesh.polygons.size() != 1 || mesh.polygon_groups.size() != 1)
	return 2;
    /* A triangle: a three-face ring naming coordinate records 0, 1, 2. */
    if (mesh.polygons[0].size() != 3 || mesh.polygon_groups[0] != 7)
	return 3;
    if (mesh.polygons[0][0] != 0 || mesh.polygons[0][1] != 1 || mesh.polygons[0][2] != 2)
	return 4;
    /* Fan triangulation of a single triangle is that triangle unchanged. */
    if (mesh.triangles != std::vector<int32_t>({0, 1, 2}))
	return 5;
    /* Every symbol must have been consumed: dropping a vertex group leaves the
     * vertex symbol arrays inconsistent and the decode must fail with a message. */
    symbols.vertex_groups.pop_back();
    if (jt::decode_topology(symbols, mesh, error) || error.empty())
	return 6;
    return 0;
}

/* TST-024: single quadrilateral plus its cover face.  A lone quad is two
 * valence-four dual vertices joined by four degree-two faces.  The seed vertex is
 * valence 4, so its four faces are read from face-degree context 3 until the ring
 * closes and the final face from context 4; the exported polygon is a four-face
 * ring fan-triangulated from face 0 into triangles (0,1,2) and (0,2,3). */
int test_quad_face()
{
    jt::TopologySymbols symbols;
    symbols.vertex_valences = {4, 4};   /* two valence-4 dual vertices */
    symbols.vertex_groups = {5, 5};
    symbols.vertex_flags = {0, 1};      /* second vertex is the cover face */
    symbols.face_degrees[3] = {2, 2, 2};/* valence-4 seed reads context 3 ... */
    symbols.face_degrees[4] = {2};      /* ... then context 4 to close the ring */
    symbols.face_attribute_masks[0] = {0, 0, 0, 0};  /* one mask per degree-2 face */

    jt::TopologyMesh mesh;
    std::string error;
    if (!jt::decode_topology(symbols, mesh, error)) {
	std::cerr << error << '\n';
	return 10;
    }
    if (mesh.polygons.size() != 1 || mesh.polygon_groups.size() != 1)
	return 11;
    /* A quad: a four-face ring naming coordinate records 0, 1, 2, 3. */
    if (mesh.polygons[0].size() != 4 || mesh.polygon_groups[0] != 5)
	return 12;
    if (mesh.polygons[0][0] != 0 || mesh.polygons[0][1] != 1 ||
	mesh.polygons[0][2] != 2 || mesh.polygons[0][3] != 3)
	return 13;
    /* Fan triangulation from ring vertex 0: (0,1,2) and (0,2,3). */
    if (mesh.triangles != std::vector<int32_t>({0, 1, 2, 0, 2, 3}))
	return 14;
    return 0;
}

/* TST-028: face attribute mask bitwise extraction (jt_topology.cpp lines
 * 133-159).  For a degree <= 64 face the per-corner attribute presence comes from
 * a single u64 mask via (mask >> i) & 1; the exported attribute_indices are -1 at
 * a corner with no bound attribute and a running attribute counter otherwise.  A
 * bit set beyond the face degree must be rejected, and the degree > 64 branch
 * reads its bits from the high_degree_attribute_masks u32 array in 32-bit words,
 * so a short array must fail rather than read past its end. */
int test_topology_attribute_masks()
{
    /* Base single-triangle sample (see test_single_triangle): three degree-2
     * faces activated in order, each reading one u64 mask from context 0. */
    jt::TopologySymbols base;
    base.vertex_valences = {3, 3};
    base.vertex_groups = {7, 7};
    base.vertex_flags = {0, 1};
    base.face_degrees[0] = {2, 2};
    base.face_degrees[1] = {2};

    /* Set bit 0 of the first face's mask: corner 0 of that face binds attribute 0
     * (the first present bit across all faces).  The seed dual vertex sits at slot
     * 0 of each of its three seeded faces, so its polygon corner for the first
     * face must carry attribute 0 and the other two corners -1. */
    {
	jt::TopologySymbols symbols = base;
	symbols.face_attribute_masks[0] = {1, 0, 0};   /* bit 0 set on face 0 only */
	jt::TopologyMesh mesh;
	std::string error;
	if (!jt::decode_topology(symbols, mesh, error)) {
	    std::cerr << error << '\n';
	    return 20;
	}
	if (mesh.attribute_indices.size() != 1 || mesh.attribute_indices[0].size() != 3)
	    return 21;
	/* The corner ordering matches the polygon ring (faces 0, 1, 2). */
	if (mesh.attribute_indices[0][0] != 0 ||
	    mesh.attribute_indices[0][1] != -1 || mesh.attribute_indices[0][2] != -1)
	    return 22;
    }

    /* A mask bit set at or beyond the face degree (bit 2 of a degree-2 face) is
     * rejected: the "mask exceeds its face degree" guard fires (mask >> degree). */
    {
	jt::TopologySymbols symbols = base;
	symbols.face_attribute_masks[0] = {0x4, 0, 0};   /* bit 2 set, degree is 2 */
	jt::TopologyMesh mesh;
	std::string error;
	if (jt::decode_topology(symbols, mesh, error) || error.empty())
	    return 23;
    }

    /* Degree > 64 uses the high_degree_attribute_masks u32 array, sized
     * ceil(degree/32) words per face.  An empty array must make activation fail
     * with the high-degree stream error rather than read out of bounds. */
    {
	jt::TopologySymbols symbols;
	symbols.vertex_valences = {3, 3};
	symbols.vertex_groups = {0, 0};
	symbols.vertex_flags = {0, 1};
	/* Seed a valence-3 vertex whose first face is degree 65 (> 64), so the
	 * decoder consults the (empty) high-degree mask array. */
	symbols.face_degrees[0] = {65};
	jt::TopologyMesh mesh;
	std::string error;
	if (jt::decode_topology(symbols, mesh, error) || error.empty())
	    return 24;
    }
    return 0;
}

/* TST-029: context-dependent face-degree stream indexing (jt_topology.cpp
 * context(), lines 104-114).  activate_face() picks one of eight face-degree
 * streams by the seed vertex valence (3 -> 0/1/2, 4 -> 3/4/5, 5 -> 6, 6+ -> 7)
 * and the ratio of accumulated known face degree to known-face count.  A fresh
 * seed vertex has no known faces (known == 0), so total < known*6 is false and
 * total == known*6 is true: valence 3 starts in context 1, valence 4 in context
 * 4, valence 5 in context 6, and valence 6+ in context 7.  Placing the degree
 * symbols only in the expected context proves degree_pos[ctx] indexes the chosen
 * stream. */
int test_topology_context_indexing()
{
    struct Case {
	int valence;
	int start_context;   /* context the first activate_face() must read */
	int rc;
    };
    const Case cases[] = {
	{3, 1, 30},
	{4, 4, 32},
	{5, 6, 34},
	{6, 7, 36},
    };
    for (const Case &c : cases) {
	/* Two dual vertices of the given valence joined by that many degree-2
	 * faces; the cover vertex (flag bit 1) is dropped, leaving one polygon.
	 * The seed reads its faces starting from c.start_context: valence 3 and
	 * 4 then advance into contexts 2/5 as known faces accumulate, while
	 * valence 5 (context 6) and 6+ (context 7) stay in a single stream. */
	const size_t v = static_cast<size_t>(c.valence);
	jt::TopologySymbols symbols;
	symbols.vertex_valences = {c.valence, c.valence};
	symbols.vertex_groups = {2, 2};
	symbols.vertex_flags = {0, 1};
	symbols.face_attribute_masks[0].assign(v, 0);

	/* Distribute the v degree-2 faces across the contexts context() returns
	 * as known-face count climbs from 0 to v-1.  Reproduce that walk here so
	 * the symbols land in exactly the streams the decoder will read. */
	std::vector<int32_t> per_context[8];
	int known = 0, total = 0;
	for (size_t f = 0; f < v; ++f) {
	    int ctx;
	    if (c.valence == 3) ctx = total < known * 6 ? 0 : (total == known * 6 ? 1 : 2);
	    else if (c.valence == 4) ctx = total < known * 4 ? 3 : (total == known * 4 ? 4 : 5);
	    else ctx = c.valence == 5 ? 6 : 7;
	    if (f == 0 && ctx != c.start_context)
		return c.rc;   /* the documented starting context is wrong */
	    per_context[ctx].push_back(2);   /* every face is degree 2 */
	    ++known;
	    total += 2;                      /* each degree-2 face contributes 2 */
	}
	for (int ctx = 0; ctx < 8; ++ctx)
	    symbols.face_degrees[ctx] = per_context[ctx];

	jt::TopologyMesh mesh;
	std::string error;
	if (!jt::decode_topology(symbols, mesh, error)) {
	    std::cerr << error << '\n';
	    return c.rc + 1;
	}
	if (mesh.polygons.size() != 1 || mesh.polygons[0].size() != v)
	    return c.rc + 1;
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    bu_setprogname(argv[0]);

    if (int rc = test_single_triangle())
	return rc;
    if (int rc = test_quad_face())
	return rc;
    if (int rc = test_topology_attribute_masks())
	return rc;
    if (int rc = test_topology_context_indexing())
	return rc;
    return 0;
}
