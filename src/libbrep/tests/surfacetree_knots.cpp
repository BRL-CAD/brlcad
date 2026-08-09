/*                 S U R F A C E T R E E _ K N O T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <cmath>
#include <cstdio>
#include <list>
#include <vector>

#include "brep.h"
#include "brep/surfacetree.h"


int
main()
{
    ON_NurbsSurface surface(3, false, 3, 3, 8, 7);
    if (!surface.MakeClampedUniformKnotVector(0) ||
	    !surface.MakeClampedUniformKnotVector(1)) {
	std::printf("FAIL: knot vector construction\n");
	return 1;
    }
    const ON_Interval domain[2] = {surface.Domain(0), surface.Domain(1)};
    for (int u = 0; u < surface.CVCount(0); ++u) {
	for (int v = 0; v < surface.CVCount(1); ++v) {
	    const double uf = (double)u / (surface.CVCount(0) - 1);
	    const double vf = (double)v / (surface.CVCount(1) - 1);
	    const double x = domain[0].ParameterAt(uf);
	    const double y = domain[1].ParameterAt(vf);
	    const double z = 0.05 * std::sin(0.7 * u) * std::cos(0.6 * v);
	    if (!surface.SetCV(u, v, ON_3dPoint(x, y, z))) {
		std::printf("FAIL: control point construction\n");
		return 1;
	    }
	}
    }
    if (!surface.IsValid()) {
	std::printf("FAIL: invalid test surface\n");
	return 1;
    }

    ON_Brep brep;
    ON_BrepFace *face = brep.NewFace(surface);
    if (!face) {
	std::printf("FAIL: face construction\n");
	return 1;
    }
    brlcad::SurfaceTree tree(face, false, 0);
    if (!tree.Valid()) {
	std::printf("FAIL: depth-zero SurfaceTree construction reason=%d\n",
	    (int)tree.Failure());
	return 1;
    }

    const ON_Surface *tree_surface = face->SurfaceOf();
    std::vector<double> spans[2];
    size_t expected_leaves = 1;
    for (int direction = 0; direction < 2; ++direction) {
	const int span_count = tree_surface->SpanCount(direction);
	if (span_count < 2) {
	    std::printf("FAIL: direction %d has only %d spans\n", direction,
		span_count);
	    return 1;
	}
	spans[direction].resize((size_t)span_count + 1);
	if (!tree_surface->GetSpanVector(direction,
		spans[direction].data())) {
	    std::printf("FAIL: direction %d span extraction\n", direction);
	    return 1;
	}
	expected_leaves *= (size_t)span_count;
    }

    std::list<const brlcad::BBNode *> leaves;
    tree.getLeaves(leaves);
    if (leaves.size() != expected_leaves) {
	std::printf("FAIL: depth-zero leaves=%zu expected=%zu\n", leaves.size(),
	    expected_leaves);
	return 1;
    }
    for (std::list<const brlcad::BBNode *>::const_iterator leaf = leaves.begin();
	    leaf != leaves.end(); ++leaf) {
	const ON_Interval interval[2] = {(*leaf)->m_u, (*leaf)->m_v};
	for (int direction = 0; direction < 2; ++direction) {
	    for (size_t split = 1;
		    split + 1 < spans[direction].size(); ++split) {
		const double knot = spans[direction][split];
		if (knot > interval[direction].Min() &&
			knot < interval[direction].Max()) {
		    std::printf("FAIL: leaf crosses direction %d knot %.17g "
			"in [%.17g %.17g]\n", direction, knot,
			interval[direction].Min(), interval[direction].Max());
		    return 1;
		}
	    }
	}
    }
    std::printf("SurfaceTree structural knots: spans=%d/%d leaves=%zu "
	"adaptive-depth=0\n", tree_surface->SpanCount(0),
	tree_surface->SpanCount(1), leaves.size());
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
