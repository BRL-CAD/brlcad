/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_native.h"
#include "iges_runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "bu/malloc.h"
#include "rt/geom.h"
#include "wdb.h"

namespace brlcad {
namespace iges {
namespace {

class Parameters {
public:
    explicit Parameters(const ParameterList *parameters) : parameters_(parameters) {}

    double real()
    {
	double value = 0.0;
	if (!parameters_ || next_ >= parameters_->values.size() ||
	    !parameters_->values[next_++].real(value) || !std::isfinite(value))
	    throw std::invalid_argument("missing or invalid numeric parameter");
	return value;
    }

    double optional(double fallback)
    {
	if (!parameters_ || next_ >= parameters_->values.size() ||
	    parameters_->values[next_].empty()) {
	    ++next_;
	    return fallback;
	}
	return real();
    }

    double positive(double scale)
    {
	const double value = real() * scale;
	if (!std::isfinite(value) || value <= 0.0)
	    throw std::invalid_argument("dimension must be finite and positive");
	return value;
    }

    EntityId reference()
    {
	EntityId id;
	if (!parameters_ || next_ >= parameters_->values.size() ||
	    !parameters_->values[next_++].entity(id) || !id.valid())
	    throw std::invalid_argument("missing or invalid profile reference");
	return id;
    }

    ON_3dPoint point(double scale)
    {
	ON_3dPoint value;
	for (int coordinate = 0; coordinate < 3; ++coordinate)
	    value[coordinate] = optional(0.0) * scale;
	if (!value.IsValid())
	    throw std::invalid_argument("invalid origin");
	return value;
    }

    ON_3dVector direction(const ON_3dVector &fallback = ON_zaxis)
    {
	ON_3dVector value;
	for (int coordinate = 0; coordinate < 3; ++coordinate)
	    value[coordinate] = optional(fallback[coordinate]);
	if (!value.IsValid() || !value.Unitize())
	    throw std::invalid_argument("invalid direction vector");
	return value;
    }

private:
    const ParameterList *parameters_;
    size_t next_ = 1;
};

ON_Plane
frame(Parameters &parameters, double scale)
{
    const ON_3dPoint origin = parameters.point(scale);
    const ON_3dVector x = parameters.direction(ON_xaxis);
    const ON_3dVector z = parameters.direction();
    const ON_3dVector y = ON_CrossProduct(z, x);
    const struct bn_tol tolerance = BN_TOL_INIT_TOL;
    if (std::fabs(ON_DotProduct(x, z)) > tolerance.perp + ON_EPSILON ||
	!y.IsValid() || y.Length() <= ON_ZERO_TOLERANCE)
	throw std::invalid_argument("primitive axes must be perpendicular");
    return ON_Plane(origin, x, y);
}

/** Sketch storage uses the library allocator because its destructor owns the
 * segment arrays.  Attach allocations immediately so exceptions remain safe. */
class Sketch {
public:
    explicit Sketch(const ON_Plane &plane)
    {
	internal_.value.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal_.value.idb_minor_type = ID_SKETCH;
	internal_.value.idb_meth = &OBJ[ID_SKETCH];
	struct rt_sketch_internal *sketch;
	BU_ALLOC(sketch, struct rt_sketch_internal);
	internal_.value.idb_ptr = sketch;
	sketch->magic = RT_SKETCH_INTERNAL_MAGIC;
	VMOVE(sketch->V, plane.origin);
	VMOVE(sketch->u_vec, plane.xaxis);
	VMOVE(sketch->v_vec, plane.yaxis);
    }

    struct rt_sketch_internal *get()
    {
	return static_cast<struct rt_sketch_internal *>(internal_.value.idb_ptr);
    }

