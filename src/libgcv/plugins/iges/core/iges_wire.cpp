/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_native.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "nmg.h"
#include "wdb.h"

namespace brlcad {
namespace iges {
namespace {
constexpr unsigned int MAX_WIRE_SUBDIVISION = 24;
constexpr size_t MAX_WIRE_POINTS = 1000000;
constexpr double WIRE_RELATIVE_TOLERANCE = 1.0e-5;
constexpr double WIRE_MINIMUM_TOLERANCE_MM = 1.0e-6;

bool
sample_bezier(const ON_BezierCurve &curve, double tolerance,
    std::vector<ON_3dPoint> &points, size_t &remaining, unsigned int depth = 0)
{
    if (!remaining)
	return false;
    const ON_Line chord(curve.PointAt(0.0), curve.PointAt(1.0));
    bool flat = true;
    for (int cv = 0; cv < curve.CVCount(); ++cv) {
	ON_3dPoint point;
	if (!curve.GetCV(cv, point) || curve.Weight(cv) <= 0.0)
	    return false;
	double parameter = 0.0;
	chord.ClosestPointTo(point, &parameter);
	const ON_3dPoint closest = chord.PointAt(std::clamp(parameter, 0.0, 1.0));
	flat = flat && point.DistanceTo(closest) <= tolerance;
    }
    if (flat) {
	points.push_back(chord.to);
	--remaining;
	return true;
    }
    if (depth >= MAX_WIRE_SUBDIVISION)
	return false;
    ON_BezierCurve left, right;
    return curve.Split(0.5, left, right) &&
	sample_bezier(left, tolerance, points, remaining, depth + 1) &&
	sample_bezier(right, tolerance, points, remaining, depth + 1);
}
} // namespace

bool
write_wire_curves(const Document &document, const DirectoryEntry &entry,
    struct rt_wdb *database, const std::string &name, bool project, std::string &error)
{
    ProfileCurves curves;
    if (!read_model_curves(document, entry.id, curves, error))
	return false;
    std::vector<std::vector<ON_3dPoint>> polylines;
    size_t remaining = MAX_WIRE_POINTS;
    for (const auto &curve : curves) {
	const ON_BoundingBox bounds = curve->BoundingBox();
	if (!bounds.IsValid()) {
	    error = "invalid wire curve bounds";
	    return false;
	}
	const double tolerance = std::max(WIRE_MINIMUM_TOLERANCE_MM,
	    bounds.Diagonal().Length() * WIRE_RELATIVE_TOLERANCE);
	for (int span = 0; span <= curve->CVCount() - curve->Order(); ++span) {
	    if (curve->Knot(span + curve->Order() - 2) >= curve->Knot(span + curve->Order() - 1))
		continue;
	    ON_BezierCurve bezier;
	    if (!curve->ConvertSpanToBezier(span, bezier)) {
		error = "cannot evaluate wire curve span";
		return false;
	    }
	    std::vector<ON_3dPoint> points = {bezier.PointAt(0.0)};
	    if (!remaining || !sample_bezier(bezier, tolerance, points, --remaining)) {
		error = "wire curve exceeded bounded subdivision limits";
		return false;
	    }
	    polylines.push_back(std::move(points));
	}
    }
    std::unique_ptr<struct model, decltype(&nmg_km)> model(nmg_mm(), nmg_km);
    struct nmgregion *region = nmg_mrsv(model.get());
    struct shell *shell = BU_LIST_FIRST(shell, &region->s_hd);
    size_t edge_count = 0;
    for (auto &points : polylines) {
	if (project)
	    for (auto &point : points)
		point.z = 0.0;
	for (size_t point = 1; point < points.size(); ++point) {
	    if (points[point - 1].DistanceTo(points[point]) <= ON_ZERO_TOLERANCE)
		continue;
	    struct edgeuse *edge = nmg_me(nullptr, nullptr, shell);
	    nmg_vertex_gv(edge->vu_p->v_p, points[point - 1]);
	    nmg_vertex_gv(edge->eumate_p->vu_p->v_p, points[point]);
	    ++edge_count;
	}
    }
    if (!edge_count) {
	error = "wire curve has no nondegenerate edges";
	return false;
    }
    if (mk_nmg(database, name.c_str(), model.release()) < 0) {
	error = "failed to write NMG wire geometry";
	return false;
    }
    return true;
}
} // namespace iges
} // namespace brlcad

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
