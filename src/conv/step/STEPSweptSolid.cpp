/*                       S T E P S W E P T S O L I D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "STEPSweptSolid.h"
#include "BRLCADWrapper.h"
#include "STEPGeneratedAPI.h"
#include "STEPString.h"
#include "STEPWrapper.h"
#include "Curve.h"
#include "FaceSurface.h"
#include "Factory.h"
#include "GlobalUnitAssignedContext.h"
#include "GlobalUncertaintyAssignedContext.h"
#include "LocalUnits.h"

#include "vmath.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

namespace {

STEPentity *
swept_product_definition(STEPWrapper &wrapper, STEPentity *selected)
{
    if (selected && wrapper.IsSchemaEntity(selected, "PRODUCT_DEFINITION"))
	return selected;
    if (selected && wrapper.IsSchemaEntity(selected, "PRODUCT_DEFINITION_SHAPE"))
	return swept_product_definition(wrapper,
	    dynamic_cast<STEPentity *>(brlcad::step::Entity(selected, "definition")));
    return NULL;
}

int64_t
swept_product_id(STEPentity *definition)
{
    STEPentity *formation = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(definition, "formation"));
    STEPentity *product = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(formation, "of_product"));
    return product ? product->STEPfile_id : 0;
}

bool
swept_point_coordinates(STEPentity *point, double *coordinates)
{
    STEPaggregate *values = brlcad::step::Aggregate(point, "coordinates");
    RealNode *node = values ? static_cast<RealNode *>(values->GetHead()) : NULL;
    size_t count = 0;
    while (node && count < 3) {
	coordinates[count++] = node->value;
	node = static_cast<RealNode *>(node->NextNode());
    }
    while (count < 3) coordinates[count++] = 0.0;
    return values != NULL;
}

bool
swept_direction_ratios(STEPentity *direction, double *ratios)
{
    STEPaggregate *values = brlcad::step::Aggregate(direction, "direction_ratios");
    RealNode *node = values ? static_cast<RealNode *>(values->GetHead()) : NULL;
    size_t count = 0;
    while (node && count < 3) {
	ratios[count++] = node->value;
	node = static_cast<RealNode *>(node->NextNode());
    }
    while (count < 3) ratios[count++] = 0.0;
    if (!values || MAGNITUDE(ratios) <= SMALL_FASTF) return false;
    VUNITIZE(ratios);
    return true;
}

bool
swept_axis2_placement(STEPentity *placement, double length,
    double *origin, double *xaxis, double *yaxis, double *zaxis)
{
    STEPentity *location = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(placement, "location"));
    if (!placement || !swept_point_coordinates(location, origin)) return false;
    VSCALE(origin, origin, length);
    VSET(zaxis, 0.0, 0.0, 1.0);
    VSET(xaxis, 1.0, 0.0, 0.0);
    STEPentity *axis = dynamic_cast<STEPentity *>(brlcad::step::Entity(placement, "axis"));
    STEPentity *reference = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(placement, "ref_direction"));
    if (axis && !swept_direction_ratios(axis, zaxis)) return false;
    if (reference && !swept_direction_ratios(reference, xaxis)) return false;
    VJOIN1(xaxis, xaxis, -(VDOT(xaxis, zaxis)), zaxis);
    if (MAGNITUDE(xaxis) <= SMALL_FASTF) return false;
    VUNITIZE(xaxis);
    VCROSS(yaxis, zaxis, xaxis);
    if (MAGNITUDE(yaxis) <= SMALL_FASTF) return false;
    VUNITIZE(yaxis);
    return true;
}

bool
swept_axis1_placement(STEPentity *placement, double length,
    double *origin, double *axis)
{
    STEPentity *location = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(placement, "location"));
    if (!placement || !swept_point_coordinates(location, origin)) return false;
    VSCALE(origin, origin, length);
    VSET(axis, 0.0, 0.0, 1.0);
    STEPentity *direction = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(placement, "axis"));
    return !direction || swept_direction_ratios(direction, axis);
}

bool
swept_axis1_line(STEPentity *placement, double length, ON_Line &line)
{
    double origin[3], direction[3];
    if (!swept_axis1_placement(placement, length, origin, direction))
	return false;
    line.from = ON_3dPoint(origin);
    line.to = line.from + ON_3dVector(direction);
    return line.IsValid() && line.Direction().Length() > ON_ZERO_TOLERANCE;
}

struct UnitFactors {
    double length = 1000.0;
    double plane_angle = 1.0;
    double solid_angle = 1.0;
    double tolerance = 1.0e-6;
};

UnitFactors
representation_units(STEPWrapper &wrapper, STEPentity *representation)
{
    UnitFactors result;
    result.tolerance = LocalUnits::tolerance;
    STEPentity *context = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(representation, "context_of_items"));
    if (!context) return result;
    GlobalUnitAssignedContext units(&wrapper, context->STEPfile_id);
    if (!units.Load(&wrapper, context)) return result;
    const double length = units.GetLengthConversionFactor();
    const double plane_angle = units.GetPlaneAngleConversionFactor();
    const double solid_angle = units.GetSolidAngleConversionFactor();
    if (length > 0.0) result.length = length;
    if (plane_angle > 0.0) result.plane_angle = plane_angle;
    if (solid_angle > 0.0) result.solid_angle = solid_angle;
    if (wrapper.ImportOptions().absolute_tolerance_mm <= 0.0) {
	GlobalUncertaintyAssignedContext uncertainty(&wrapper,
	    context->STEPfile_id);
	if (uncertainty.Load(&wrapper, context)) {
	    const double value = uncertainty.GetLengthUncertainty();
	    if (value > 0.0)
		result.tolerance = std::max(1.0e-9, value);
	}
    }
    return result;
}

void
apply_units(const UnitFactors &units)
{
    LocalUnits::length = units.length;
    LocalUnits::planeangle = units.plane_angle;
    LocalUnits::solidangle = units.solid_angle;
    LocalUnits::tolerance = units.tolerance;
    LocalUnits::representation_tolerance = units.tolerance;
}

void
representation_matrix(STEPWrapper &wrapper, STEPentity *representation,
    double length, mat_t matrix)
{
    MAT_IDN(matrix);
	const std::vector<SDAI_Application_instance *> items =
	    brlcad::step::Entities(representation, "items");
    for (std::vector<SDAI_Application_instance *>::const_iterator i = items.begin();
	 i != items.end(); ++i) {
	STEPentity *placement = dynamic_cast<STEPentity *>(*i);
	if (placement && wrapper.IsSchemaEntity(placement, "AXIS2_PLACEMENT_3D")) {
	    double origin[3], xaxis[3], yaxis[3], zaxis[3];
	    if (swept_axis2_placement(placement, length, origin, xaxis, yaxis, zaxis)) {
		mat_t rotation;
		MAT_IDN(rotation);
		VMOVE(&rotation[0], xaxis);
		VMOVE(&rotation[4], yaxis);
		VMOVE(&rotation[8], zaxis);
		bn_mat_inv(matrix, rotation);
		MAT_DELTAS_VEC(matrix, origin);
	    }
	    return;
	}
    }
}

bool
append_boundary_curves(STEPWrapper &wrapper, STEPentity *boundary,
    ON_Brep &curve_holder, const ON_Plane &plane, std::vector<ON_Curve *> &curves,
    std::string &reason)
{
    if (!boundary || wrapper.getLogicalAttribute(boundary, "self_intersect") == LTrue ||
	brlcad::step::Entities(boundary, "segments").empty()) {
	reason = "profile boundary is absent, self-intersecting, or has no segments";
	return false;
    }
	const std::vector<SDAI_Application_instance *> segments =
	    brlcad::step::Entities(boundary, "segments");
    for (std::vector<SDAI_Application_instance *>::const_iterator i = segments.begin();
	 i != segments.end(); ++i) {
	STEPentity *segment = dynamic_cast<STEPentity *>(*i);
	STEPentity *source = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(segment, "parent_curve"));
	/* A boundary_curve normally references bounded SURFACE_CURVE complex
	 * instances.  The verified 3D component is the authoritative curve for
	 * constructing the exact planar profile; pcurves remain available to the
	 * general face converter but are not needed for a plane extrusion. */
	std::string component_names;
	if (source) {
	    MAP_OF_SUPERTYPES components;
	    wrapper.getSuperTypes(source->STEPfile_id, components);
	    for (MAP_OF_SUPERTYPES::const_iterator component = components.begin();
		 component != components.end(); ++component) {
		if (!component_names.empty()) component_names += ",";
		component_names += component->first;
	    }
	    MAP_OF_SUPERTYPES::const_iterator surface = components.find("Surface_Curve");
	    if (surface != components.end()) {
		SDAI_Application_instance *curve_3d =
		    wrapper.getEntityAttribute(surface->second, "curve_3d");
		STEPentity *resolved_curve = dynamic_cast<STEPentity *>(curve_3d);
		if (resolved_curve) source = resolved_curve;
	    }
	}
	STEPEntity *factory_object = source ? Factory::CreateObject(&wrapper, source) : NULL;
	Curve *curve = dynamic_cast<Curve *>(factory_object);
	if (!factory_object) {
	    reason = "curve factory rejected profile boundary #" +
		std::to_string(source ? source->STEPfile_id : 0) + " (" +
		(source && source->EntityName() ? source->EntityName() : "unknown") +
		(component_names.empty() ? ")" : "; components=" + component_names + ")");
	    return false;
	}
	if (!curve) {
	    reason = "profile boundary reference resolved to a non-curve factory object";
	    return false;
	}
	if (!curve->LoadONBrep(&curve_holder)) {
	    reason = "could not materialize a profile boundary curve";
	    return false;
	}
	const int curve_id = curve->GetONId();
	if (curve_id < 0 || curve_id >= curve_holder.m_C3.Count() || !curve_holder.m_C3[curve_id]) {
	    reason = "profile boundary curve did not produce a 3D curve";
	    return false;
	}
	ON_Curve *copy = curve_holder.m_C3[curve_id]->DuplicateCurve();
	if (!copy || !copy->IsInPlane(plane, LocalUnits::tolerance)) {
	    delete copy;
	    reason = "profile boundary curve is not in its asserted plane";
	    return false;
	}
	if (wrapper.getBooleanAttribute(segment, "same_sense") != BTrue &&
	    !copy->Reverse()) {
	    delete copy;
	    reason = "profile boundary curve orientation could not be reversed";
	    return false;
	}
	curves.push_back(copy);
    }
    if (curves.empty()) return false;

    for (size_t index = 0; index < curves.size(); ++index) {
	ON_Curve *first = curves[index];
	ON_Curve *second = curves[(index + 1) % curves.size()];
	const ON_3dPoint end = first->PointAtEnd();
	const ON_3dPoint start = second->PointAtStart();
	const double gap = end.DistanceTo(start);
	if (gap > LocalUnits::tolerance) {
	    reason = "profile boundary gap exceeds the model tolerance";
	    return false;
	}
	if (gap > ON_ZERO_TOLERANCE &&
	    wrapper.ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	    const ON_3dPoint midpoint = 0.5 * (end + start);
	    if (!first->SetEndPoint(midpoint) || !second->SetStartPoint(midpoint)) {
		reason = "profile boundary endpoints could not be snapped safely";
		return false;
	    }
	    wrapper.RecordRepair(boundary->STEPfile_id, "BOUNDARY_CURVE", "segments",
		"snapped swept profile endpoint within model tolerance");
	}
    }
    return true;
}