    void build(const ProfileCurves &curves, const ON_Plane &plane, double tolerance)
    {
	auto *sketch = get();
	size_t vertices = 0;
	for (const auto &curve : curves)
	    vertices += static_cast<size_t>(curve->CVCount());
	if (vertices > static_cast<size_t>(std::numeric_limits<int>::max()))
	    throw std::invalid_argument("profile has too many control points");
	sketch->verts = static_cast<point2d_t *>(bu_calloc(vertices,
	    sizeof(point2d_t), "IGES sketch vertices"));
	sketch->vert_count = vertices;
	sketch->curve.segment = static_cast<void **>(bu_calloc(curves.size(),
	    sizeof(void *), "IGES sketch segments"));
	sketch->curve.reverse = static_cast<int *>(bu_calloc(curves.size(),
	    sizeof(int), "IGES sketch orientations"));
	size_t vertex = 0;
	for (const auto &curve : curves) {
	    const int start = static_cast<int>(vertex);
	    for (int cv = 0; cv < curve->CVCount(); ++cv, ++vertex) {
		ON_3dPoint point;
		if (!curve->GetCV(cv, point) ||
		    std::fabs(plane.DistanceTo(point)) > tolerance ||
		    !plane.ClosestPointTo(point, &sketch->verts[vertex][0],
			&sketch->verts[vertex][1]))
		    throw std::invalid_argument("profile is not planar in its sweep plane");
	    }
	    if (curve->Order() == 2 && curve->CVCount() == 2) {
		struct line_seg *segment;
		BU_ALLOC(segment, struct line_seg);
		sketch->curve.segment[sketch->curve.count++] = segment;
		segment->magic = CURVE_LSEG_MAGIC;
		segment->start = start;
		segment->end = start + 1;
		continue;
	    }
	    struct nurb_seg *segment;
	    BU_ALLOC(segment, struct nurb_seg);
	    sketch->curve.segment[sketch->curve.count++] = segment;
	    segment->magic = CURVE_NURB_MAGIC;
	    segment->order = curve->Order();
	    segment->c_size = curve->CVCount();
	    segment->pt_type = RT_NURB_MAKE_PT_TYPE(curve->IsRational() ? 3 : 2,
		RT_NURB_PT_XY, curve->IsRational());
	    segment->ctl_points = static_cast<int *>(bu_calloc(segment->c_size,
		sizeof(int), "IGES sketch control points"));
	    if (curve->IsRational())
		segment->weights = static_cast<fastf_t *>(bu_calloc(segment->c_size,
		    sizeof(fastf_t), "IGES sketch weights"));
	    for (int cv = 0; cv < segment->c_size; ++cv) {
		segment->ctl_points[cv] = start + cv;
		if (segment->weights)
		    segment->weights[cv] = curve->Weight(cv);
	    }
	    // OpenNURBS omits the two redundant end knots retained by sketches.
	    segment->k.k_size = curve->KnotCount() + 2;
	    segment->k.knots = static_cast<fastf_t *>(bu_calloc(segment->k.k_size,
		sizeof(fastf_t), "IGES sketch knots"));
	    segment->k.knots[0] = curve->Knot(0);
	    for (int knot = 0; knot < curve->KnotCount(); ++knot)
		segment->k.knots[knot + 1] = curve->Knot(knot);
	    segment->k.knots[segment->k.k_size - 1] = curve->Knot(curve->KnotCount() - 1);
	}
    }

private:
    Internal internal_;
};

void
append_line(ProfileCurves &curves, const ON_3dPoint &start,
    const ON_3dPoint &end, double tolerance)
{
    if (start.DistanceTo(end) <= tolerance)
	return;
    auto line = std::make_unique<ON_NurbsCurve>();
    if (!ON_LineCurve(start, end).GetNurbForm(*line))
	throw std::invalid_argument("cannot construct profile closing segment");
    curves.push_back(std::move(line));
}

bool
linear_revolution(struct rt_wdb *database, const std::string &name,
    const ON_NurbsCurve &curve, const ON_Plane &plane, double fraction, double tolerance)
{
    ON_3dPoint start = curve.PointAtStart();
    ON_3dPoint end = curve.PointAtEnd();
    if (std::fabs(plane.DistanceTo(start)) > tolerance ||
	std::fabs(plane.DistanceTo(end)) > tolerance)
	throw std::invalid_argument("revolution profile is not coplanar with its axis");
    double bottom = ON_DotProduct(start - plane.origin, plane.yaxis);
    double top = ON_DotProduct(end - plane.origin, plane.yaxis);
    if (top < bottom) {
	std::swap(bottom, top);
	std::swap(start, end);
    }
    const double first_radius = ON_DotProduct(start - plane.origin, plane.xaxis);
    const double last_radius = ON_DotProduct(end - plane.origin, plane.xaxis);
    if (top - bottom <= tolerance || first_radius < 0.0 || last_radius < 0.0 ||
	std::max(first_radius, last_radius) <= tolerance)
	throw std::invalid_argument("degenerate or axis-crossing revolution profile");
    const ON_3dPoint base = plane.origin + bottom * plane.yaxis;
    const bool full = fraction >= 1.0 - ON_ZERO_TOLERANCE;
    const auto auxiliary_name = [&](const std::string &suffix) {
	const std::string stem = name + suffix;
	std::string candidate = stem;
	for (size_t index = 1; db_lookup(database->dbip, candidate.c_str(), LOOKUP_QUIET); ++index)
	    candidate = stem + '_' + std::to_string(index);
	return candidate;
    };
    const std::string body = full ? name : auxiliary_name(".body");
    if (mk_cone(database, body.c_str(), base, plane.yaxis, top - bottom,
	first_radius, last_radius) < 0)
	return false;
    if (full)
	return true;

    const bool subtract_sector = fraction > 0.5;
    const double angle = 2.0 * ON_PI * (subtract_sector ? 1.0 - fraction : fraction);
    const double rotation = subtract_sector ? 2.0 * ON_PI * fraction : 0.0;
    const auto quadrant_value = [](double value) {
	// Exact quadrant boundaries must not drift across coincident CSG faces.
	if (std::fabs(value) <= ON_ZERO_TOLERANCE)
	    return 0.0;
	if (std::fabs(std::fabs(value) - 1.0) <= ON_ZERO_TOLERANCE)
	    return std::copysign(1.0, value);
	return value;
    };
    const ON_3dVector transverse = ON_CrossProduct(plane.yaxis, plane.xaxis);
    const ON_3dVector x = quadrant_value(std::cos(rotation)) * plane.xaxis +
	quadrant_value(std::sin(rotation)) * transverse;
    const ON_3dVector y = ON_CrossProduct(plane.yaxis, x);
    constexpr double CLIPPING_MARGIN = 2.0;
    const double radius = CLIPPING_MARGIN * std::max(first_radius, last_radius);
    std::array<ON_3dPoint, 4> corners;
    if (std::fabs(angle - ON_PI) <= ON_ZERO_TOLERANCE) {
	corners = {base - radius * x, base + radius * x,
	    base + radius * x + radius * y, base - radius * x + radius * y};
    } else {
	const double reach = radius / std::cos(angle * 0.5);
	const ON_3dPoint far_end = base + reach *
	    (quadrant_value(std::cos(angle)) * x + quadrant_value(std::sin(angle)) * y);
	corners = {base, base + reach * x,
	    far_end, far_end};
    }
    std::array<fastf_t, 24> coordinates;
    for (size_t index = 0; index < corners.size(); ++index) {
	VMOVE(coordinates.data() + 3 * index, corners[index]);
	const ON_3dPoint upper = corners[index] + (top - bottom) * plane.yaxis;
	VMOVE(coordinates.data() + 3 * (index + 4), upper);
    }
    const std::string clip = auxiliary_name(".clip");
    if (mk_arb8(database, clip.c_str(), coordinates.data()) < 0)
	return false;
    struct wmember members;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember(body.c_str(), &members.l, nullptr, WMOP_UNION) ||
	!mk_addmember(clip.c_str(), &members.l, nullptr,
	    subtract_sector ? WMOP_SUBTRACT : WMOP_INTERSECT)) {
	mk_freemembers(&members.l);
	return false;
    }
    const int status = mk_lfcomb(database, name.c_str(), &members, 0)
    return status == 0;
}

bool
sweep(const DirectoryEntry &entry, Parameters &parameters, struct rt_wdb *database,
    const std::string &name, double scale, double tolerance, const ProfileReader &read_profile,
    const NativeBrepWriter &write_brep)
{
    const EntityId profile_id = parameters.reference();
    const bool revolve = entry.type == 162;
    const double extent = revolve ? parameters.optional(1.0) : parameters.positive(scale);
    if (!std::isfinite(extent) || extent <= 0.0 || (revolve && extent > 1.0))
	throw std::invalid_argument("invalid sweep extent");
    const ON_3dPoint origin = revolve ? parameters.point(scale) : ON_origin;
    const ON_3dVector axis = parameters.direction();
    ProfileCurves curves;
    if (!read_profile(profile_id, curves) || curves.empty())
	throw std::invalid_argument("unsupported or invalid sweep profile");
    for (size_t index = 1; index < curves.size(); ++index)
	if (curves[index - 1]->PointAtEnd().DistanceTo(curves[index]->PointAtStart()) > tolerance)
	    throw std::invalid_argument("disconnected sweep profile");

    ON_Plane plane;
    if (revolve) {
	ON_3dVector radial;
	for (const auto &curve : curves) {
	    for (int cv = 0; cv < curve->CVCount(); ++cv) {
		ON_3dPoint point;
		if (!curve->GetCV(cv, point))
		    throw std::invalid_argument("invalid sweep control point");
		const ON_3dVector offset = point - origin;
		radial = offset - ON_DotProduct(offset, axis) * axis;
		if (radial.Length() > tolerance)
		    break;
	    }
	    if (radial.Length() > tolerance)
		break;
	}
	if (!radial.Unitize())
	    throw std::invalid_argument("revolution profile lies on its axis");
	plane = ON_Plane(origin, radial, axis);
	if (entry.form == 0 && curves.size() == 1 && curves.front()->IsLinear(tolerance))
	    return linear_revolution(database, name, *curves.front(), plane, extent, tolerance);
	if (entry.form == 0) {
	    const ON_3dPoint start = curves.front()->PointAtStart();
	    const ON_3dPoint end = curves.back()->PointAtEnd();
	    const ON_3dPoint axis_start = origin + ON_DotProduct(start - origin, axis) * axis;
	    const ON_3dPoint axis_end = origin + ON_DotProduct(end - origin, axis) * axis;
	    append_line(curves, end, axis_end, tolerance);
	    append_line(curves, axis_end, axis_start, tolerance);
	    append_line(curves, axis_start, start, tolerance);
	} else if (entry.form == 1) {
	    append_line(curves, curves.back()->PointAtEnd(), curves.front()->PointAtStart(), tolerance);
	} else {
	    throw std::invalid_argument("unsupported revolution form");
	}
    } else {
	ON_PolyCurve profile;
	for (const auto &curve : curves) {
	    auto duplicate = std::unique_ptr<ON_Curve>(curve->DuplicateCurve());
	    if (!duplicate || !profile.Append(duplicate.get()))
		throw std::invalid_argument("cannot assemble sweep profile");
	    duplicate.release();
	}
	if (!profile.IsPlanar(&plane, tolerance) ||
	    std::fabs(ON_DotProduct(plane.zaxis, axis)) <= ON_ZERO_TOLERANCE)
	    throw std::invalid_argument("extrusion needs a planar profile transverse to its direction");
	ON_Arc arc;
	if (curves.size() == 1 && curves.front()->IsArc(&plane, &arc, tolerance) && arc.IsCircle()) {
	    // Canonical circles have symmetric controls.  Use their exact center
	    // only when it agrees with the fit to floating-point precision;
	    // an arbitrary rational parameterization need not be symmetric.
	    const ON_3dPoint candidate = curves.front()->BoundingBox().Center();
	    const double roundoff = ON_ZERO_TOLERANCE * std::max(1.0, arc.Radius());
	    const ON_3dPoint center = candidate.DistanceTo(arc.Center()) <= roundoff ? candidate : arc.Center();
	    const ON_Plane circle_plane(center, arc.Normal());
	    const double radius = curves.front()->PointAtStart().DistanceTo(center);
	    const ON_3dVector height = extent * axis;
	    const ON_3dVector a = radius * circle_plane.xaxis;
	    const ON_3dVector b = radius * circle_plane.yaxis;
	    return mk_tgc(database, name.c_str(), center, height, a, b, a, b) == 0;
	}
    }
    if (curves.back()->PointAtEnd().DistanceTo(curves.front()->PointAtStart()) > tolerance)
	throw std::invalid_argument("sweep profile is not closed");

    Sketch sketch(plane);
    sketch.build(curves, plane, tolerance);
    // Curved revolution ray support is incomplete.  Use the exact primitive
    // B-Rep callback, without creating a dangling auxiliary sketch object.
    struct rt_db_internal primitive;
    RT_DB_INTERNAL_INIT(&primitive);
    struct rt_revolve_internal revolution = {};
    struct rt_extrude_internal extrusion = {};
    if (revolve) {
	revolution.magic = RT_REVOLVE_INTERNAL_MAGIC;
	VMOVE(revolution.v3d, origin);
	VMOVE(revolution.axis3d, axis);
	revolution.ang = extent * 2.0 * ON_PI;
	revolution.skt = sketch.get();
	primitive.idb_minor_type = ID_REVOLVE;
	primitive.idb_ptr = &revolution;
    } else {
	extrusion.magic = RT_EXTRUDE_INTERNAL_MAGIC;
	VMOVE(extrusion.V, plane.origin);
	VSCALE(extrusion.h, axis, extent);
	VMOVE(extrusion.u_vec, plane.xaxis);
	VMOVE(extrusion.v_vec, plane.yaxis);
	extrusion.skt = sketch.get();
	primitive.idb_minor_type = ID_EXTRUDE;
	primitive.idb_ptr = &extrusion;
    }
    primitive.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    primitive.idb_meth = &OBJ[primitive.idb_minor_type];
    struct bn_tol geometry_tolerance = BN_TOL_INIT_TOL;
    geometry_tolerance.dist = tolerance;
    geometry_tolerance.dist_sq = tolerance * tolerance;
    const auto brep = primitive_brep(primitive, geometry_tolerance);
    if (!brep || !brep->IsValid() || !brep->IsSolid())
	throw std::invalid_argument("sweep did not produce a valid closed B-Rep");
    return write_brep(name, *brep);
}

} // namespace

const char *
native_solid_name(int type)
{
    switch (type) {
	case 150: return "block";
	case 152: return "wedge";
	case 154: return "cyl";
	case 156: return "cone";
	case 158: return "sphere";
	case 160: return "torus";
	case 162: return "revolution";
	case 164: return "extrusion";
	case 168: return "ell";
	default: return nullptr;
    }
}

bool
write_native_solid(const Document &document, const DirectoryEntry &entry,
    struct rt_wdb *database, const std::string &name, double unit_to_mm,
    double tolerance, const ProfileReader &read_profile,
    const NativeBrepWriter &write_brep, std::string &error)
{
    try {
	Parameters parameters(document.parameters(entry.id));
	switch (entry.type) {
	    case 150:
	    case 152: {
		const double x = parameters.positive(unit_to_mm);
		const double y = parameters.positive(unit_to_mm);
		const double z = parameters.positive(unit_to_mm);
		const double top = entry.type == 152 ? parameters.real() * unit_to_mm : x;
		if (!std::isfinite(top) || top < 0.0 || (entry.type == 152 && top >= x))
		    throw std::invalid_argument("invalid wedge top length");
		const ON_Plane plane = frame(parameters, unit_to_mm);
		std::array<ON_3dPoint, 8> points;
		points[0] = plane.origin;
		points[1] = plane.PointAt(x, 0.0);
		points[2] = plane.PointAt(top, y);
		points[3] = plane.PointAt(0.0, y);
		for (size_t index = 0; index < 4; ++index)
		    points[index + 4] = points[index] + z * plane.zaxis;
		std::array<fastf_t, 24> coordinates;
		for (size_t index = 0; index < points.size(); ++index)
		    VMOVE(coordinates.data() + 3 * index, points[index]);
		return mk_arb8(database, name.c_str(), coordinates.data()) == 0;
	    }
	    case 154:
	    case 156: {
		const double height = parameters.positive(unit_to_mm);
		const double radius = parameters.positive(unit_to_mm);
		const double end_radius = entry.type == 156 ? parameters.optional(0.0) * unit_to_mm : radius;
		if (!std::isfinite(end_radius) || end_radius < 0.0)
		    throw std::invalid_argument("invalid cone end radius");
		const ON_3dPoint base = parameters.point(unit_to_mm);
		const ON_3dVector axis = parameters.direction();
		return mk_cone(database, name.c_str(), base, axis, height, radius, end_radius) == 0;
	    }
	    case 158: {
		const double radius = parameters.positive(unit_to_mm);
		const ON_3dPoint center = parameters.point(unit_to_mm);
		return mk_sph(database, name.c_str(), center, radius) == 0;
	    }
	    case 160: {
		const double major = parameters.positive(unit_to_mm);
		const double minor = parameters.positive(unit_to_mm);
		if (minor >= major)
		    throw std::invalid_argument("torus minor radius must be smaller than its major radius");
		const ON_3dPoint center = parameters.point(unit_to_mm);
		const ON_3dVector axis = parameters.direction();
		return mk_tor(database, name.c_str(), center, axis, major, minor) == 0;
	    }
	    case 168: {
		const double x = parameters.positive(unit_to_mm);
		const double y = parameters.positive(unit_to_mm);
		const double z = parameters.positive(unit_to_mm);
		const ON_Plane plane = frame(parameters, unit_to_mm);
		return mk_ell(database, name.c_str(), plane.origin, x * plane.xaxis,
		    y * plane.yaxis, z * plane.zaxis) == 0;
	    }
	    case 162:
	    case 164:
		return sweep(entry, parameters, database, name, unit_to_mm, tolerance, read_profile, write_brep);
	    default:
		error = "unsupported native solid type";
		return false;
	}
    } catch (const std::invalid_argument &invalid) {
	error = invalid.what();
	return false;
    }
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
