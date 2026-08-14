/*             B R E P _ C U R V E T R E E _ D I S P L A Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <list>
#include <vector>

#include "brep/curvetree.h"

static ON_NurbsCurve *
tiny_closed_trim()
{
    const double e = 1.0e-6;
    const std::vector<ON_3dPoint> points = {
	ON_3dPoint(0.0, 0.0, 0.0),
	ON_3dPoint(e, e, 0.0),
	ON_3dPoint(-e, e, 0.0),
	ON_3dPoint(0.0, 0.0, 0.0)
    };
    ON_NurbsCurve *curve = new ON_NurbsCurve(2, false, 4,
	(int)points.size());
    for (size_t i = 0; i < points.size(); i++)
	curve->SetCV((int)i, points[i]);
    curve->MakeClampedUniformKnotVector();
    curve->SetDomain(0.0, 1.0);
    return curve;
}

int
main()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetDomain(0, -1.0, 1.0);
    surface->SetDomain(1, -1.0, 1.0);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    ON_BrepFace &face = brep.NewFace(brep.AddSurface(surface));
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);

    ON_BrepVertex &vertex = brep.NewVertex(ON_3dPoint::Origin);
    ON_NurbsCurve *edge_curve = tiny_closed_trim();
    const int edge_curve_index = brep.AddEdgeCurve(edge_curve);
    ON_BrepEdge &edge = brep.NewEdge(vertex, vertex, edge_curve_index);
    ON_NurbsCurve *trim_curve = tiny_closed_trim();
    const int trim_curve_index = brep.AddTrimCurve(trim_curve);
    ON_BrepTrim &trim = brep.NewTrim(edge, false, loop,
	trim_curve_index);
    trim.m_type = ON_BrepTrim::boundary;
    trim.m_iso = ON_Surface::not_iso;

    const size_t budget = 5 * brlcad::BRNode::estimated_allocation_size();
    brlcad::CurveTree tree(&face, budget, 1.0e-4);
    if (tree.limit_reached())
	return 1;
    std::list<const brlcad::BRNode *> leaves;
    tree.getLeaves(leaves);
    if (leaves.empty())
	return 1;

    /* The same deliberately tight budget is insufficient when subdivision
     * has no model-space display tolerance. */
    brlcad::CurveTree untoleranced_tree(&face, budget);
    if (!untoleranced_tree.limit_reached())
	return 1;

    /* Display callers may assign each exact trim hierarchy a time share and
     * fall back to a bounded untrimmed cue when it is exhausted. */
    brlcad::CurveTree timed_tree(&face, 0, 0.0, 1);
    return timed_tree.limit_reached() && timed_tree.time_limit_reached() ?
	0 : 1;
}