ON_Brep *
planar_profile(STEPWrapper &wrapper, STEPentity *profile, double length,
    std::string &reason)
{
    const std::vector<SDAI_Application_instance *> boundary_entities =
	brlcad::step::Entities(profile, "boundaries");
    if (!profile || wrapper.getBooleanAttribute(profile, "implicit_outer") == BTrue ||
	boundary_entities.empty()) {
	reason = "swept profile requires explicit planar boundaries";
	return NULL;
    }
    STEPentity *basis = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(profile, "basis_surface"));
    STEPentity *position = basis && wrapper.IsSchemaEntity(basis, "PLANE") ?
	dynamic_cast<STEPentity *>(brlcad::step::Entity(basis, "position")) : NULL;
    double origin[3], xaxis[3], yaxis[3], zaxis[3];
    if (!swept_axis2_placement(position, length, origin, xaxis, yaxis, zaxis)) {
	reason = "swept profile basis is not a valid placed plane";
	return NULL;
    }
    const ON_Plane plane{ON_3dPoint(origin), ON_3dVector(xaxis), ON_3dVector(yaxis)};

    wrapper.ResetOpenNURBSState();
    ON_Brep curve_holder;
    std::vector<std::vector<ON_Curve *> > boundaries;
    std::vector<bool> outer_boundaries;
    for (std::vector<SDAI_Application_instance *>::const_iterator i =
	 boundary_entities.begin(); i != boundary_entities.end(); ++i) {
	STEPentity *boundary = dynamic_cast<STEPentity *>(*i);
	boundaries.push_back(std::vector<ON_Curve *>());
	outer_boundaries.push_back(boundary &&
	    wrapper.IsSchemaEntity(boundary, "OUTER_BOUNDARY_CURVE"));
	if (!append_boundary_curves(wrapper, boundary, curve_holder, plane, boundaries.back(), reason)) {
	    for (std::vector<std::vector<ON_Curve *> >::iterator loop = boundaries.begin();
		 loop != boundaries.end(); ++loop)
		for (std::vector<ON_Curve *>::iterator curve = loop->begin(); curve != loop->end(); ++curve)
		    delete *curve;
	    return NULL;
	}
    }
    if (boundaries.empty()) {
	reason = "swept profile has no boundary loops";
	return NULL;
    }
    size_t outer_count = 0;
    size_t outer_index = 0;
    for (size_t index = 0; index < outer_boundaries.size(); ++index) {
	if (outer_boundaries[index]) {
	    ++outer_count;
	    outer_index = index;
	}
    }
    if (outer_count != 1) {
	for (std::vector<std::vector<ON_Curve *> >::iterator loop = boundaries.begin();
	     loop != boundaries.end(); ++loop)
	    for (std::vector<ON_Curve *>::iterator curve = loop->begin(); curve != loop->end(); ++curve)
		delete *curve;
	reason = "explicit swept profile must contain exactly one outer boundary";
	return NULL;
    }

    ON_Brep *brep = new ON_Brep();
    ON_PlaneSurface *surface = new ON_PlaneSurface(plane);
    ON_BoundingBox bbox;
    for (std::vector<std::vector<ON_Curve *> >::const_iterator loop = boundaries.begin();
	 loop != boundaries.end(); ++loop)
	for (std::vector<ON_Curve *>::const_iterator curve = loop->begin(); curve != loop->end(); ++curve)
	    bbox.Union((*curve)->BoundingBox());
    double umin = 0.0, umax = 1.0, vmin = 0.0, vmax = 1.0;
    if (bbox.IsValid()) {
	bool first = true;
	for (unsigned int corner = 0; corner < 8; ++corner) {
	    const ON_3dPoint point((corner & 1) ? bbox.m_max.x : bbox.m_min.x,
		(corner & 2) ? bbox.m_max.y : bbox.m_min.y,
		(corner & 4) ? bbox.m_max.z : bbox.m_min.z);
	    double u = 0.0, v = 0.0;
	    if (!plane.ClosestPointTo(point, &u, &v)) continue;
	    if (first) {
		umin = umax = u;
		vmin = vmax = v;
		first = false;
	    } else {
		umin = std::min(umin, u);
		umax = std::max(umax, u);
		vmin = std::min(vmin, v);
		vmax = std::max(vmax, v);
	    }
	}
	umin -= LocalUnits::tolerance;
	umax += LocalUnits::tolerance;
	vmin -= LocalUnits::tolerance;
	vmax += LocalUnits::tolerance;
    }
    surface->SetDomain(0, umin, umax);
    surface->SetDomain(1, vmin, vmax);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    const int surface_id = brep->AddSurface(surface);
    ON_BrepFace &face = brep->NewFace(surface_id);
    bool success = true;

    /* SET ordering is not significant in STEP.  Construct the outer loop
     * first, followed by all inner loops in stable input order. */
    std::vector<size_t> loop_order;
    loop_order.push_back(outer_index);
    for (size_t index = 0; index < boundaries.size(); ++index)
	if (index != outer_index) loop_order.push_back(index);
    for (std::vector<size_t>::const_iterator ordered_index = loop_order.begin();
	 ordered_index != loop_order.end(); ++ordered_index) {
	std::vector<ON_Curve *> &loop = boundaries[*ordered_index];
	ON_SimpleArray<ON_Curve *> curves;
	for (std::vector<ON_Curve *>::iterator curve = loop.begin(); curve != loop.end(); ++curve)
	    curves.Append(*curve);
	const ON_BrepLoop::TYPE loop_type = outer_boundaries[*ordered_index] ?
	    ON_BrepLoop::outer : ON_BrepLoop::inner;
	if (!brep->NewPlanarFaceLoop(face.m_face_index, loop_type, curves, true)) {
	    success = false;
	    reason = "openNURBS could not construct a planar profile loop";
	}
	for (int index = 0; index < curves.Count(); ++index) delete curves[index];
	loop.clear();
	if (!success) break;
    }
    if (!success) {
	for (std::vector<std::vector<ON_Curve *> >::iterator loop = boundaries.begin();
	     loop != boundaries.end(); ++loop)
	    for (std::vector<ON_Curve *>::iterator curve = loop->begin(); curve != loop->end(); ++curve)
		delete *curve;
	delete brep;
	return NULL;
    }
    brep->SetTrimIsoFlags(face);
    return brep;
}

