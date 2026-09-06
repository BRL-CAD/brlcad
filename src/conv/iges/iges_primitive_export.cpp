/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_writer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "brep.h"
#include "rt/geom.h"

namespace brlcad {
namespace iges {
namespace {

bool
perpendicular(const ON_3dVector &left, const ON_3dVector &right,
    const struct bn_tol &tolerance)
{
    const double product = left.Length() * right.Length();
    return product > 0.0 && std::fabs(ON_DotProduct(left, right)) <= tolerance.perp * product;
}

ExportedEntity
block(Writer &writer, const struct rt_arb_internal &arb, const std::string &name)
{
    const auto &tolerance = writer.options().tolerance;
    const ON_3dPoint origin(arb.pt[0]);
    ON_3dVector x = ON_3dPoint(arb.pt[1]) - origin;
    const ON_3dVector y = ON_3dPoint(arb.pt[3]) - origin;
    ON_3dVector z = ON_3dPoint(arb.pt[4]) - origin;
    if (!perpendicular(x, y, tolerance) || !perpendicular(x, z, tolerance) ||
	!perpendicular(y, z, tolerance))
	return {};
    const std::array<ON_3dPoint, 8> expected = {origin, origin + x, origin + x + y, origin + y,
	origin + z, origin + x + z, origin + x + y + z, origin + y + z};
    for (size_t vertex = 0; vertex < expected.size(); ++vertex)
	if (expected[vertex].DistanceTo(ON_3dPoint(arb.pt[vertex])) > tolerance.dist)
	    return {};
    if (ON_DotProduct(ON_CrossProduct(z, x), y) < 0.0)
	std::swap(x, z);
    ParameterWriter parameters(150);
    parameters.real(x.Length()).real(y.Length()).real(z.Length()).point(origin);
    if (!x.Unitize() || !z.Unitize())
	return {};
    parameters.point(x).point(z);
    return {writer.named_entity(EntitySpec(150), parameters, name), false};
}

ExportedEntity
cone(Writer &writer, const struct rt_tgc_internal &tgc, const std::string &name)
{
    const auto &tolerance = writer.options().tolerance;
    ON_3dVector axis(tgc.h);
    const ON_3dVector a(tgc.a), b(tgc.b), c(tgc.c), d(tgc.d);
    double start_radius = a.Length();
    double end_radius = c.Length();
    const double height = axis.Length();
    if (!axis.Unitize() || height <= 0.0 ||
	std::fabs(start_radius - b.Length()) > tolerance.dist ||
	std::fabs(end_radius - d.Length()) > tolerance.dist ||
	std::max(start_radius, end_radius) <= 0.0 ||
	(start_radius > 0.0 && (!perpendicular(axis, a, tolerance) || !perpendicular(axis, b, tolerance) ||
	    !perpendicular(a, b, tolerance))) ||
	(end_radius > 0.0 && (!perpendicular(axis, c, tolerance) || !perpendicular(axis, d, tolerance) ||
	    !perpendicular(c, d, tolerance))))
	return {};
    const bool cylinder = std::fabs(start_radius - end_radius) <= tolerance.dist;
    const int type = cylinder ? 154 : 156;
    ON_3dPoint base(tgc.v);
    if (start_radius < end_radius) {
	std::swap(start_radius, end_radius);
	base += ON_3dVector(tgc.h);
	axis = -axis;
    }
    ParameterWriter parameters(type);
    parameters.real(height).real(start_radius);
    if (!cylinder)
	parameters.real(end_radius);
    parameters.point(base).point(axis);
    return {writer.named_entity(EntitySpec(type), parameters, name), false};
}

ExportedEntity
ellipsoid(Writer &writer, const struct rt_ell_internal &ell, bool sphere,
    const std::string &name)
{
    ON_3dVector x(ell.a), y(ell.b), z(ell.c);
    if (x.Length() <= 0.0 || y.Length() <= 0.0 || z.Length() <= 0.0)
	return {};
    const int type = sphere ? 158 : 168;
    ParameterWriter parameters(type);
    parameters.real(x.Length());
    if (!sphere)
	parameters.real(y.Length()).real(z.Length());
    parameters.point(ell.v);
    if (!sphere) {
	const auto &tolerance = writer.options().tolerance;
	if (!perpendicular(x, y, tolerance) || !perpendicular(x, z, tolerance) ||
	    !perpendicular(y, z, tolerance) || !x.Unitize() || !z.Unitize())
	    return {};
	parameters.point(x).point(z);
    }
    return {writer.named_entity(EntitySpec(type), parameters, name), false};
}

bool
polyhedral(int type)
{
    // These callbacks already use NMG internally; adding OpenNURBS would
    // add representation overhead without improving the source geometry.
    switch (type) {
	case ID_ARB8: case ID_ARS: case ID_HALF: case ID_POLY: case ID_NMG:
	case ID_ARBN: case ID_HF: case ID_DSP: case ID_BOT: case ID_METABALL:
	    return true;
	default: return false;
    }
}
} // namespace

bool
supported_primitive(int type)
{
    if (polyhedral(type))
	return true;
    switch (type) {
	case ID_TOR: case ID_TGC: case ID_ELL: case ID_REC: case ID_BSPLINE:
	case ID_SPH: case ID_PIPE: case ID_PARTICLE: case ID_RPC: case ID_RHC:
	case ID_EPA: case ID_EHY: case ID_ETO: case ID_EXTRUDE: case ID_CLINE:
	case ID_SUPERELL: case ID_BREP: case ID_HYP: case ID_REVOLVE:
	    return true;
	default:
	    return false;
    }
}

ExportedEntity
export_primitive(Writer &writer, struct rt_db_internal &internal, const std::string &name)
{
    if (internal.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	!supported_primitive(internal.idb_type) || !internal.idb_ptr)
	return {};
    ExportedEntity result;
    switch (internal.idb_type) {
	case ID_ARB8:
	    result = block(writer, *static_cast<const struct rt_arb_internal *>(internal.idb_ptr), name);
	    break;
	case ID_TGC: case ID_REC:
	    result = cone(writer, *static_cast<const struct rt_tgc_internal *>(internal.idb_ptr), name);
	    break;
	case ID_ELL: case ID_SPH:
	    result = ellipsoid(writer, *static_cast<const struct rt_ell_internal *>(internal.idb_ptr),
		internal.idb_type == ID_SPH, name);
	    break;
	case ID_TOR: {
	    const auto *torus = static_cast<const struct rt_tor_internal *>(internal.idb_ptr);
	    // Type 160 requires a ring torus.  Horn and spindle geometries need
	    // the B-Rep or tessellation fallback, not an invalid native record.
	    if (torus->r_h <= 0.0 || torus->r_a <= torus->r_h)
		break;
	    ParameterWriter parameters(160);
	    parameters.real(torus->r_a).real(torus->r_h).point(torus->v).point(torus->h);
	    result = {writer.named_entity(EntitySpec(160), parameters, name), false};
	    break;
	}
	default: break;
    }
    if (result)
	return result;
    if (!polyhedral(internal.idb_type)) {
	result = export_brep(writer, internal, name);
	if (result)
	    return result;
    }
    return export_nmg(writer, internal, name);
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
