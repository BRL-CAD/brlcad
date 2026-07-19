/*               A P 2 1 4 S W E P T S O L I D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include "AP214SweptSolid.h"
#include "BRLCADWrapper.h"
#include "STEPString.h"
#include "STEPWrapper.h"

#include "Curve.h"
#include "FaceSurface.h"
#include "Factory.h"
#include "GlobalUnitAssignedContext.h"
#include "LocalUnits.h"

#include "vmath.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

namespace {

SdaiProduct_definition *
swept_product_definition(SdaiCharacterized_definition *definition)
{
    if (!definition) return NULL;
    if (definition->IsCharacterized_product_definition() == LTrue) {
	SdaiCharacterized_product_definition *select = *definition;
	if (select && select->IsProduct_definition() == LTrue) {
	    SdaiProduct_definition *product = *select;
	    return product;
	}
    }
    if (definition->IsShape_definition() == LTrue) {
	SdaiShape_definition *select = *definition;
	if (select && select->IsProduct_definition_shape() == LTrue) {
	    SdaiProduct_definition_shape *shape = *select;
	    return shape ? swept_product_definition(shape->definition_()) : NULL;
	}
    }
    return NULL;
}

int64_t
swept_product_id(SdaiProduct_definition *definition)
{
    SdaiProduct_definition_formation *formation = definition ? definition->formation_() : NULL;
    SdaiProduct *product = formation ? formation->of_product_() : NULL;
    return product ? product->STEPfile_id : 0;
}

bool
swept_point_coordinates(SdaiPoint *point, double *coordinates)
{
    SdaiCartesian_point *cartesian = dynamic_cast<SdaiCartesian_point *>(point);
    RealAggregate *values = cartesian ? cartesian->coordinates_() : NULL;
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
swept_direction_ratios(SdaiDirection *direction, double *ratios)
{
    RealAggregate *values = direction ? direction->direction_ratios_() : NULL;
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
swept_axis2_placement(SdaiAxis2_placement_3d *placement, double length,
    double *origin, double *xaxis, double *yaxis, double *zaxis)
{
    if (!placement || !swept_point_coordinates(placement->location_(), origin)) return false;
    VSCALE(origin, origin, length);
    VSET(zaxis, 0.0, 0.0, 1.0);
    VSET(xaxis, 1.0, 0.0, 0.0);
    if (placement->axis_() && !swept_direction_ratios(placement->axis_(), zaxis)) return false;
    if (placement->ref_direction_() && !swept_direction_ratios(placement->ref_direction_(), xaxis)) return false;
    VJOIN1(xaxis, xaxis, -(VDOT(xaxis, zaxis)), zaxis);
    if (MAGNITUDE(xaxis) <= SMALL_FASTF) return false;
    VUNITIZE(xaxis);
    VCROSS(yaxis, zaxis, xaxis);
    if (MAGNITUDE(yaxis) <= SMALL_FASTF) return false;
    VUNITIZE(yaxis);
    return true;
}

bool
swept_axis1_placement(SdaiAxis1_placement *placement, double length,
    double *origin, double *axis)
{
    if (!placement || !swept_point_coordinates(placement->location_(), origin)) return false;
    VSCALE(origin, origin, length);
    VSET(axis, 0.0, 0.0, 1.0);
    return !placement->axis_() || swept_direction_ratios(placement->axis_(), axis);
}

struct UnitFactors {
    double length = 1000.0;
    double plane_angle = 1.0;
    double solid_angle = 1.0;
};

UnitFactors
representation_units(STEPWrapper &wrapper, SdaiRepresentation *representation)
{
    UnitFactors result;
    SdaiRepresentation_context *context = representation ? representation->context_of_items_() : NULL;
    if (!context) return result;
    GlobalUnitAssignedContext units(&wrapper, context->STEPfile_id);
    if (!units.Load(&wrapper, context)) return result;
    const double length = units.GetLengthConversionFactor();
    const double plane_angle = units.GetPlaneAngleConversionFactor();
    const double solid_angle = units.GetSolidAngleConversionFactor();
    if (length > 0.0) result.length = length;
    if (plane_angle > 0.0) result.plane_angle = plane_angle;
    if (solid_angle > 0.0) result.solid_angle = solid_angle;
    return result;
}

void
representation_matrix(SdaiRepresentation *representation, double length, mat_t matrix)
{
    MAT_IDN(matrix);
    EntityAggregate *items = representation ? representation->items_() : NULL;
    EntityNode *node = items ? static_cast<EntityNode *>(items->GetHead()) : NULL;
    while (node) {
	SdaiAxis2_placement_3d *placement = dynamic_cast<SdaiAxis2_placement_3d *>(node->node);
	if (placement) {
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
	node = static_cast<EntityNode *>(node->NextNode());
    }
}

bool
append_boundary_curves(STEPWrapper &wrapper, SdaiBoundary_curve *boundary,
    ON_Brep &curve_holder, const ON_Plane &plane, std::vector<ON_Curve *> &curves,
    std::string &reason)
{
    if (!boundary || boundary->self_intersect_() == LTrue || !boundary->segments_()) {
	reason = "profile boundary is absent, self-intersecting, or has no segments";
	return false;
    }
    EntityNode *node = static_cast<EntityNode *>(boundary->segments_()->GetHead());
    while (node) {
	SdaiComposite_curve_segment *segment =
	    dynamic_cast<SdaiComposite_curve_segment *>(node->node);
	SdaiCurve *source = segment ? segment->parent_curve_() : NULL;
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
		SdaiCurve *resolved_curve = dynamic_cast<SdaiCurve *>(curve_3d);
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
	if (!segment->same_sense_() && !copy->Reverse()) {
	    delete copy;
	    reason = "profile boundary curve orientation could not be reversed";
	    return false;
	}
	curves.push_back(copy);
	node = static_cast<EntityNode *>(node->NextNode());
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
planar_profile(STEPWrapper &wrapper, SdaiCurve_bounded_surface *profile, double length,
    std::string &reason)
{
    if (!profile || profile->implicit_outer_() || !profile->boundaries_()) {
	reason = "swept profile requires explicit planar boundaries";
	return NULL;
    }
    SdaiPlane *basis = dynamic_cast<SdaiPlane *>(profile->basis_surface_());
    SdaiAxis2_placement_3d *position = basis ? basis->position_() : NULL;
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
    EntityNode *node = static_cast<EntityNode *>(profile->boundaries_()->GetHead());
    while (node) {
	SdaiBoundary_curve *boundary = dynamic_cast<SdaiBoundary_curve *>(node->node);
	boundaries.push_back(std::vector<ON_Curve *>());
	outer_boundaries.push_back(node->node &&
	    node->node->IsA(SCHEMA_NAMESPACE::e_outer_boundary_curve));
	if (!append_boundary_curves(wrapper, boundary, curve_holder, plane, boundaries.back(), reason)) {
	    for (std::vector<std::vector<ON_Curve *> >::iterator loop = boundaries.begin();
		 loop != boundaries.end(); ++loop)
		for (std::vector<ON_Curve *>::iterator curve = loop->begin(); curve != loop->end(); ++curve)
		    delete *curve;
	    return NULL;
	}
	node = static_cast<EntityNode *>(node->NextNode());
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

ON_PolyCurve *
outer_profile_curve(const ON_Brep &profile, std::string &reason)
{
    if (profile.m_F.Count() != 1 || profile.m_F[0].LoopCount() != 1) {
	reason = "revolved profiles with inner boundary loops are not yet supported";
	return NULL;
    }
    const ON_BrepLoop *loop = profile.m_F[0].Loop(0);
    if (!loop || loop->m_type != ON_BrepLoop::outer || loop->TrimCount() < 1) {
	reason = "revolved profile has no usable outer boundary loop";
	return NULL;
    }

    ON_PolyCurve *curve = new ON_PolyCurve(loop->TrimCount());
    for (int index = 0; index < loop->TrimCount(); ++index) {
	const ON_BrepTrim *trim = loop->Trim(index);
	const ON_Curve *edge_curve = trim ? trim->EdgeCurveOf() : NULL;
	ON_Curve *copy = edge_curve ? edge_curve->DuplicateCurve() : NULL;
	if (!copy || (trim->m_bRev3d && !copy->Reverse()) || !curve->Append(copy)) {
	    delete copy;
	    delete curve;
	    reason = "could not assemble the ordered revolved profile curve";
	    return NULL;
	}
    }
    curve->SynchronizeSegmentDomains();
    if (!curve->IsValid() || !curve->IsClosed() ||
	curve->PointAtStart().DistanceTo(curve->PointAtEnd()) > LocalUnits::tolerance) {
	delete curve;
	reason = "revolved profile boundary is not a valid closed curve";
	return NULL;
    }
    return curve;
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
append_revolution_cap(ON_Brep &brep, const ON_Plane &plane, const ON_Curve &boundary,
    bool reverse_face, std::string &reason)
{
    ON_Brep *cap = ON_BrepTrimmedPlane(plane, boundary, NULL);
    if (!cap || cap->m_F.Count() != 1 || cap->m_E.Count() != 1) {
	delete cap;
	reason = "openNURBS could not construct a single-edge revolution end cap";
	return false;
    }
    if (reverse_face) cap->FlipFace(cap->m_F[0]);

    const int old_edge_count = brep.m_E.Count();
    brep.Append(*cap);
    delete cap;
    const int cap_edge_index = old_edge_count;
    int side_edge_index = -1;
    for (int index = 0; index < old_edge_count; ++index) {
	if (brep.m_E[index].m_edge_index < 0 || brep.m_E[index].m_ti.Count() != 1)
	    continue;
	if (curves_coincident(brep.m_E[index], brep.m_E[cap_edge_index],
		LocalUnits::tolerance)) {
	    side_edge_index = index;
	    break;
	}
    }
    if (side_edge_index < 0) {
	reason = "revolution end cap does not coincide with a swept boundary edge";
	return false;
    }

    const int side_vertex = brep.m_E[side_edge_index].m_vi[0];
    const int cap_vertex = brep.m_E[cap_edge_index].m_vi[0];
    if (side_vertex < 0 || cap_vertex < 0 ||
	brep.m_V[side_vertex].Point().DistanceTo(brep.m_V[cap_vertex].Point()) >
	    LocalUnits::tolerance ||
	(side_vertex != cap_vertex && !brep.CombineCoincidentVertices(
	    brep.m_V[side_vertex], brep.m_V[cap_vertex])) ||
	!brep.CombineCoincidentEdges(brep.m_E[side_edge_index], brep.m_E[cap_edge_index])) {
	reason = "openNURBS could not mate a revolution end cap to the swept surface";
	return false;
    }
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
    SdaiExtruded_area_solid *solid, SdaiRepresentation *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    if (!solid || !representation || !solid->swept_area_() || !solid->extruded_direction_() ||
	!std::isfinite(solid->depth_()) || solid->depth_() <= 0.0) {
	reason = "extrusion has no profile/direction or has a non-positive depth";
	return false;
    }
    LocalUnits::length = units.length;
    LocalUnits::planeangle = units.plane_angle;
    LocalUnits::solidangle = units.solid_angle;
    ON_Brep *brep = planar_profile(wrapper, solid->swept_area_(), units.length, reason);
    if (!brep) return false;

    double direction[3];
    if (!swept_direction_ratios(solid->extruded_direction_(), direction)) {
	reason = "extrusion direction is invalid";
	delete brep;
	return false;
    }
    const ON_3dVector extrusion(direction[0] * solid->depth_() * units.length,
	direction[1] * solid->depth_() * units.length,
	direction[2] * solid->depth_() * units.length);
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
    representation_matrix(representation, units.length, matrix);
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
    SdaiExtruded_face_solid *solid, SdaiRepresentation *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    if (!solid || !representation || !solid->swept_face_() || !solid->extruded_direction_() ||
	!std::isfinite(solid->depth_()) || solid->depth_() <= 0.0) {
	reason = "face extrusion has no face/direction or has a non-positive depth";
	return false;
    }

    LocalUnits::length = units.length;
    LocalUnits::planeangle = units.plane_angle;
    LocalUnits::solidangle = units.solid_angle;
    wrapper.ResetOpenNURBSState();
    STEPEntity *factory_object = Factory::CreateObject(&wrapper, solid->swept_face_());
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
    if (!swept_direction_ratios(solid->extruded_direction_(), direction)) {
	delete brep;
	reason = "face extrusion direction is invalid";
	return false;
    }
    const ON_3dVector extrusion(direction[0] * solid->depth_() * units.length,
	direction[1] * solid->depth_() * units.length,
	direction[2] * solid->depth_() * units.length);
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
    representation_matrix(representation, units.length, matrix);
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
    SDAI_Application_instance *item, SdaiAxis1_placement *axis,
    double angle_measure, ON_Brep *profile, SdaiRepresentation *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    if (!item || !axis || !profile || !representation ||
	!std::isfinite(angle_measure) || angle_measure <= 0.0) {
	delete profile;
	reason = "revolution has no profile/axis or has a non-positive angle";
	return false;
    }
    const double angle = angle_measure * units.plane_angle;
    const double angle_tolerance = 64.0 * ON_EPSILON * ON_PI;
    if (angle > 2.0 * ON_PI + angle_tolerance) {
	delete profile;
	reason = "revolution angle exceeds one complete turn";
	return false;
    }
    const bool full_revolution = std::fabs(angle - 2.0 * ON_PI) <= angle_tolerance;

    LocalUnits::length = units.length;
    LocalUnits::planeangle = units.plane_angle;
    LocalUnits::solidangle = units.solid_angle;
    ON_PolyCurve *generating_curve = outer_profile_curve(*profile, reason);
    delete profile;
    if (!generating_curve) return false;

    ON_NurbsCurve *nurbs_curve = ON_NurbsCurve::New();
    if (!generating_curve->GetNurbForm(*nurbs_curve) || !nurbs_curve->IsClosed()) {
	delete generating_curve;
	delete nurbs_curve;
	reason = "revolved profile could not be represented as an exact closed NURBS curve";
	return false;
    }
    delete generating_curve;

    double origin[3], direction[3];
    if (!swept_axis1_placement(axis, units.length, origin, direction)) {
	delete nurbs_curve;
	reason = "revolution axis placement is invalid";
	return false;
    }
    ON_Plane profile_plane;
    const ON_3dPoint axis_origin(origin);
    const ON_3dVector axis_direction(direction);
    if (!nurbs_curve->IsPlanar(&profile_plane, LocalUnits::tolerance) ||
	std::fabs(profile_plane.DistanceTo(axis_origin)) > LocalUnits::tolerance ||
	std::fabs(profile_plane.zaxis * axis_direction) > 64.0 * ON_EPSILON) {
	delete nurbs_curve;
	reason = "revolution axis does not lie in the swept profile plane";
	return false;
    }

    ON_RevSurface *surface = ON_RevSurface::New();
    surface->m_curve = nurbs_curve;
    surface->m_axis = ON_Line(axis_origin, axis_origin + axis_direction);
    surface->m_angle = ON_Interval(0.0, full_revolution ? 2.0 * ON_PI : angle);
    surface->m_t = surface->m_angle;
    surface->m_bTransposed = false;
    surface->BoundingBox();
    ON_Brep *brep = ON_BrepRevSurface(surface, false, false, NULL);
    if (!brep) {
	delete surface;
	reason = "openNURBS could not construct the analytic revolution BREP";
	return false;
    }
    if (!full_revolution) {
	ON_NurbsCurve start_boundary(*nurbs_curve);
	ON_NurbsCurve end_boundary(start_boundary);
	ON_Xform rotation;
	rotation.Rotation(angle, axis_direction, axis_origin);
	if (!end_boundary.Transform(rotation)) {
	    delete brep;
	    reason = "could not transform the exact revolution end-cap boundary";
	    return false;
	}
	ON_Plane end_plane(profile_plane);
	if (!end_plane.Transform(rotation)) {
	    delete brep;
	    reason = "could not transform the exact revolution end-cap plane";
	    return false;
	}

	/* ON_BrepRevSurface creates one naked closed edge at each angular end.
	 * Add planar faces whose single NURBS boundary exactly matches those
	 * edges, then merge the coincident topology. */
	if (!append_revolution_cap(*brep, profile_plane, start_boundary, false, reason) ||
	    !append_revolution_cap(*brep, end_plane, end_boundary, true, reason)) {
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
	    const int first_cap = 1;
	    const int second_cap = 2;
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
    representation_matrix(representation, units.length, matrix);
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
    SdaiRevolved_area_solid *solid, SdaiRepresentation *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    if (!solid || !representation || !solid->swept_area_() || !solid->axis_() ||
	!std::isfinite(solid->angle_()) || solid->angle_() <= 0.0) {
	reason = "revolution has no profile/axis or has a non-positive angle";
	return false;
    }
    LocalUnits::length = units.length;
    LocalUnits::planeangle = units.plane_angle;
    LocalUnits::solidangle = units.solid_angle;
    ON_Brep *profile = planar_profile(wrapper, solid->swept_area_(), units.length, reason);
    if (!profile) return false;
    return convert_profile_revolution(wrapper, database, solid, solid->axis_(),
	solid->angle_(), profile, representation, product_name, units, reason);
}