struct RevolvedProfileLoop {
    ON_NurbsCurve *curve;
};

void
delete_profile_loops(std::vector<RevolvedProfileLoop> &loops)
{
    for (std::vector<RevolvedProfileLoop>::iterator loop = loops.begin();
	 loop != loops.end(); ++loop)
	delete loop->curve;
    loops.clear();
}

bool
profile_loop_curves(const ON_Brep &profile,
	std::vector<RevolvedProfileLoop> &loops, size_t &outer_index,
	std::string &reason)
{
    loops.clear();
    outer_index = 0;
    if (profile.m_F.Count() != 1 || profile.m_F[0].LoopCount() < 1) {
	reason = "revolved profile does not contain one bounded planar face";
	return false;
    }

    size_t outer_count = 0;
    for (int loop_index = 0; loop_index < profile.m_F[0].LoopCount();
	 ++loop_index) {
	const ON_BrepLoop *loop = profile.m_F[0].Loop(loop_index);
	if (!loop || (loop->m_type != ON_BrepLoop::outer &&
		loop->m_type != ON_BrepLoop::inner) || loop->TrimCount() < 1) {
	    delete_profile_loops(loops);
	    reason = "revolved profile contains an unusable boundary loop";
	    return false;
	}
	ON_PolyCurve *polycurve = new ON_PolyCurve(loop->TrimCount());
	for (int trim_index = 0; trim_index < loop->TrimCount(); ++trim_index) {
	    const ON_BrepTrim *trim = loop->Trim(trim_index);
	    const ON_Curve *edge_curve = trim ? trim->EdgeCurveOf() : NULL;
	    ON_Curve *copy = edge_curve ? edge_curve->DuplicateCurve() : NULL;
	    if (!copy || (trim->m_bRev3d && !copy->Reverse()) ||
		!polycurve->Append(copy)) {
		delete copy;
		delete polycurve;
		delete_profile_loops(loops);
		reason = "could not assemble an ordered revolved profile loop";
		return false;
	    }
	}
	polycurve->SynchronizeSegmentDomains();
	ON_NurbsCurve *curve = ON_NurbsCurve::New();
	if (polycurve->GetNurbForm(*curve) <= 0 || !curve->IsValid() ||
	    !curve->IsClosed() || curve->PointAtStart().DistanceTo(
		curve->PointAtEnd()) > LocalUnits::tolerance) {
	    delete curve;
	    delete polycurve;
	    delete_profile_loops(loops);
	    reason = "revolved profile boundary is not a valid closed curve";
	    return false;
	}
	delete polycurve;
	RevolvedProfileLoop result;
	result.curve = curve;
	if (loop->m_type == ON_BrepLoop::outer) {
	    ++outer_count;
	    outer_index = loops.size();
	}
	loops.push_back(result);
    }
    if (outer_count != 1) {
	delete_profile_loops(loops);
	reason = "revolved profile must contain exactly one outer boundary loop";
	return false;
    }
    return true;
}

bool
curves_coincident(const ON_Curve &first, const ON_Curve &second, double tolerance)
{
    const ON_Interval first_domain = first.Domain();
    const ON_Interval second_domain = second.Domain();
    const double samples[] = {0.0, 0.173, 0.419, 0.731, 1.0};
    bool forward = true;
    bool reverse = true;
    for (size_t index = 0; index < sizeof(samples) / sizeof(samples[0]); ++index) {
	const ON_3dPoint point = first.PointAt(first_domain.ParameterAt(samples[index]));
	forward = forward && point.DistanceTo(second.PointAt(
	    second_domain.ParameterAt(samples[index]))) <= tolerance;
	reverse = reverse && point.DistanceTo(second.PointAt(
	    second_domain.ParameterAt(1.0 - samples[index]))) <= tolerance;
    }
    return forward || reverse;
}

bool
merge_revolution_edges(ON_Brep &brep, int side_edge_index,
	int cap_edge_index, std::string &reason)
{
    if (side_edge_index < 0 || side_edge_index >= brep.m_E.Count() ||
	cap_edge_index < 0 || cap_edge_index >= brep.m_E.Count()) {
	reason = "revolution cap edge index is invalid";
	return false;
    }
    bool reversed = false;
    const ON_Interval side_domain = brep.m_E[side_edge_index].Domain();
    const ON_Interval cap_domain = brep.m_E[cap_edge_index].Domain();
    const double samples[] = {0.0, 0.173, 0.419, 0.731, 1.0};
    bool forward = true;
    bool reverse = true;
    for (size_t index = 0; index < sizeof(samples) / sizeof(samples[0]); ++index) {
	const ON_3dPoint point = brep.m_E[side_edge_index].PointAt(
	    side_domain.ParameterAt(samples[index]));
	forward = forward && point.DistanceTo(brep.m_E[cap_edge_index].PointAt(
	    cap_domain.ParameterAt(samples[index]))) <= LocalUnits::tolerance;
	reverse = reverse && point.DistanceTo(brep.m_E[cap_edge_index].PointAt(
	    cap_domain.ParameterAt(1.0 - samples[index]))) <= LocalUnits::tolerance;
    }
    if (!forward && !reverse) {
	reason = "revolution cap edge does not coincide with a swept boundary edge";
	return false;
    }
    reversed = !forward && reverse;
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	const int side_vertex = brep.m_E[side_edge_index].m_vi[endpoint];
	const int cap_vertex = brep.m_E[cap_edge_index].m_vi[
	    reversed ? 1 - endpoint : endpoint];
	if (side_vertex < 0 || cap_vertex < 0 ||
	    (side_vertex != cap_vertex && !brep.CombineCoincidentVertices(
		brep.m_V[side_vertex], brep.m_V[cap_vertex]))) {
	    reason = "openNURBS could not mate revolution cap vertices";
	    return false;
	}
    }
    if (reversed) {
	for (int index = 0; index < brep.m_E[cap_edge_index].m_ti.Count(); ++index) {
	    const int trim_index = brep.m_E[cap_edge_index].m_ti[index];
	    if (trim_index >= 0 && trim_index < brep.m_T.Count())
		brep.m_T[trim_index].m_bRev3d =
		    !brep.m_T[trim_index].m_bRev3d;
	}
    }
    if (!brep.CombineCoincidentEdges(brep.m_E[side_edge_index],
	    brep.m_E[cap_edge_index])) {
	reason = "openNURBS could not mate a revolution cap edge";
	return false;
    }
    return true;
}

bool
append_revolution_cap(ON_Brep &brep, const ON_Plane &plane,
	const std::vector<ON_NurbsCurve *> &boundaries, size_t outer_index,
	bool reverse_face, std::string &reason, int *cap_face_index)
{
    if (boundaries.empty() || outer_index >= boundaries.size()) {
	reason = "revolution cap has no outer boundary";
	return false;
    }
    ON_BoundingBox bbox;
    for (std::vector<ON_NurbsCurve *>::const_iterator boundary =
	 boundaries.begin(); boundary != boundaries.end(); ++boundary)
	if (!*boundary || !(*boundary)->GetBoundingBox(bbox, true)) {
	    reason = "revolution cap boundary has no finite bounding box";
	    return false;
	}
    double umin = 0.0, umax = 0.0, vmin = 0.0, vmax = 0.0;
    bool first = true;
    for (unsigned int corner = 0; corner < 8; ++corner) {
	const ON_3dPoint point((corner & 1) ? bbox.m_max.x : bbox.m_min.x,
	    (corner & 2) ? bbox.m_max.y : bbox.m_min.y,
	    (corner & 4) ? bbox.m_max.z : bbox.m_min.z);
	double u = 0.0, v = 0.0;
	if (!plane.ClosestPointTo(point, &u, &v)) continue;
	if (first) {
	    umin = umax = u;
	    vmin = vmax = v;
	    first = false;
	} else {
	    umin = std::min(umin, u);
	    umax = std::max(umax, u);
	    vmin = std::min(vmin, v);
	    vmax = std::max(vmax, v);
	}
    }
    if (first) {
	reason = "revolution cap boundaries cannot be projected to their plane";
	return false;
    }

    ON_PlaneSurface *surface = new ON_PlaneSurface(plane);
    surface->SetDomain(0, umin - LocalUnits::tolerance,
	umax + LocalUnits::tolerance);
    surface->SetDomain(1, vmin - LocalUnits::tolerance,
	vmax + LocalUnits::tolerance);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    const int surface_index = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(surface_index);
    const int face_index = face.m_face_index;
    const int side_edge_limit = brep.m_E.Count();

    std::vector<size_t> loop_order;
    loop_order.push_back(outer_index);
    for (size_t index = 0; index < boundaries.size(); ++index)
	if (index != outer_index) loop_order.push_back(index);
    std::vector<int> cap_edges;
    for (std::vector<size_t>::const_iterator loop = loop_order.begin();
	 loop != loop_order.end(); ++loop) {
	ON_Curve *copy = boundaries[*loop]->DuplicateCurve();
	ON_SimpleArray<ON_Curve *> curves;
	if (copy) curves.Append(copy);
	const int edge_count = brep.m_E.Count();
	const ON_BrepLoop::TYPE loop_type = *loop == outer_index ?
	    ON_BrepLoop::outer : ON_BrepLoop::inner;
	const bool created = copy && brep.NewPlanarFaceLoop(face_index,
	    loop_type, curves, true);
	delete copy;
	if (!created || brep.m_E.Count() != edge_count + 1) {
	    reason = "openNURBS could not construct a revolution cap loop";
	    return false;
	}
	cap_edges.push_back(edge_count);
    }

    if (reverse_face) brep.FlipFace(brep.m_F[face_index]);
    brep.SetTrimIsoFlags(brep.m_F[face_index]);
    for (std::vector<int>::const_iterator cap_edge = cap_edges.begin();
	 cap_edge != cap_edges.end(); ++cap_edge) {
	int side_edge = -1;
	for (int index = 0; index < side_edge_limit; ++index) {
	    if (brep.m_E[index].m_edge_index < 0 ||
		brep.m_E[index].m_ti.Count() != 1)
		continue;
	    if (curves_coincident(brep.m_E[index], brep.m_E[*cap_edge],
		    LocalUnits::tolerance)) {
		side_edge = index;
		break;
	    }
	}
	if (side_edge < 0 || !merge_revolution_edges(brep, side_edge,
		*cap_edge, reason))
	    return false;
    }
    if (cap_face_index) *cap_face_index = face_index;
    return true;
}

const brlcad::step::Style *
swept_style(STEPWrapper &wrapper, int64_t item_id, int64_t representation_id)
{
    const std::map<int64_t, brlcad::step::Style> &styles = wrapper.Document().styles;
    std::map<int64_t, brlcad::step::Style>::const_iterator found = styles.find(item_id);
    if (found == styles.end()) found = styles.find(representation_id);
    return found == styles.end() ? NULL : &found->second;
}

bool
convert_extrusion(STEPWrapper &wrapper, BRLCADWrapper &database,
    STEPentity *solid, STEPentity *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    STEPentity *profile = dynamic_cast<STEPentity *>(brlcad::step::Entity(solid, "swept_area"));
    STEPentity *direction_entity = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(solid, "extruded_direction"));
    const double depth = wrapper.getRealAttribute(solid, "depth");
    if (!solid || !representation || !profile || !direction_entity ||
	!std::isfinite(depth) || depth <= 0.0) {
	reason = "extrusion has no profile/direction or has a non-positive depth";
	return false;
    }
    apply_units(units);
    ON_Brep *brep = planar_profile(wrapper, profile, units.length, reason);
    if (!brep) return false;

    double direction[3];
    if (!swept_direction_ratios(direction_entity, direction)) {
	reason = "extrusion direction is invalid";
	delete brep;
	return false;
    }
    const ON_3dVector extrusion(direction[0] * depth * units.length,
	direction[1] * depth * units.length,
	direction[2] * depth * units.length);
    const ON_LineCurve path(ON_Line(ON_origin, ON_origin + extrusion));
    if (ON_BrepExtrudeFace(*brep, 0, path, true) != 2) {
	reason = "openNURBS could not extrude and cap the planar profile";
	delete brep;
	return false;
    }
    brep->SetTolerancesBoxesAndFlags(false, false, false, false, true, true, true, true);
    ON_wString messages;
    ON_TextLog log(messages);
    if (!brep->IsValid(&log) || !brep->IsSolid()) {
	reason = "extruded BREP failed structural or solid validation";
	if (wrapper.Verbose()) {
	    ON_String text(messages);
	    std::cerr << text.Array();
	}
	delete brep;
	return false;
    }

    const std::string name = database.StableBRLCADName(product_name + "_swept_item",
	solid->STEPfile_id);
    mat_t matrix;
    representation_matrix(wrapper, representation, units.length, matrix);
    const brlcad::step::Style *style = swept_style(wrapper, solid->STEPfile_id,
	representation->STEPfile_id);
    const bool written = database.WriteBrep(name, brep, matrix, true, solid->STEPfile_id,
	wrapper.getStringAttribute(solid, "name"), style);
    delete brep;
    if (!written) {
	reason = "BRL-CAD database rejected the validated extruded BREP";
	return false;
    }
    if (style) ++wrapper.Statistics().styles_applied;
    mat_t identity;
    MAT_IDN(identity);
    return database.AddMember(product_name, name, identity);
}