bool
convert_face_revolution(STEPWrapper &wrapper, BRLCADWrapper &database,
    SdaiRevolved_face_solid *solid, SdaiRepresentation *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    if (!solid || !representation || !solid->swept_face_() || !solid->axis_() ||
	!std::isfinite(solid->angle_()) || solid->angle_() <= 0.0) {
	reason = "face revolution has no face/axis or has a non-positive angle";
	return false;
    }

    LocalUnits::length = units.length;
    LocalUnits::planeangle = units.plane_angle;
    LocalUnits::solidangle = units.solid_angle;
    wrapper.ResetOpenNURBSState();
    STEPEntity *factory_object = Factory::CreateObject(&wrapper, solid->swept_face_());
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
    return convert_profile_revolution(wrapper, database, solid, solid->axis_(),
	solid->angle_(), profile, representation, product_name, units, reason);
}

bool
convert_swept_disk(STEPWrapper &wrapper, BRLCADWrapper &database,
    SdaiSwept_disk_solid *solid, SdaiRepresentation *representation,
    const std::string &product_name, const UnitFactors &units, std::string &reason)
{
    if (!solid || !representation || !solid->directrix_() ||
	!std::isfinite(solid->radius_()) || solid->radius_() <= 0.0 ||
	!std::isfinite(solid->inner_radius_()) || solid->inner_radius_() < 0.0 ||
	solid->inner_radius_() >= solid->radius_() ||
	!std::isfinite(solid->start_param_()) || !std::isfinite(solid->end_param_()) ||
	std::fabs(solid->start_param_() - solid->end_param_()) <=
	    ON_EPSILON * std::max(1.0, std::max(std::fabs(solid->start_param_()),
		std::fabs(solid->end_param_())))) {
	reason = "swept disk has an invalid directrix, radius, inner radius, or parameter range";
	return false;
    }

    LocalUnits::length = units.length;
    LocalUnits::planeangle = units.plane_angle;
    LocalUnits::solidangle = units.solid_angle;
    wrapper.ResetOpenNURBSState();
    ON_Brep curve_holder;
    STEPEntity *factory_object = Factory::CreateObject(&wrapper, solid->directrix_());
    Curve *directrix = dynamic_cast<Curve *>(factory_object);
    if (!directrix || !directrix->LoadONBrep(&curve_holder)) {
	reason = "swept disk directrix could not be materialized as an exact curve";
	return false;
    }
    const int curve_id = directrix->GetONId();
    const ON_Curve *curve = curve_id >= 0 && curve_id < curve_holder.m_C3.Count() ?
	curve_holder.m_C3[curve_id] : NULL;
    if (!curve || !ON_LineCurve::Cast(curve)) {
	reason = "only exact linear SWEPT_DISK_SOLID directrices are currently supported";
	return false;
    }
    const ON_Interval domain = curve->Domain();
    if (!domain.Includes(solid->start_param_()) || !domain.Includes(solid->end_param_())) {
	reason = "swept disk parameter range lies outside its directrix domain";
	return false;
    }

    const ON_3dPoint start = curve->PointAt(solid->start_param_());
    const ON_3dPoint end = curve->PointAt(solid->end_param_());
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
    if (!database.WriteRcc(outer_name, base, height, solid->radius_() * units.length,
	solid->STEPfile_id, original)) {
	reason = "BRL-CAD database rejected the exact swept-disk outer cylinder";
	return false;
    }

    mat_t matrix;
    representation_matrix(representation, units.length, matrix);
    if (!database.AddMember(item_name, outer_name, matrix)) {
	reason = "could not add the swept-disk outer cylinder to its region";
	return false;
    }
    if (solid->inner_radius_() > 0.0) {
	const std::string inner_name = item_name + "_inner.s";
	if (!database.WriteRcc(inner_name, base, height,
		solid->inner_radius_() * units.length, solid->STEPfile_id, original) ||
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
ConvertAP214SweptSolids(STEPWrapper &wrapper, BRLCADWrapper &database,
    const std::vector<uint64_t> &excluded_sdrs)
{
    std::set<int64_t> processed;
    wrapper.SetInstanceTypes({"SHAPE_DEFINITION_REPRESENTATION"}, excluded_sdrs);
    for (int index = 0; index < wrapper.InstanceCount(); ++index) {
	SDAI_Application_instance *instance = wrapper.InstanceAt(index);
	if (!instance || instance->STEPfile_id <= 0 ||
	    !instance->IsA(SCHEMA_NAMESPACE::e_shape_definition_representation))
	    continue;
	SdaiShape_definition_representation *link =
	    dynamic_cast<SdaiShape_definition_representation *>(instance);
	SdaiRepresented_definition *represented = link ? link->definition_() : NULL;
	SdaiProperty_definition *property = represented && represented->IsProperty_definition() == LTrue ?
	    static_cast<SdaiProperty_definition *>(*represented) : NULL;
	SdaiProduct_definition *definition = property ? swept_product_definition(property->definition_()) : NULL;
	SdaiRepresentation *representation = link ? link->used_representation_() : NULL;
	if (!definition || !representation || !representation->items_()) continue;
	const int64_t product_id = swept_product_id(definition);
	std::map<int64_t, brlcad::step::Product>::iterator product =
	    wrapper.Document().products.find(product_id);
	if (product == wrapper.Document().products.end() || product->second.output_name.empty()) continue;

	const UnitFactors units = representation_units(wrapper, representation);
	EntityNode *node = static_cast<EntityNode *>(representation->items_()->GetHead());
	while (node) {
	    SdaiSwept_area_solid *swept = dynamic_cast<SdaiSwept_area_solid *>(node->node);
	    SdaiSwept_face_solid *face_swept = dynamic_cast<SdaiSwept_face_solid *>(node->node);
	    SdaiSwept_disk_solid *disk = dynamic_cast<SdaiSwept_disk_solid *>(node->node);
	    SDAI_Application_instance *geometry = swept ?
		static_cast<SDAI_Application_instance *>(swept) :
		(face_swept ? static_cast<SDAI_Application_instance *>(face_swept) :
		static_cast<SDAI_Application_instance *>(disk));
	    if (!geometry || !processed.insert(geometry->STEPfile_id).second) {
		node = static_cast<EntityNode *>(node->NextNode());
		continue;
	    }
	    if (!wrapper.ShouldConvertEntity(geometry->STEPfile_id)) {
		node = static_cast<EntityNode *>(node->NextNode());
		continue;
	    }
	    ++wrapper.Statistics().geometry_attempted;
	    bool written = false;
	    std::string failure_reason;
	    std::string type = face_swept ? "SWEPT_FACE_SOLID" : "SWEPT_AREA_SOLID";
	    if (disk) {
		type = "SWEPT_DISK_SOLID";
		written = convert_swept_disk(wrapper, database, disk, representation,
		    product->second.output_name, units, failure_reason);
	    } else if (SdaiExtruded_area_solid *extruded =
		dynamic_cast<SdaiExtruded_area_solid *>(swept)) {
		type = "EXTRUDED_AREA_SOLID";
		written = convert_extrusion(wrapper, database, extruded, representation,
		    product->second.output_name, units, failure_reason);
	    } else if (SdaiRevolved_area_solid *revolved =
		dynamic_cast<SdaiRevolved_area_solid *>(swept)) {
		type = "REVOLVED_AREA_SOLID";
		written = convert_revolution(wrapper, database, revolved, representation,
		    product->second.output_name, units, failure_reason);
	    } else if (SdaiExtruded_face_solid *extruded_face =
		dynamic_cast<SdaiExtruded_face_solid *>(face_swept)) {
		type = "EXTRUDED_FACE_SOLID";
		written = convert_face_extrusion(wrapper, database, extruded_face,
		    representation, product->second.output_name, units, failure_reason);
	    } else if (SdaiRevolved_face_solid *revolved_face =
		dynamic_cast<SdaiRevolved_face_solid *>(face_swept)) {
		type = "REVOLVED_FACE_SOLID";
		written = convert_face_revolution(wrapper, database, revolved_face,
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
	    } else {
		++wrapper.Statistics().geometry_skipped;
		wrapper.RecordSkippedItem(geometry->STEPfile_id, type,
		    failure_reason.empty() ?
		    "exact swept-area conversion failed validation" : failure_reason);
		wrapper.RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		    geometry->STEPfile_id, type, std::string(),
		    failure_reason.empty() ?
		    "exact swept-area conversion failed validation" : failure_reason);
	    }
	    node = static_cast<EntityNode *>(node->NextNode());
	}
    }
    wrapper.ClearEntityCache();
    wrapper.ResetInstanceTypes();
}