bool
convert_face_extrusion(STEPWrapper &wrapper, BRLCADWrapper &database,
    STEPentity *solid, STEPentity *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    STEPentity *swept_face = dynamic_cast<STEPentity *>(brlcad::step::Entity(solid, "swept_face"));
    STEPentity *direction_entity = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(solid, "extruded_direction"));
    const double depth = wrapper.getRealAttribute(solid, "depth");
    if (!solid || !representation || !swept_face || !direction_entity ||
	!std::isfinite(depth) || depth <= 0.0) {
	reason = "face extrusion has no face/direction or has a non-positive depth";
	return false;
    }

    apply_units(units);
    wrapper.ResetOpenNURBSState();
    STEPEntity *factory_object = Factory::CreateObject(&wrapper, swept_face);
    FaceSurface *face = dynamic_cast<FaceSurface *>(factory_object);
    ON_Brep *brep = new ON_Brep();
    if (!face || !face->LoadONBrep(brep) || brep->m_F.Count() != 1 ||
	face->GetONId() < 0 || face->GetONId() >= brep->m_F.Count()) {
	delete brep;
	reason = "swept face could not be materialized as one bounded planar face";
	return false;
    }
    ON_Plane profile_plane;
    const ON_Surface *profile_surface = brep->m_F[face->GetONId()].SurfaceOf();
    if (!profile_surface || !profile_surface->IsPlanar(&profile_plane, LocalUnits::tolerance)) {
	delete brep;
	reason = "swept face is not planar within the model tolerance";
	return false;
    }

    double direction[3];
    if (!swept_direction_ratios(direction_entity, direction)) {
	delete brep;
	reason = "face extrusion direction is invalid";
	return false;
    }
    const ON_3dVector extrusion(direction[0] * depth * units.length,
	direction[1] * depth * units.length,
	direction[2] * depth * units.length);
    if (std::fabs(extrusion * profile_plane.zaxis) <= LocalUnits::tolerance) {
	delete brep;
	reason = "face extrusion direction is parallel to the profile plane";
	return false;
    }
    const ON_LineCurve path(ON_Line(ON_origin, ON_origin + extrusion));
    if (ON_BrepExtrudeFace(*brep, face->GetONId(), path, true) != 2) {
	delete brep;
	reason = "openNURBS could not extrude and cap the bounded face";
	return false;
    }
    brep->SetTolerancesBoxesAndFlags(false, false, false, false, true, true, true, true);
    ON_wString messages;
    ON_TextLog log(messages);
    if (!brep->IsValid(&log) || !brep->IsSolid()) {
	reason = "extruded-face BREP failed structural or solid validation";
	if (wrapper.Verbose()) {
	    ON_String text(messages);
	    std::cerr << text.Array();
	}
	delete brep;
	return false;
    }

    const std::string name = database.StableBRLCADName(product_name + "_swept_item",
	solid->STEPfile_id);
    mat_t matrix;
    representation_matrix(wrapper, representation, units.length, matrix);
    const brlcad::step::Style *style = swept_style(wrapper, solid->STEPfile_id,
	representation->STEPfile_id);
    const bool written = database.WriteBrep(name, brep, matrix, true, solid->STEPfile_id,
	wrapper.getStringAttribute(solid, "name"), style);
    delete brep;
    if (!written) {
	reason = "BRL-CAD database rejected the validated extruded-face BREP";
	return false;
    }
    if (style) ++wrapper.Statistics().styles_applied;
    mat_t identity;
    MAT_IDN(identity);
    return database.AddMember(product_name, name, identity);
}

bool
convert_profile_revolution(STEPWrapper &wrapper, BRLCADWrapper &database,
    STEPentity *item, const ON_Line &revolution_axis,
    double angle, ON_Brep *profile, STEPentity *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    if (!item || !revolution_axis.IsValid() || !profile || !representation ||
	!std::isfinite(angle) || angle <= 0.0) {
	delete profile;
	reason = "revolution has no profile/axis or has a non-positive angle";
	return false;
    }
    const double angle_tolerance = 64.0 * ON_EPSILON * ON_PI;
    if (angle > 2.0 * ON_PI + angle_tolerance) {
	delete profile;
	reason = "revolution angle exceeds one complete turn";
	return false;
    }
    const bool full_revolution = std::fabs(angle - 2.0 * ON_PI) <= angle_tolerance;

    apply_units(units);
    std::vector<RevolvedProfileLoop> profile_loops;
    size_t outer_index = 0;
    const bool have_profile = profile_loop_curves(*profile, profile_loops,
	outer_index, reason);
    delete profile;
    if (!have_profile) return false;

    const ON_3dPoint axis_origin = revolution_axis.from;
    ON_3dVector axis_direction = revolution_axis.Direction();
    if (!axis_origin.IsValid() || !axis_direction.Unitize()) {
	delete_profile_loops(profile_loops);
	reason = "revolution axis placement is invalid";
	return false;
    }
    ON_Plane profile_plane;
    if (!profile_loops[outer_index].curve->IsPlanar(&profile_plane,
	    LocalUnits::tolerance) ||
	std::fabs(profile_plane.DistanceTo(axis_origin)) > LocalUnits::tolerance ||
	std::fabs(profile_plane.zaxis * axis_direction) > 64.0 * ON_EPSILON) {
	delete_profile_loops(profile_loops);
	reason = "revolution axis does not lie in the swept profile plane";
	return false;
    }

    ON_Brep *brep = new ON_Brep();
    std::vector<ON_NurbsCurve *> start_boundaries;
    std::vector<ON_NurbsCurve *> end_boundaries;
    for (size_t loop_index = 0; loop_index < profile_loops.size();
	 ++loop_index) {
	ON_NurbsCurve *generating_curve = profile_loops[loop_index].curve;
	profile_loops[loop_index].curve = NULL;
	ON_NurbsCurve *start_boundary = NULL;
	ON_NurbsCurve *end_boundary = NULL;
	if (!full_revolution) {
	    start_boundary = new ON_NurbsCurve(*generating_curve);
	    end_boundary = new ON_NurbsCurve(*generating_curve);
	}
	ON_RevSurface *surface = ON_RevSurface::New();
	surface->m_curve = generating_curve;
	surface->m_axis = ON_Line(axis_origin, axis_origin + axis_direction);
	surface->m_angle = ON_Interval(0.0,
	    full_revolution ? 2.0 * ON_PI : angle);
	surface->m_t = surface->m_angle;
	surface->m_bTransposed = false;
	surface->BoundingBox();
	ON_Brep *wall = ON_BrepRevSurface(surface, false, false, NULL);
	if (!wall || wall->m_F.Count() != 1) {
	    if (!wall) delete surface;
	    delete wall;
	    delete start_boundary;
	    delete end_boundary;
	    delete_profile_loops(profile_loops);
	    for (std::vector<ON_NurbsCurve *>::iterator boundary =
		 start_boundaries.begin(); boundary != start_boundaries.end();
		 ++boundary)
		delete *boundary;
	    for (std::vector<ON_NurbsCurve *>::iterator boundary =
		 end_boundaries.begin(); boundary != end_boundaries.end();
		 ++boundary)
		delete *boundary;
	    delete brep;
	    reason = "openNURBS could not construct an analytic revolution wall";
	    return false;
	}
	brep->Append(*wall);
	delete wall;
	if (!full_revolution) {
	    start_boundaries.push_back(start_boundary);
	    end_boundaries.push_back(end_boundary);
	}
    }
    delete_profile_loops(profile_loops);

    if (!full_revolution) {
	ON_Xform rotation;
	rotation.Rotation(angle, axis_direction, axis_origin);
	bool transformed = true;
	for (std::vector<ON_NurbsCurve *>::iterator boundary =
	     end_boundaries.begin(); boundary != end_boundaries.end(); ++boundary)
	    transformed = (*boundary)->Transform(rotation) && transformed;
	if (!transformed) {
	    for (std::vector<ON_NurbsCurve *>::iterator boundary =
		 start_boundaries.begin(); boundary != start_boundaries.end();
		 ++boundary)
		delete *boundary;
	    for (std::vector<ON_NurbsCurve *>::iterator boundary =
		 end_boundaries.begin(); boundary != end_boundaries.end();
		 ++boundary)
		delete *boundary;
	    delete brep;
	    reason = "could not transform exact revolution end-cap boundaries";
	    return false;
	}
	ON_Plane end_plane(profile_plane);
	if (!end_plane.Transform(rotation)) {
	    for (std::vector<ON_NurbsCurve *>::iterator boundary =
		 start_boundaries.begin(); boundary != start_boundaries.end();
		 ++boundary)
		delete *boundary;
	    for (std::vector<ON_NurbsCurve *>::iterator boundary =
		 end_boundaries.begin(); boundary != end_boundaries.end();
		 ++boundary)
		delete *boundary;
	    delete brep;
	    reason = "could not transform the exact revolution end-cap plane";
	    return false;
	}

	/* Each revolved loop contributes one naked closed edge at each angular
	 * end.  Build one planar face with the matching outer and inner loops at
	 * each end, then merge every duplicated boundary into its wall edge. */
	int first_cap = -1;
	int second_cap = -1;
	const bool caps_created = append_revolution_cap(*brep, profile_plane,
	    start_boundaries, outer_index, false, reason, &first_cap) &&
	    append_revolution_cap(*brep, end_plane, end_boundaries, outer_index,
		true, reason, &second_cap);
	for (std::vector<ON_NurbsCurve *>::iterator boundary =
	     start_boundaries.begin(); boundary != start_boundaries.end();
	     ++boundary)
	    delete *boundary;
	for (std::vector<ON_NurbsCurve *>::iterator boundary =
	     end_boundaries.begin(); boundary != end_boundaries.end(); ++boundary)
	    delete *boundary;
	if (!caps_created) {
	    delete brep;
	    return false;
	}
	brep->Compact();
	brep->SetTolerancesBoxesAndFlags(false);
	/* The standalone openNURBS coincidence merge can leave the retained
	 * analytic edge tolerance unset even when both inputs are the same exact
	 * curve.  Bound those sentinel values by the asserted model tolerance;
	 * subsequent structural and solid checks still reject mismatched trims. */
	for (int index = 0; index < brep->m_E.Count(); ++index)
	    if (brep->m_E[index].m_tolerance < 0.0)
		brep->m_E[index].m_tolerance = LocalUnits::tolerance;
	if (!brep->IsSolid()) {
	    /* Profile orientation determines the swept face orientation.  Toggle
	     * cap senses in a bounded search and retain only an oriented manifold. */
	    brep->FlipFace(brep->m_F[first_cap]);
	    if (!brep->IsSolid()) {
		brep->FlipFace(brep->m_F[second_cap]);
		if (!brep->IsSolid()) {
		    brep->FlipFace(brep->m_F[first_cap]);
		    if (!brep->IsSolid()) {
			delete brep;
			reason = "partial revolution end caps do not form an oriented manifold";
			return false;
		    }
		}
	    }
	}
    }
    brep->SetTolerancesBoxesAndFlags(false);
    for (int index = 0; index < brep->m_E.Count(); ++index)
	if (brep->m_E[index].m_tolerance < 0.0)
	    brep->m_E[index].m_tolerance = LocalUnits::tolerance;
    ON_wString messages;
    ON_TextLog log(messages);
    if (!brep->IsValid(&log) || !brep->IsSolid()) {
	reason = "revolved BREP failed structural or solid validation";
	if (wrapper.Verbose()) {
	    ON_String text(messages);
	    std::cerr << text.Array();
	}
	delete brep;
	return false;
    }

    const std::string name = database.StableBRLCADName(product_name + "_swept_item",
	item->STEPfile_id);
    mat_t matrix;
    representation_matrix(wrapper, representation, units.length, matrix);
    const brlcad::step::Style *style = swept_style(wrapper, item->STEPfile_id,
	representation->STEPfile_id);
    const bool written = database.WriteBrep(name, brep, matrix, true, item->STEPfile_id,
	wrapper.getStringAttribute(item, "name"), style);
    delete brep;
    if (!written) {
	reason = "BRL-CAD database rejected the validated revolved BREP";
	return false;
    }
    if (style) ++wrapper.Statistics().styles_applied;
    mat_t identity;
    MAT_IDN(identity);
    return database.AddMember(product_name, name, identity);
}

bool
convert_revolution(STEPWrapper &wrapper, BRLCADWrapper &database,
    STEPentity *solid, STEPentity *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    STEPentity *profile_entity = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(solid, "swept_area"));
    STEPentity *axis = dynamic_cast<STEPentity *>(brlcad::step::Entity(solid, "axis"));
    const double angle = wrapper.getRealAttribute(solid, "angle");
    if (!solid || !representation || !profile_entity || !axis ||
	!std::isfinite(angle) || angle <= 0.0) {
	reason = "revolution has no profile/axis or has a non-positive angle";
	return false;
    }
    apply_units(units);
    ON_Line revolution_axis;
    if (!swept_axis1_line(axis, units.length, revolution_axis)) {
	reason = "revolution axis placement is invalid";
	return false;
    }
    ON_Brep *profile = planar_profile(wrapper, profile_entity, units.length, reason);
    if (!profile) return false;
    return convert_profile_revolution(wrapper, database, solid, revolution_axis,
	angle * units.plane_angle, profile, representation, product_name, units,
	reason);
}

bool
convert_face_revolution(STEPWrapper &wrapper, BRLCADWrapper &database,
    STEPentity *solid, STEPentity *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    STEPentity *swept_face = dynamic_cast<STEPentity *>(brlcad::step::Entity(solid, "swept_face"));
    STEPentity *axis = dynamic_cast<STEPentity *>(brlcad::step::Entity(solid, "axis"));
    const double angle = wrapper.getRealAttribute(solid, "angle");
    if (!solid || !representation || !swept_face || !axis ||
	!std::isfinite(angle) || angle <= 0.0) {
	reason = "face revolution has no face/axis or has a non-positive angle";
	return false;
    }

    apply_units(units);
    ON_Line revolution_axis;
    if (!swept_axis1_line(axis, units.length, revolution_axis)) {
	reason = "face revolution axis placement is invalid";
	return false;
    }
    wrapper.ResetOpenNURBSState();
    STEPEntity *factory_object = Factory::CreateObject(&wrapper, swept_face);
    FaceSurface *face = dynamic_cast<FaceSurface *>(factory_object);
    ON_Brep *profile = new ON_Brep();
    if (!face || !face->LoadONBrep(profile) || profile->m_F.Count() != 1 ||
	face->GetONId() < 0 || face->GetONId() >= profile->m_F.Count()) {
	delete profile;
	reason = "swept face could not be materialized as one bounded face";
	return false;
    }
    ON_Plane profile_plane;
    const ON_Surface *profile_surface = profile->m_F[face->GetONId()].SurfaceOf();
    if (!profile_surface || !profile_surface->IsPlanar(&profile_plane,
	    LocalUnits::tolerance)) {
	delete profile;
	reason = "swept face is not planar within the model tolerance";
	return false;
    }
    return convert_profile_revolution(wrapper, database, solid, revolution_axis,
	angle * units.plane_angle, profile, representation, product_name, units,
	reason);
}

ON_Brep *
circular_swept_disk_profile(const ON_Plane &plane, double radius,
    double inner_radius, std::string &reason)
{
    const ON_Circle outer_circle(plane, plane.origin, radius);
    const ON_ArcCurve outer_curve(outer_circle);
    ON_Brep *profile = outer_circle.IsValid() ?
	ON_BrepTrimmedPlane(plane, outer_curve, NULL) : NULL;
    if (!profile || profile->m_F.Count() != 1 ||
	profile->m_F[0].LoopCount() != 1) {
	delete profile;
	reason = "openNURBS could not construct the circular swept-disk profile";
	return NULL;
    }
    if (inner_radius > 0.0) {
	const ON_Circle inner_circle(plane, plane.origin, inner_radius);
	ON_Curve *inner_curve = inner_circle.IsValid() ?
	    new ON_ArcCurve(inner_circle) : NULL;
	ON_SimpleArray<ON_Curve *> curves;
	if (inner_curve) curves.Append(inner_curve);
	const bool created = inner_curve && profile->NewPlanarFaceLoop(0,
	    ON_BrepLoop::inner, curves, true);
	delete inner_curve;
	if (!created || profile->m_F[0].LoopCount() != 2) {
	    delete profile;
	    reason = "openNURBS could not construct the swept-disk inner profile";
	    return NULL;
	}
    }
    profile->SetTrimIsoFlags(profile->m_F[0]);
    return profile;
}

bool
convert_circular_swept_disk(STEPWrapper &wrapper, BRLCADWrapper &database,
    STEPentity *solid, STEPentity *directrix, STEPentity *representation,
    const std::string &product_name, const UnitFactors &units, double radius,
    double inner_radius, double start_param, double end_param,
    std::string &reason)
{
    STEPentity *position = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(directrix, "position"));
    double center_value[3], x_value[3], y_value[3], normal_value[3];
    if (!swept_axis2_placement(position, units.length, center_value, x_value,
	    y_value, normal_value)) {
	reason = "circular swept-disk directrix has an invalid placement";
	return false;
    }
    const double directrix_radius =
	wrapper.getRealAttribute(directrix, "radius") * units.length;
    const double outer_radius = radius * units.length;
    const double void_radius = inner_radius * units.length;
    if (!std::isfinite(directrix_radius) || directrix_radius <= 0.0 ||
	outer_radius >= directrix_radius - LocalUnits::tolerance) {
	reason = "circular swept disk is degenerate or self-intersects at its "
	    "curvature radius";
	return false;
    }

    double start_angle = start_param * units.plane_angle;
    double sweep_angle = end_param * units.plane_angle - start_angle;
    if (std::isfinite(sweep_angle) && sweep_angle < 0.0)
	sweep_angle += std::ceil(-sweep_angle / (2.0 * ON_PI)) *
	    2.0 * ON_PI;
    const double angle_tolerance = 64.0 * ON_EPSILON * ON_PI;
    if (!std::isfinite(start_angle) || !std::isfinite(sweep_angle) ||
	sweep_angle <= angle_tolerance ||
	sweep_angle > 2.0 * ON_PI + angle_tolerance) {
	reason = "circular swept-disk parameter range is empty or exceeds one turn";
	return false;
    }
    if (std::fabs(sweep_angle - 2.0 * ON_PI) <= angle_tolerance)
	sweep_angle = 2.0 * ON_PI;

    const ON_3dPoint center(center_value);
    ON_3dVector xaxis(x_value);
    ON_3dVector yaxis(y_value);
    ON_3dVector normal(normal_value);
    const ON_3dVector radial = std::cos(start_angle) * xaxis +
	std::sin(start_angle) * yaxis;
    const ON_3dPoint profile_center = center + directrix_radius * radial;
    const ON_Plane profile_plane(profile_center, radial, normal);
    ON_Brep *profile = circular_swept_disk_profile(profile_plane,
	outer_radius, void_radius, reason);
    if (!profile) return false;

    const ON_Line revolution_axis(center, center + normal);
    return convert_profile_revolution(wrapper, database, solid,
	revolution_axis, sweep_angle, profile, representation, product_name,
	units, reason);
}

bool
convert_swept_disk(STEPWrapper &wrapper, BRLCADWrapper &database,
    STEPentity *solid, STEPentity *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    STEPentity *directrix_entity = dynamic_cast<STEPentity *>(
	brlcad::step::Entity(solid, "directrix"));
    const double radius = wrapper.getRealAttribute(solid, "radius");
    const double inner_radius = wrapper.getRealAttribute(solid, "inner_radius");
    const double start_param = wrapper.getRealAttribute(solid, "start_param");
    const double end_param = wrapper.getRealAttribute(solid, "end_param");
    if (!solid || !representation || !directrix_entity ||
	!std::isfinite(radius) || radius <= 0.0 ||
	!std::isfinite(inner_radius) || inner_radius < 0.0 ||
	inner_radius >= radius ||
	!std::isfinite(start_param) || !std::isfinite(end_param) ||
	std::fabs(start_param - end_param) <=
	    ON_EPSILON * std::max(1.0, std::max(std::fabs(start_param),
		std::fabs(end_param)))) {
	reason = "swept disk has an invalid directrix, radius, inner radius, or parameter range";
	return false;
    }

    apply_units(units);
    if (brlcad::step::EqualTypeName(directrix_entity->EntityName(),
	    "CIRCLE")) {
	return convert_circular_swept_disk(wrapper, database, solid,
	    directrix_entity, representation, product_name, units, radius,
	    inner_radius, start_param, end_param, reason);
    }
    wrapper.ResetOpenNURBSState();
    ON_Brep curve_holder;
    STEPEntity *factory_object = Factory::CreateObject(&wrapper, directrix_entity);
    Curve *directrix = dynamic_cast<Curve *>(factory_object);
    if (!directrix || !directrix->LoadONBrep(&curve_holder)) {
	reason = "swept disk directrix could not be materialized as an exact curve";
	return false;
    }
    const int curve_id = directrix->GetONId();
    const ON_Curve *curve = curve_id >= 0 && curve_id < curve_holder.m_C3.Count() ?
	curve_holder.m_C3[curve_id] : NULL;
    if (!curve || !ON_LineCurve::Cast(curve)) {
	reason = "only exact linear and circular SWEPT_DISK_SOLID directrices "
	    "are currently supported";
	return false;
    }
    const ON_Interval domain = curve->Domain();
    if (!domain.Includes(start_param) || !domain.Includes(end_param)) {
	reason = "swept disk parameter range lies outside its directrix domain";
	return false;
    }

    const ON_3dPoint start = curve->PointAt(start_param);
    const ON_3dPoint end = curve->PointAt(end_param);
    const ON_3dVector path = end - start;
    if (!start.IsValid() || !end.IsValid() || path.Length() <= LocalUnits::tolerance) {
	reason = "swept disk directrix interval is degenerate";
	return false;
    }

    const double base[3] = {start.x, start.y, start.z};
    const double height[3] = {path.x, path.y, path.z};
    const std::string item_name = database.StableBRLCADName(
	product_name + "_swept_item", solid->STEPfile_id);
    const std::string outer_name = item_name + "_outer.s";
    const std::string original = wrapper.getStringAttribute(solid, "name");
    if (!database.WriteRcc(outer_name, base, height, radius * units.length,
	solid->STEPfile_id, original)) {
	reason = "BRL-CAD database rejected the exact swept-disk outer cylinder";
	return false;
    }

    mat_t matrix;
    representation_matrix(wrapper, representation, units.length, matrix);
    if (!database.AddMember(item_name, outer_name, matrix)) {
	reason = "could not add the swept-disk outer cylinder to its region";
	return false;
    }
    if (inner_radius > 0.0) {
	const std::string inner_name = item_name + "_inner.s";
	if (!database.WriteRcc(inner_name, base, height,
		inner_radius * units.length, solid->STEPfile_id, original) ||
	    !database.AddMember(item_name, inner_name, matrix, WMOP_SUBTRACT)) {
	    reason = "could not construct the exact swept-disk inner void";
	    return false;
	}
    }

    const brlcad::step::Style *style = swept_style(wrapper, solid->STEPfile_id,
	representation->STEPfile_id);
    mat_t identity;
    MAT_IDN(identity);
    if (!database.SetCombinationProperties(item_name, true, solid->STEPfile_id,
	original, style) || !database.AddMember(product_name, item_name, identity)) {
	reason = "could not add the exact swept-disk region to its product";
	return false;
    }
    if (style) ++wrapper.Statistics().styles_applied;
    return true;
}

} // namespace

void
ConvertSTEPSweptSolids(STEPWrapper &wrapper, BRLCADWrapper &database)
{
    std::set<int64_t> processed;
    /* The generic representation walk may record an association as visited
     * after accounting for an unsupported swept item, so its handled list
     * cannot exclude this pass.  In lazy mode use only index references to
     * identify associations whose representation actually contains a swept
     * item.  Activating every association would materialize an entire large
     * non-sweep representation just to reject it here. */
    if (wrapper.HasLazyIndex()) {
	std::set<uint64_t> swept_ids;
	/* Query concrete importable leaves.  Asking the lazy index for broad
	 * schema supertypes repeatedly tests every file instance against a large
	 * AP242 inheritance graph, including the overwhelmingly common case where
	 * the file has no swept solids at all.  Reverse references from the few
	 * concrete roots find their owning representation and association. */
	const char *swept_types[] = {
	    "EXTRUDED_AREA_SOLID", "REVOLVED_AREA_SOLID",
	    "EXTRUDED_FACE_SOLID", "REVOLVED_FACE_SOLID", "SWEPT_DISK_SOLID"
	};
	for (size_t type = 0; type < sizeof(swept_types) / sizeof(swept_types[0]); ++type) {
	    const std::vector<uint64_t> ids =
		wrapper.LazyInstancesByType(swept_types[type]);
	    swept_ids.insert(ids.begin(), ids.end());
	}
	if (swept_ids.empty()) return;
	std::set<uint64_t> association_set;
	for (uint64_t swept_id : swept_ids) {
	    for (uint64_t representation : wrapper.LazyReverseReferences(swept_id)) {
		if (!wrapper.LazyIsSchemaEntity(representation, "REPRESENTATION"))
		    continue;
		for (uint64_t association :
			wrapper.LazyReverseReferences(representation))
		    if (wrapper.LazyIsSchemaEntity(association,
			    "SHAPE_DEFINITION_REPRESENTATION"))
			association_set.insert(association);
	    }
	}
	const std::vector<uint64_t> association_ids(association_set.begin(),
	    association_set.end());
	if (association_ids.empty()) return;
	wrapper.SetInstanceIds(association_ids);
    } else {
	wrapper.SetInstanceTypes({"SHAPE_DEFINITION_REPRESENTATION"});
    }
    for (int index = 0; index < wrapper.InstanceCount(); ++index) {
	SDAI_Application_instance *instance = wrapper.InstanceAt(index);
	if (!instance || instance->STEPfile_id <= 0 ||
	    !wrapper.IsSchemaEntity(instance, "SHAPE_DEFINITION_REPRESENTATION"))
	    continue;
	STEPentity *link = dynamic_cast<STEPentity *>(instance);
	/* AP203e2/AP214 retain the inherited represented_definition SELECT,
	 * whereas AP242 narrows this attribute to a direct PROPERTY_DEFINITION.
	 * Descriptor-backed Entity() deliberately handles both physical forms. */
	STEPentity *property = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(link, "definition"));
	if (!property || !wrapper.IsSchemaEntity(property, "PROPERTY_DEFINITION")) continue;
	STEPentity *definition = swept_product_definition(wrapper,
	    dynamic_cast<STEPentity *>(brlcad::step::Entity(property, "definition")));
	STEPentity *representation = dynamic_cast<STEPentity *>(
	    brlcad::step::Entity(link, "used_representation"));
	const std::vector<SDAI_Application_instance *> items =
	    brlcad::step::Entities(representation, "items");
	if (!definition || !representation || items.empty()) continue;
	const int64_t product_id = swept_product_id(definition);
	std::map<int64_t, brlcad::step::Product>::iterator product =
	    wrapper.Document().products.find(product_id);
	if (product == wrapper.Document().products.end() || product->second.output_name.empty()) continue;

	const UnitFactors units = representation_units(wrapper, representation);
	for (std::vector<SDAI_Application_instance *>::const_iterator i = items.begin();
	     i != items.end(); ++i) {
	    STEPentity *geometry = dynamic_cast<STEPentity *>(*i);
	    const bool swept = geometry &&
		wrapper.IsSchemaEntity(geometry, "SWEPT_AREA_SOLID");
	    const bool face_swept = geometry &&
		wrapper.IsSchemaEntity(geometry, "SWEPT_FACE_SOLID");
	    const bool disk = geometry &&
		wrapper.IsSchemaEntity(geometry, "SWEPT_DISK_SOLID");
	    if (!swept && !face_swept && !disk) continue;
	    if (!geometry || !processed.insert(geometry->STEPfile_id).second) {
		continue;
	    }
	    if (!wrapper.ShouldConvertEntity(geometry->STEPfile_id)) {
		continue;
	    }
	    ++wrapper.Statistics().geometry_attempted;
	    bool written = false;
	    bool supported = false;
	    std::string failure_reason;
	    std::string type = wrapper.HasLazyIndex() ?
		wrapper.LazyTypeName(geometry->STEPfile_id) :
		(geometry->EntityName() ? geometry->EntityName() :
		    (face_swept ? "SWEPT_FACE_SOLID" : "SWEPT_AREA_SOLID"));
	    if (disk && brlcad::step::EqualTypeName(geometry->EntityName(),
		    "SWEPT_DISK_SOLID")) {
		supported = true;
		type = "SWEPT_DISK_SOLID";
		written = convert_swept_disk(wrapper, database, geometry, representation,
		    product->second.output_name, units, failure_reason);
	    } else if (brlcad::step::EqualTypeName(geometry->EntityName(),
		    "EXTRUDED_AREA_SOLID")) {
		supported = true;
		type = "EXTRUDED_AREA_SOLID";
		written = convert_extrusion(wrapper, database, geometry, representation,
		    product->second.output_name, units, failure_reason);
	    } else if (brlcad::step::EqualTypeName(geometry->EntityName(),
		    "REVOLVED_AREA_SOLID")) {
		supported = true;
		type = "REVOLVED_AREA_SOLID";
		written = convert_revolution(wrapper, database, geometry, representation,
		    product->second.output_name, units, failure_reason);
	    } else if (brlcad::step::EqualTypeName(geometry->EntityName(),
		    "EXTRUDED_FACE_SOLID")) {
		supported = true;
		type = "EXTRUDED_FACE_SOLID";
		written = convert_face_extrusion(wrapper, database, geometry,
		    representation, product->second.output_name, units, failure_reason);
	    } else if (brlcad::step::EqualTypeName(geometry->EntityName(),
		    "REVOLVED_FACE_SOLID")) {
		supported = true;
		type = "REVOLVED_FACE_SOLID";
		written = convert_face_revolution(wrapper, database, geometry,
		    representation, product->second.output_name, units, failure_reason);
	    }
	    if (written) {
		++wrapper.Statistics().geometry_written;
		brlcad::step::Representation &record =
		    wrapper.Document().representations[geometry->STEPfile_id];
		record.entity_id = geometry->STEPfile_id;
		record.product_id = product_id;
		record.type = type;
		record.output_name = database.StableBRLCADName(
		    product->second.output_name + "_swept_item", geometry->STEPfile_id);
		wrapper.RecordRepresentationItemCoverage(geometry->STEPfile_id,
		    brlcad::step::RepresentationCoverageStatus::Handled,
		    "exact swept solid converted successfully");
	    } else {
		++wrapper.Statistics().geometry_skipped;
		const std::string omission_reason = failure_reason.empty() ?
		    (supported ? "exact swept-area conversion failed validation" :
			"swept-solid subtype has no exact importer") : failure_reason;
		wrapper.RecordSkippedItem(geometry->STEPfile_id, type, omission_reason);
		wrapper.RecordRepresentationItemCoverage(geometry->STEPfile_id,
		    supported ? brlcad::step::RepresentationCoverageStatus::Skipped :
			brlcad::step::RepresentationCoverageStatus::Unsupported,
		    omission_reason);
		wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		    geometry->STEPfile_id, type, std::string(),
		    omission_reason);
	    }
	}
    }
    wrapper.ClearEntityCache();
    wrapper.ResetInstanceTypes();
}
