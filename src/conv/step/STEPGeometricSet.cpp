/*                 S T E P G E O M E T R I C S E T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
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
 *
 * Schema-neutral, edition-independent GEOMETRIC_SET conversion.  This build
 * unit intentionally
 * owns curve, point, and bounded-surface set materialization so work on these
 * exchange representations does not rebuild the solid BRep repair engine or
 * the document traversal and scheduling pipeline.
 */

#include "common.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "brep.h"
#include "bn/mat.h"
#include "vmath.h"

#include "STEPGeometricSet.h"
#include "STEPWrapper.h"
#include "step-g/OpenNurbsInterfaces.h"

#include "Axis2Placement3D.h"
#include "BoundaryCurve.h"
#include "BoundedSurface.h"
#include "CompositeCurve.h"
#include "CompositeCurveSegment.h"
#include "Curve.h"
#include "CurveBoundedSurface.h"
#include "DefinitionalRepresentation.h"
#include "GeometricSet.h"
#include "GeometricSetSelect.h"
#include "LocalUnits.h"
#include "PCurve.h"
#include "PCurveOrSurface.h"
#include "Plane.h"
#include "Point.h"
#include "Representation.h"
#include "Surface.h"
#include "SurfaceCurve.h"

namespace {

constexpr int kPcurveLocusScreeningSegments = 64;

static const brlcad::step::Style *
style_for_item(STEPWrapper *wrapper, int64_t item_id)
{
    const std::map<int64_t, brlcad::step::Style> &styles =
        wrapper->Document().styles;
    const std::map<int64_t, brlcad::step::Style>::const_iterator found =
        styles.find(item_id);
    return found == styles.end() ? NULL : &found->second;
}


static void
representation_matrix(Representation *representation, mat_t matrix)
{
    MAT_IDN(matrix);
    if (!representation)
        return;

    Axis2Placement3D *axis = NULL;
    LIST_OF_REPRESENTATION_ITEMS *items = representation->items_();
    if (items) {
        for (LIST_OF_REPRESENTATION_ITEMS::iterator item = items->begin();
                item != items->end(); ++item) {
            axis = dynamic_cast<Axis2Placement3D *>(*item);
            if (axis)
                break;
        }
    }
    if (!axis)
        return;

    const double length_factor = representation->GetLengthConversionFactor();
    vect_t translation;
    mat_t rotation;
    VMOVE(translation, axis->GetOrigin());
    VSCALE(translation, translation, length_factor);
    MAT_IDN(rotation);
    VMOVE(&rotation[0], axis->GetXAxis());
    VMOVE(&rotation[4], axis->GetYAxis());
    VMOVE(&rotation[8], axis->GetZAxis());
    bn_mat_inv(matrix, rotation);
    MAT_DELTAS_VEC(matrix, translation);
}


static ON_3dPoint
closed_surface_point_at(const ON_Surface *surface, const ON_3dPoint &parameter)
{
    if (!surface || !parameter.IsValid())
        return ON_3dPoint::UnsetPoint;
    ON_3dPoint native(parameter);
    for (int direction = 0; direction < 2; ++direction) {
        if (!surface->IsClosed(direction))
            continue;
        const ON_Interval domain = surface->Domain(direction);
        const double period = domain.Length();
        if (!(period > ON_ZERO_TOLERANCE) ||
                (native[direction] >= domain.Min() &&
                 native[direction] <= domain.Max()))
            continue;
        double wrapped = fmod(native[direction] - domain.Min(), period);
        if (wrapped < 0.0)
            wrapped += period;
        native[direction] = domain.Min() + wrapped;
    }
    return surface->PointAt(native.x, native.y);
}

} // namespace


bool
step_geometric_set_has_curves(GeometricSet *set)
{
    if (!set || !set->GetElements())
	return false;
    for (LIST_OF_GEOMETRIC_SET_SELECT::const_iterator item = set->GetElements()->begin();
	 item != set->GetElements()->end(); ++item) {
	if (*item && (*item)->GetCurveElement())
	    return true;
    }
    return false;
}


bool
step_geometric_set_has_points(GeometricSet *set)
{
    if (!set || !set->GetElements())
	return false;
    for (LIST_OF_GEOMETRIC_SET_SELECT::const_iterator item = set->GetElements()->begin();
	 item != set->GetElements()->end(); ++item) {
	if (*item && (*item)->GetPointElement())
	    return true;
    }
    return false;
}


bool
step_geometric_set_has_surfaces(GeometricSet *set)
{
    if (!set || !set->GetElements())
	return false;
    for (LIST_OF_GEOMETRIC_SET_SELECT::const_iterator item =
	    set->GetElements()->begin(); item != set->GetElements()->end(); ++item) {
	if (*item && (*item)->GetSurfaceElement())
	    return true;
    }
    return false;
}


static BrepWriteStatus
convert_WriteWireSet(GeometricSet *set, Representation *representation,
    STEPWrapper *wrapper, BRLCADWrapper *dotg, std::string *name, int dry_run,
    const brlcad::step::Style *style_override = NULL)
{
    if (!set || !representation || !wrapper || !dotg || !name)
	return BREP_CONVERSION_FAILED;

    LocalUnits::length = representation->GetLengthConversionFactor();
    LocalUnits::planeangle = representation->GetPlaneAngleConversionFactor();
    LocalUnits::solidangle = representation->GetSolidAngleConversionFactor();

    wrapper->ResetOpenNURBSState();
    ON_Brep *wire = new ON_Brep();
    size_t curve_count = 0;
    LIST_OF_GEOMETRIC_SET_SELECT *elements = set->GetElements();
    if (elements) {
	for (LIST_OF_GEOMETRIC_SET_SELECT::iterator element = elements->begin();
	     element != elements->end(); ++element) {
	    Curve *curve = *element ? (*element)->GetCurveElement() : NULL;
	    if (!curve)
		continue;
	    if (!curve->LoadONBrep(wire)) {
		delete wire;
		return BREP_CONVERSION_FAILED;
	    }
	    const int curve_index = curve->GetONId();
	    if (curve_index < 0 || curve_index >= wire->m_C3.Count() || !wire->m_C3[curve_index]) {
		delete wire;
		return BREP_CONVERSION_FAILED;
	    }
	    ON_Curve *geometry = wire->m_C3[curve_index];
	    const ON_Interval domain = geometry->Domain();
	    const ON_3dPoint start = geometry->PointAt(domain.Min());
	    const ON_3dPoint end = geometry->PointAt(domain.Max());
	    const int start_vertex = wire->NewVertex(start, LocalUnits::tolerance).m_vertex_index;
	    int end_vertex = start_vertex;
	    if (start.DistanceTo(end) > LocalUnits::tolerance)
		end_vertex = wire->NewVertex(end, LocalUnits::tolerance).m_vertex_index;
	    ON_BrepEdge &edge = wire->NewEdge(wire->m_V[start_vertex],
		wire->m_V[end_vertex], curve_index);
	    edge.m_tolerance = LocalUnits::tolerance;
	    ++curve_count;
	}
    }
    if (!curve_count) {
	delete wire;
	return BREP_CONVERSION_FAILED;
    }

    /* Curve replicas and composite curves may materialize dependency curves
     * before adding their final exact curve.  Only representation-item curves
     * receive edges; discard the unreferenced dependency copies. */
    if (!wire->Compact()) {
	delete wire;
	return BREP_CONVERSION_FAILED;
    }

    wire->SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, true, true);
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!wire->IsValid(&validation_log)) {
	if (wrapper->Verbose()) {
	    ON_String validation_text(validation_messages);
	    std::cerr << validation_text.Array();
	}
	delete wire;
	return BREP_INVALID_STRUCTURE;
    }

    mat_t mat;
    representation_matrix(representation, mat);
    const brlcad::step::Style *style = style_override ? style_override : style_for_item(wrapper, set->GetId());
    if (!style)
	style = style_for_item(wrapper, representation->GetId());
    const bool written = dry_run || dotg->WriteBrep(*name, wire, mat, false,
	set->GetId(), set->Name(), style);
    if (written && style)
	++wrapper->Statistics().styles_applied;
    delete wire;
    return written ? BREP_WRITE_SUCCESS : BREP_OUTPUT_FAILED;
}


static BrepWriteStatus
convert_WritePointSet(GeometricSet *set, Representation *representation,
    STEPWrapper *wrapper, BRLCADWrapper *dotg, const std::string &name,
    int dry_run)
{
    if (!set || !representation || !wrapper || !dotg || name.empty())
	return BREP_CONVERSION_FAILED;

    mat_t matrix;
    representation_matrix(representation, matrix);
    const double length_factor = representation->GetLengthConversionFactor();
    std::vector<double> coordinates;
    LIST_OF_GEOMETRIC_SET_SELECT *elements = set->GetElements();
    if (elements) {
	for (LIST_OF_GEOMETRIC_SET_SELECT::const_iterator element = elements->begin();
	     element != elements->end(); ++element) {
	    Point *point = *element ? (*element)->GetPointElement() : NULL;
	    if (!point) continue;
	    const double *source = point->Point3d();
	    /* POINT_ON_CURVE and other implicit point forms require evaluation
	     * semantics which this exact path does not yet provide. */
	    if (!source) return BREP_CONVERSION_FAILED;
	    point_t local = {
		source[0] * length_factor,
		source[1] * length_factor,
		source[2] * length_factor
	    };
	    point_t transformed;
	    MAT4X3PNT(transformed, matrix, local);
	    coordinates.push_back(transformed[0]);
	    coordinates.push_back(transformed[1]);
	    coordinates.push_back(transformed[2]);
	}
    }
    if (coordinates.empty()) return BREP_CONVERSION_FAILED;
    const bool written = dry_run || dotg->WritePoints(name,
	coordinates.size() / 3, coordinates.data(), 1.0, set->GetId(), set->Name());
    return written ? BREP_WRITE_SUCCESS : BREP_OUTPUT_FAILED;
}


/* Resolve the parameter-space curve carried by PCURVE and SURFACE_CURVE
 * selects.  STEP parameter coordinates are dimensionless surface parameters,
 * so their ordinary curve adapters must run with a unit length factor.  The
 * caller resets all OpenNURBS ids for each transactional surface candidate;
 * reference curves loaded here therefore cannot inherit a prior 3-D scale. */
static bool
geometric_set_parameter_curve(Curve *source, Surface *basis, ON_Brep *brep,
    std::unique_ptr<ON_Curve> &result)
{
    result.reset();
    if (!source || !basis || !brep)
	return false;

    PCurve *pcurve = dynamic_cast<PCurve *>(source);
    if (pcurve) {
	Surface *pcurve_basis = pcurve->BasisSurface();
	DefinitionalRepresentation *definition = pcurve->ReferenceToCurve();
	if (!pcurve_basis || pcurve_basis->GetId() != basis->GetId() ||
		!definition || !definition->items_())
	    return false;
	Curve *definition_curve = NULL;
	for (LIST_OF_REPRESENTATION_ITEMS::const_iterator item =
		definition->items_()->begin(); item != definition->items_()->end();
	     ++item) {
	    Curve *candidate = dynamic_cast<Curve *>(*item);
	    if (!candidate)
		continue;
	    if (definition_curve)
		return false;
	    definition_curve = candidate;
	}
	if (!definition_curve)
	    return false;
	const double saved_length = LocalUnits::length;
	LocalUnits::length = 1.0;
	const bool loaded = definition_curve->LoadONBrep(brep);
	LocalUnits::length = saved_length;
	const int curve_index = loaded ? definition_curve->GetONId() : -1;
	if (curve_index < 0 || curve_index >= brep->m_C3.Count() ||
		!brep->m_C3[curve_index])
	    return false;
	result.reset(brep->m_C3[curve_index]->DuplicateCurve());
	return result && result->ChangeDimension(2) && result->IsValid();
    }

    SurfaceCurve *surface_curve = dynamic_cast<SurfaceCurve *>(source);
    if (surface_curve) {
	for (LIST_OF_PCURVE_OR_SURFACE::const_iterator associated =
		surface_curve->AssociatedGeometry().begin();
	     associated != surface_curve->AssociatedGeometry().end(); ++associated) {
	    PCurve *candidate = *associated ? (*associated)->GetPCurve() : NULL;
	    if (!candidate || !candidate->BasisSurface() ||
		    candidate->BasisSurface()->GetId() != basis->GetId())
		continue;
	    return geometric_set_parameter_curve(candidate, basis, brep, result);
	}
	return false;
    }

    CompositeCurve *composite = dynamic_cast<CompositeCurve *>(source);
    if (!composite || composite->Segments().empty())
	return false;
    std::unique_ptr<ON_PolyCurve> joined(new ON_PolyCurve());
    joined->Reserve(static_cast<int>(composite->Segments().size()));
    for (LIST_OF_SEGMENTS::const_iterator segment =
	    composite->Segments().begin(); segment != composite->Segments().end();
	 ++segment) {
	Curve *parent = *segment ? (*segment)->ParentCurve() : NULL;
	const Boolean same_sense = *segment ? (*segment)->SameSense() : BUnset;
	std::unique_ptr<ON_Curve> piece;
	if (!parent || same_sense == BUnset ||
		!geometric_set_parameter_curve(parent, basis, brep, piece) ||
		(same_sense == BFalse && !piece->Reverse()) ||
		!joined->Append(piece.release()))
	    return false;
    }
    if (!joined->IsValid())
	return false;
    result = std::move(joined);
    return true;
}


static bool
geometric_set_model_boundary_curve(BoundaryCurve *boundary,
    STEPWrapper *wrapper, ON_Brep *brep, int *curve_index,
    std::string *failure)
{
    if (curve_index)
	*curve_index = -1;
    if (!boundary || !wrapper || !brep || !curve_index ||
	    boundary->Segments().empty()) {
	if (failure)
	    *failure = "a boundary had no composite-curve segments";
	return false;
    }

    std::vector<std::unique_ptr<ON_Curve> > pieces;
    pieces.reserve(boundary->Segments().size());
    for (LIST_OF_SEGMENTS::const_iterator segment =
	    boundary->Segments().begin(); segment != boundary->Segments().end();
	 ++segment) {
	Curve *parent = *segment ? (*segment)->ParentCurve() : NULL;
	const Boolean same_sense = *segment ? (*segment)->SameSense() : BUnset;
	if (!parent || same_sense == BUnset || !parent->LoadONBrep(brep)) {
	    if (failure)
		*failure = "a boundary segment did not provide its 3-D curve";
	    return false;
	}
	const int parent_index = parent->GetONId();
	ON_Curve *source = parent_index >= 0 && parent_index < brep->m_C3.Count() ?
	    brep->m_C3[parent_index] : NULL;
	std::unique_ptr<ON_Curve> piece(source ? source->DuplicateCurve() : NULL);
	if (!piece || (same_sense == BFalse && !piece->Reverse()) ||
		!piece->IsValid()) {
	    if (failure)
		*failure = "a boundary segment's 3-D curve was invalid";
	    return false;
	}
	pieces.push_back(std::move(piece));
    }

    std::vector<double> join_gaps(pieces.size(), 0.0);
    std::vector<ON_3dPoint> piece_starts(pieces.size());
    std::vector<ON_3dPoint> piece_ends(pieces.size());
    size_t repaired_joins = 0;
    for (size_t index = 0; index < pieces.size(); ++index) {
	const ON_3dPoint end = pieces[index]->PointAtEnd();
	const ON_3dPoint start = pieces[(index + 1) % pieces.size()]->PointAtStart();
	piece_starts[index] = pieces[index]->PointAtStart();
	piece_ends[index] = end;
	double gap = end.IsValid() && start.IsValid() ?
	    end.DistanceTo(start) : DBL_MAX;
	const double original_gap = gap;
	if (!std::isfinite(gap) || gap > LocalUnits::tolerance) {
	    if (failure) {
		std::ostringstream reason;
		reason << "a boundary segment join exceeded the model tolerance"
		    << " (gap " << gap << ", tolerance "
		    << LocalUnits::tolerance << ')';
		*failure = reason.str();
	    }
	    return false;
	}
	if (gap > ON_ZERO_TOLERANCE &&
		wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe) {
	    if (failure)
		*failure = "a boundary segment join required safe tolerance repair";
	    return false;
	}
	if (gap > ON_ZERO_TOLERANCE && wrapper->ImportOptions().repair ==
		brlcad::step::RepairMode::Safe) {
	    const ON_3dPoint midpoint = 0.5 * (end + start);
	    std::unique_ptr<ON_Curve> first(pieces[index]->DuplicateCurve());
	    std::unique_ptr<ON_Curve> second(
		pieces[(index + 1) % pieces.size()]->DuplicateCurve());
	    if (first && second && first->SetEndPoint(midpoint) &&
		    second->SetStartPoint(midpoint) && first->IsValid() &&
		    second->IsValid()) {
		pieces[index] = std::move(first);
		pieces[(index + 1) % pieces.size()] = std::move(second);
		piece_ends[index] = midpoint;
		piece_starts[(index + 1) % pieces.size()] = midpoint;
		gap = 0.0;
	    }
	}
	join_gaps[index] = gap;
	repaired_joins += original_gap > ON_ZERO_TOLERANCE ? 1 : 0;
    }

    std::unique_ptr<ON_PolyCurve> joined(new ON_PolyCurve());
    joined->Reserve(static_cast<int>(pieces.size() + repaired_joins));
    for (size_t index = 0; index < pieces.size(); ++index) {
	ON_Curve *piece = pieces[index].release();
	if (!joined->Append(piece)) {
	    delete piece;
	    if (failure)
		*failure = "a boundary segment could not be appended";
	    return false;
	}
	if (join_gaps[index] <= ON_ZERO_TOLERANCE)
	    continue;
	const ON_3dPoint end = piece_ends[index];
	const ON_3dPoint start = piece_starts[(index + 1) % pieces.size()];
	std::unique_ptr<ON_LineCurve> connector(
	    end.IsValid() && start.IsValid() ? new ON_LineCurve(end, start) : NULL);
	if (!connector || !connector->IsValid() ||
		!joined->Append(connector.get())) {
	    if (failure)
		*failure = "a tolerance-bounded boundary connector was invalid";
	    return false;
	}
	connector.release();
    }
    if (!joined->IsValid() || !joined->IsClosed()) {
	if (failure)
	    *failure = "the assembled 3-D boundary was not a valid closed curve";
	return false;
    }
    *curve_index = brep->AddEdgeCurve(joined.release());
    if (*curve_index < 0)
	return false;
    if (repaired_joins)
	wrapper->RecordRepair(boundary->GetId(), "BOUNDARY_CURVE", "segments",
	    "bridged a composite boundary join within the declared model tolerance");
    return true;
}


static bool
build_curve_bounded_surface(CurveBoundedSurface *bounded,
    STEPWrapper *wrapper, ON_Brep *result, std::string *failure)
{
    if (failure)
	failure->clear();
    if (!bounded || !wrapper || !result || !bounded->BasisSurface() ||
	    bounded->Boundaries().empty() || bounded->ImplicitOuter() == BUnset)
	return false;

    wrapper->ResetOpenNURBSState();
    std::unique_ptr<ON_Brep> candidate(new ON_Brep());
    ON_BoundingBox edge_bounds;
    bool have_edge_bounds = false;
    std::vector<int> boundary_curve_indices;
    boundary_curve_indices.reserve(bounded->Boundaries().size());
    for (LIST_OF_BOUNDARIES::const_iterator boundary =
	    bounded->Boundaries().begin(); boundary != bounded->Boundaries().end();
	 ++boundary) {
	int curve_index = -1;
	if (!geometric_set_model_boundary_curve(*boundary, wrapper,
		candidate.get(), &curve_index, failure)) {
	    return false;
	}
	boundary_curve_indices.push_back(curve_index);
	ON_BoundingBox box;
	if (curve_index < 0 || curve_index >= candidate->m_C3.Count() ||
		!candidate->m_C3[curve_index] ||
		!candidate->m_C3[curve_index]->GetBoundingBox(box)) {
	    if (failure) *failure = "a boundary curve had no finite 3-D bounds";
	    return false;
	}
	if (have_edge_bounds)
	    edge_bounds.Union(box);
	else {
	    edge_bounds = box;
	    have_edge_bounds = true;
	}
    }

    Surface *basis = bounded->BasisSurface();
    basis->SetCurveBounds(have_edge_bounds ? &edge_bounds : NULL);
    if (!have_edge_bounds || !basis->LoadONBrep(candidate.get())) {
	if (failure) *failure = "the bounded surface basis could not be constructed";
	return false;
    }
    const int surface_index = basis->GetONId();
    if (surface_index < 0 || surface_index >= candidate->m_S.Count() ||
	    !candidate->m_S[surface_index])
	return false;
    ON_BrepFace &face = candidate->NewFace(surface_index);

    struct LoopRecord {
	int loop_index;
	bool declared_outer;
	double box_area;
    };
    std::vector<LoopRecord> loops;
    int declared_outer_count = 0;
    size_t boundary_offset = 0;
    for (LIST_OF_BOUNDARIES::const_iterator boundary =
	    bounded->Boundaries().begin(); boundary != bounded->Boundaries().end();
	 ++boundary, ++boundary_offset) {
	const int curve_index = boundary_offset < boundary_curve_indices.size() ?
	    boundary_curve_indices[boundary_offset] : -1;
	ON_Curve *edge_curve = curve_index >= 0 &&
	    curve_index < candidate->m_C3.Count() ?
	    candidate->m_C3[curve_index] : NULL;
	if (!edge_curve)
	    return false;
	ON_3dPoint edge_start = edge_curve->PointAtStart();
	ON_3dPoint edge_end = edge_curve->PointAtEnd();
	const double endpoint_gap = edge_start.IsValid() && edge_end.IsValid() ?
	    edge_start.DistanceTo(edge_end) : DBL_MAX;
	if (endpoint_gap > LocalUnits::tolerance) {
	    if (failure) *failure = "a boundary curve was not closed in model space";
	    return false;
	}

	std::unique_ptr<ON_Curve> pcurve;
	if (!geometric_set_parameter_curve(*boundary, basis, candidate.get(),
		pcurve)) {
	    if (failure) *failure = "a boundary had no pcurve for its basis surface";
	    return false;
	}
	ON_3dPoint uv_start = pcurve->PointAtStart();
	ON_3dPoint uv_end = pcurve->PointAtEnd();
	const ON_Surface *surface = candidate->m_S[surface_index];
	ON_3dPoint start_lift = uv_start.IsValid() ?
	    closed_surface_point_at(surface, uv_start) : ON_3dPoint::UnsetPoint;
	ON_3dPoint end_lift = uv_end.IsValid() ?
	    closed_surface_point_at(surface, uv_end) : ON_3dPoint::UnsetPoint;
	bool regenerated_internal_pcurve = false;
	/* STEP analytic pcurves use their schema-defined natural parameters.
	 * Private OpenNURBS adapters may use different finite domains or periodic
	 * branches.  If the authored coordinates do not lift to their immutable
	 * 3-D boundary, use the shared face-import pullback to express that same
	 * locus in the adapter's private parameterization. */
	if ((!start_lift.IsValid() || !end_lift.IsValid() ||
		start_lift.DistanceTo(edge_start) > LocalUnits::tolerance ||
		end_lift.DistanceTo(edge_end) > LocalUnits::tolerance)) {
	    std::string pullback_failure;
	    ON_Curve *internal_pcurve = step_curve_surface_pcurve(surface,
		edge_curve, LocalUnits::tolerance, &pullback_failure);
	    if (internal_pcurve) {
		pcurve.reset(internal_pcurve);
		uv_start = pcurve->PointAtStart();
		uv_end = pcurve->PointAtEnd();
		start_lift = closed_surface_point_at(surface, uv_start);
		end_lift = closed_surface_point_at(surface, uv_end);
		regenerated_internal_pcurve = true;
		wrapper->RecordRepair((*boundary)->GetId(), "BOUNDARY_CURVE",
		    "pcurve", "mapped an analytic boundary into the private "
		    "OpenNURBS surface parameterization");
	    }
	}
	/* A closed STEP boundary has no semantically preferred parameter seam.
	 * Its 3-D composite and its pcurve composite may therefore start at
	 * different points on the same closed locus.  Align the private 3-D curve
	 * seam to the pcurve start exactly before creating their shared topology;
	 * never move either locus or accept a nearest point outside tolerance. */
	if (start_lift.IsValid() && end_lift.IsValid() && edge_curve->IsClosed() &&
		start_lift.DistanceTo(end_lift) <= LocalUnits::tolerance &&
		(start_lift.DistanceTo(edge_start) > LocalUnits::tolerance ||
		 end_lift.DistanceTo(edge_end) > LocalUnits::tolerance)) {
	    std::unique_ptr<ON_Curve> relocated(step_closed_curve_with_seam_at(
		edge_curve, start_lift, LocalUnits::tolerance));
	    if (relocated && relocated->PointAtEnd().DistanceTo(end_lift) <=
		    LocalUnits::tolerance) {
		delete candidate->m_C3[curve_index];
		candidate->m_C3[curve_index] = relocated.release();
		edge_curve = candidate->m_C3[curve_index];
		edge_start = edge_curve->PointAtStart();
		edge_end = edge_curve->PointAtEnd();
		wrapper->RecordRepair((*boundary)->GetId(), "BOUNDARY_CURVE",
		    "segments", "aligned the arbitrary seam of a closed 3-D "
		    "boundary with its authored pcurve");
	    }
	}
	if (!start_lift.IsValid() || !end_lift.IsValid() ||
		start_lift.DistanceTo(edge_start) > LocalUnits::tolerance ||
		end_lift.DistanceTo(edge_end) > LocalUnits::tolerance) {
	    if (failure) {
		std::ostringstream reason;
		reason << "an authored boundary pcurve did not meet its 3-D "
		    << "boundary endpoints (boundary #" << (*boundary)->GetId()
		    << ", distances " << start_lift.DistanceTo(edge_start)
		    << '/' << end_lift.DistanceTo(edge_end) << ", reverse "
		    << start_lift.DistanceTo(edge_end) << '/'
		    << end_lift.DistanceTo(edge_start) << ", tolerance "
		    << LocalUnits::tolerance << ')';
		*failure = reason.str();
	    }
	    return false;
	}
	bool periodic_parameter_closure =
	    start_lift.DistanceTo(end_lift) <= LocalUnits::tolerance;
	for (int direction = 0; periodic_parameter_closure && direction < 2;
		direction++) {
	    const double difference = uv_end[direction] - uv_start[direction];
	    if (fabs(difference) <= ON_ZERO_TOLERANCE)
		continue;
	    const ON_Interval parameter_domain = surface->Domain(direction);
	    const double period = parameter_domain.Length();
	    periodic_parameter_closure = surface->IsClosed(direction) &&
		period > ON_ZERO_TOLERANCE &&
		fabs(difference - round(difference / period) * period) <=
		    ON_ZERO_TOLERANCE;
	}
	if (uv_start.DistanceTo(uv_end) > ON_ZERO_TOLERANCE &&
		!periodic_parameter_closure) {
	    if (wrapper->ImportOptions().repair !=
		    brlcad::step::RepairMode::Safe ||
		    start_lift.DistanceTo(end_lift) > LocalUnits::tolerance ||
		    !pcurve->SetEndPoint(uv_start)) {
		if (failure) *failure = "a boundary pcurve was not closed";
		return false;
	    }
	    wrapper->RecordRepair(bounded->GetId(), "CURVE_BOUNDED_SURFACE",
		"boundaries", "closed a topology-proven boundary pcurve endpoint");
	}

	/* Structural validity alone does not prove that an authored pcurve trims
	 * the advertised 3-D boundary.  In particular, SetEndPoint() can make a
	 * topologically closed polycurve whose last span no longer lies on the
	 * basis surface/edge intersection.  Densely lift the complete candidate
	 * and require every sample to remain on the immutable boundary locus. */
	std::vector<ON_3dPoint> lifted_boundary;
	size_t rejected_sample = 0;
	double rejected_distance = DBL_MAX;
	const auto pcurve_preserves_boundary = [&]() {
	    lifted_boundary.assign(kPcurveLocusScreeningSegments + 1,
		ON_3dPoint::UnsetPoint);
	    const ON_Interval pcurve_domain = pcurve->Domain();
	    bool finite_lifts = pcurve_domain.IsIncreasing();
	    for (int sample = 0; finite_lifts &&
		    sample <= kPcurveLocusScreeningSegments; ++sample) {
		const ON_3dPoint uv = pcurve->PointAt(pcurve_domain.ParameterAt(
		    static_cast<double>(sample) /
			kPcurveLocusScreeningSegments));
		lifted_boundary[sample] = uv.IsValid() ?
		    closed_surface_point_at(surface, uv) : ON_3dPoint::UnsetPoint;
		finite_lifts = lifted_boundary[sample].IsValid();
	    }
	    rejected_sample = 0;
	    rejected_distance = DBL_MAX;
	    return finite_lifts && step_curve_locus_contains_points(edge_curve,
		lifted_boundary.data(), lifted_boundary.size(),
		LocalUnits::tolerance, &rejected_sample, &rejected_distance);
	};
	bool pcurve_valid = pcurve_preserves_boundary();
	if (!pcurve_valid && !regenerated_internal_pcurve) {
	    std::string pullback_failure;
	    std::unique_ptr<ON_Curve> internal(step_curve_surface_pcurve(surface,
		edge_curve, LocalUnits::tolerance, &pullback_failure));
	    if (internal) {
		pcurve = std::move(internal);
		uv_start = pcurve->PointAtStart();
		uv_end = pcurve->PointAtEnd();
		start_lift = closed_surface_point_at(surface, uv_start);
		end_lift = closed_surface_point_at(surface, uv_end);
		if (start_lift.IsValid() && end_lift.IsValid() &&
			start_lift.DistanceTo(edge_start) <= LocalUnits::tolerance &&
			end_lift.DistanceTo(edge_end) <= LocalUnits::tolerance) {
		    pcurve_valid = pcurve_preserves_boundary();
		    if (pcurve_valid)
			wrapper->RecordRepair((*boundary)->GetId(),
			    "BOUNDARY_CURVE", "pcurve",
			    "regenerated an authored pcurve from its exact 3-D "
			    "boundary after complete locus validation");
		}
	    }
	}
	if (!pcurve_valid) {
	    if (failure) {
		std::ostringstream reason;
		reason << "a boundary pcurve left its authored 3-D curve locus";
		if (std::isfinite(rejected_distance))
		    reason << " at sample " << rejected_sample << '/'
			<< kPcurveLocusScreeningSegments << " (distance "
			<< rejected_distance << ", tolerance "
			<< LocalUnits::tolerance << ')';
		*failure = reason.str();
	    }
	    return false;
	}

	const int vertex_index = candidate->NewVertex(edge_start,
	    std::max(LocalUnits::tolerance, endpoint_gap)).m_vertex_index;
	ON_BrepEdge &edge = candidate->NewEdge(candidate->m_V[vertex_index],
	    candidate->m_V[vertex_index], curve_index, NULL,
	    std::max(LocalUnits::tolerance, endpoint_gap));
	const int c2 = candidate->AddTrimCurve(pcurve.release());
	if (c2 < 0)
	    return false;
	const bool declared_outer = (*boundary)->IsOuterBoundary();
	declared_outer_count += declared_outer ? 1 : 0;
	ON_BrepLoop &loop = candidate->NewLoop(ON_BrepLoop::unknown, face);
	ON_BrepTrim &trim = candidate->NewTrim(edge, false, loop, c2);
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_tolerance[0] = LocalUnits::tolerance;
	trim.m_tolerance[1] = LocalUnits::tolerance;
	candidate->SetTrimIsoFlags(trim);
	ON_BoundingBox uv_box;
	const bool have_uv_box = trim.TrimCurveOf() &&
	    trim.TrimCurveOf()->GetBoundingBox(uv_box);
	const double box_area = have_uv_box ?
	    fabs((uv_box.m_max.x - uv_box.m_min.x) *
		(uv_box.m_max.y - uv_box.m_min.y)) : 0.0;
	loops.push_back({loop.m_loop_index, declared_outer, box_area});
    }

    if (declared_outer_count > 1 ||
	    (bounded->ImplicitOuter() == BTrue && declared_outer_count != 0)) {
	if (failure) *failure = "the explicit and implicit outer-boundary semantics conflict";
	return false;
    }
    int outer_loop = -1;
    if (bounded->ImplicitOuter() == BTrue) {
	ON_BrepLoop *implicit = candidate->NewOuterLoop(face.m_face_index);
	if (!implicit) {
	    if (failure) *failure = "the bounded basis could not supply its implicit outer loop";
	    return false;
	}
	outer_loop = implicit->m_loop_index;
    } else {
	for (std::vector<LoopRecord>::const_iterator loop = loops.begin();
		loop != loops.end(); ++loop)
	    if (loop->declared_outer)
		outer_loop = loop->loop_index;
	if (outer_loop < 0) {
	    std::vector<LoopRecord>::const_iterator largest = std::max_element(
		loops.begin(), loops.end(), [](const LoopRecord &first,
		    const LoopRecord &second) { return first.box_area < second.box_area; });
	    if (largest == loops.end() || !(largest->box_area > 0.0)) {
		if (failure) *failure = "no explicit or inferable outer boundary was present";
		return false;
	    }
	    outer_loop = largest->loop_index;
	    wrapper->RecordRepair(bounded->GetId(), "CURVE_BOUNDED_SURFACE",
		"implicit_outer", "identified the containing authored boundary as outer");
	}
    }

    for (std::vector<LoopRecord>::const_iterator record = loops.begin();
	    record != loops.end(); ++record) {
	ON_BrepLoop &loop = candidate->m_L[record->loop_index];
	loop.m_type = record->loop_index == outer_loop ?
	    ON_BrepLoop::outer : ON_BrepLoop::inner;
	const int direction = candidate->LoopDirection(loop);
	const int required_direction = loop.m_type == ON_BrepLoop::outer ? 1 : -1;
	if (!direction) {
	    if (failure) *failure = "a parameter-space boundary was not a closed loop";
	    return false;
	}
	if (direction != required_direction)
	    candidate->FlipLoop(loop);
    }
    candidate->SortFaceLoops(face);
    candidate->SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, true, true);
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!candidate->Compact() || !candidate->IsValid(&validation_log)) {
	if (failure) {
	    ON_String text(validation_messages);
	    *failure = text.Array() ? text.Array() :
		"OpenNURBS rejected the bounded-surface topology";
	}
	return false;
    }
    result->Append(*candidate);
    return true;
}


static bool
build_geometric_set_surface(Surface *surface, STEPWrapper *wrapper,
    ON_Brep *result, std::string *failure)
{
    if (!surface || !wrapper || !result)
	return false;
    CurveBoundedSurface *curve_bounded =
	dynamic_cast<CurveBoundedSurface *>(surface);
    if (curve_bounded)
	return build_curve_bounded_surface(curve_bounded, wrapper, result, failure);

    /* GEOMETRIC_SET itself permits unbounded SURFACE members, but the bounded
     * surface representation profiles used for exchange require a finite
     * BOUNDED_SURFACE.  Do not invent clipping for analytic planes, cylinders,
     * or other infinite loci. */
    BoundedSurface *bounded = dynamic_cast<BoundedSurface *>(surface);
    if (!bounded) {
	if (failure) *failure = "an unbounded surface has no authored finite extent";
	return false;
    }

    wrapper->ResetOpenNURBSState();
    ON_Brep geometry;
    if (!bounded->LoadONBrep(&geometry)) {
	if (failure) *failure = "the bounded surface geometry could not be constructed";
	return false;
    }
    const int surface_index = bounded->GetONId();
    if (surface_index < 0 || surface_index >= geometry.m_S.Count() ||
	    !geometry.m_S[surface_index])
	return false;
    std::unique_ptr<ON_Brep> candidate(new ON_Brep());
    candidate->NewFace(*geometry.m_S[surface_index]);
    candidate->SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, true, true);
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!candidate->IsValid(&validation_log)) {
	if (failure) {
	    ON_String text(validation_messages);
	    *failure = text.Array() ? text.Array() :
		"OpenNURBS rejected the finite surface domain";
	}
	return false;
    }
    result->Append(*candidate);
    return true;
}


static BrepWriteStatus
convert_WriteSurfaceSet(GeometricSet *set, Representation *representation,
    STEPWrapper *wrapper, BRLCADWrapper *dotg, const std::string &name,
    int dry_run, const brlcad::step::Style *style_override = NULL)
{
    if (!set || !representation || !wrapper || !dotg || name.empty())
	return BREP_CONVERSION_FAILED;
    LocalUnits::length = representation->GetLengthConversionFactor();
    LocalUnits::planeangle = representation->GetPlaneAngleConversionFactor();
    LocalUnits::solidangle = representation->GetSolidAngleConversionFactor();

    std::unique_ptr<ON_Brep> surfaces(new ON_Brep());
    size_t attempted = 0;
    size_t converted = 0;
    LIST_OF_GEOMETRIC_SET_SELECT *elements = set->GetElements();
    if (!elements)
	return BREP_CONVERSION_FAILED;
    for (LIST_OF_GEOMETRIC_SET_SELECT::const_iterator element =
	    elements->begin(); element != elements->end(); ++element) {
	Surface *surface = *element ? (*element)->GetSurfaceElement() : NULL;
	if (!surface)
	    continue;
	++attempted;
	std::string failure;
	if (build_geometric_set_surface(surface, wrapper, surfaces.get(), &failure)) {
	    ++converted;
	    wrapper->RecordRepresentationItemCoverage(surface->GetId(),
		brlcad::step::RepresentationCoverageStatus::Handled,
		"bounded surface element converted successfully");
	} else {
	    const std::string reason = failure.empty() ?
		"bounded surface element could not be converted" : failure;
	    wrapper->RecordSkippedItem(surface->GetId(),
		"BOUNDED_SURFACE", reason);
	    wrapper->RecordRepresentationItemCoverage(surface->GetId(),
		brlcad::step::RepresentationCoverageStatus::Skipped, reason);
	}
    }
    if (!attempted || !converted)
	return BREP_CONVERSION_FAILED;

    surfaces->SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, true, true);
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!surfaces->Compact() || !surfaces->IsValid(&validation_log))
	return BREP_INVALID_STRUCTURE;

    mat_t matrix;
    representation_matrix(representation, matrix);
    const brlcad::step::Style *style = style_override ? style_override :
	style_for_item(wrapper, set->GetId());
    if (!style)
	style = style_for_item(wrapper, representation->GetId());
    const bool written = dry_run || dotg->WriteBrep(name, surfaces.get(),
	matrix, false, set->GetId(), set->Name(), style);
    if (written && style)
	++wrapper->Statistics().styles_applied;
    if (written && converted != attempted) {
	std::ostringstream message;
	message << "converted " << converted << " of " << attempted
	    << " bounded surface elements; omitted elements retain per-entity diagnostics";
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    set->GetId(), "GEOMETRIC_SET", "elements", message.str());
    }
    return written ? BREP_WRITE_SUCCESS : BREP_OUTPUT_FAILED;
}


BrepWriteStatus
step_convert_datum_plane(Plane *plane, Representation *representation,
    BRLCADWrapper *dotg, const std::string &name, int dry_run)
{
    if (!plane || !representation || !dotg || name.empty())
	return BREP_CONVERSION_FAILED;
    const double *source_origin = plane->GetOrigin();
    const double *source_normal = plane->GetNormal();
    if (!source_origin || !source_normal) return BREP_CONVERSION_FAILED;

    mat_t matrix;
    representation_matrix(representation, matrix);
    const double length_factor = representation->GetLengthConversionFactor();
    point_t local_origin = {
	source_origin[0] * length_factor,
	source_origin[1] * length_factor,
	source_origin[2] * length_factor
    };
    point_t origin;
    vect_t normal = {source_normal[0], source_normal[1], source_normal[2]};
    vect_t transformed_normal;
    MAT4X3PNT(origin, matrix, local_origin);
    MAT4X3VEC(transformed_normal, matrix, normal);
    if (MAGNITUDE(transformed_normal) <= SMALL_FASTF)
	return BREP_CONVERSION_FAILED;
    VUNITIZE(transformed_normal);
    const bool written = dry_run || dotg->WriteDatumPlane(name, origin,
	transformed_normal, 10.0, plane->GetId(), plane->Name());
    return written ? BREP_WRITE_SUCCESS : BREP_OUTPUT_FAILED;
}


BrepWriteStatus
step_convert_geometric_set(GeometricSet *set, Representation *representation,
    STEPWrapper *wrapper, BRLCADWrapper *dotg, std::string *name, int dry_run,
    const brlcad::step::Style *style_override)
{
    if (!set || !representation || !wrapper || !dotg || !name)
	return BREP_CONVERSION_FAILED;
    const bool curves = step_geometric_set_has_curves(set);
    const bool points = step_geometric_set_has_points(set);
    const bool surfaces = step_geometric_set_has_surfaces(set);
    if (!curves && !points && !surfaces)
	return BREP_CONVERSION_FAILED;
    if (curves && !points && !surfaces)
	return convert_WriteWireSet(set, representation, wrapper, dotg, name,
	    dry_run, style_override);
    if (points && !curves && !surfaces)
	return convert_WritePointSet(set, representation, wrapper, dotg, *name,
	    dry_run);
    if (surfaces && !curves && !points)
	return convert_WriteSurfaceSet(set, representation, wrapper, dotg, *name,
	    dry_run, style_override);

    struct SetComponent {
	std::string kind;
	std::string name;
	BrepWriteStatus status;
    };
    std::vector<SetComponent> components;
    if (curves) {
	std::string component_name = *name + "_curves";
	components.push_back({"curve", component_name,
	    convert_WriteWireSet(set, representation, wrapper, dotg,
		&component_name, dry_run, style_override)});
    }
    if (points) {
	const std::string component_name = *name + "_points";
	components.push_back({"point", component_name,
	    convert_WritePointSet(set, representation, wrapper, dotg,
		component_name, dry_run)});
    }
    if (surfaces) {
	const std::string component_name = *name + "_surfaces";
	components.push_back({"surface", component_name,
	    convert_WriteSurfaceSet(set, representation, wrapper, dotg,
		component_name, dry_run, style_override)});
    }

    size_t successful = 0;
    BrepWriteStatus first_failure = BREP_CONVERSION_FAILED;
    for (std::vector<SetComponent>::const_iterator component =
	    components.begin(); component != components.end(); ++component) {
	if (component->status == BREP_WRITE_SUCCESS)
	    ++successful;
	else {
	    first_failure = component->status;
	    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		set->GetId(), "GEOMETRIC_SET", "elements",
		"could not convert the " + component->kind +
		" element family; preserved the other usable families");
	}
    }
    if (!successful)
	return first_failure;
    if (!dry_run) {
	mat_t identity;
	MAT_IDN(identity);
	if (!dotg->EnsureCombination(*name))
	    return BREP_OUTPUT_FAILED;
	for (std::vector<SetComponent>::const_iterator component =
		components.begin(); component != components.end(); ++component) {
	    if (component->status == BREP_WRITE_SUCCESS &&
		    !dotg->AddMember(*name, component->name, identity))
		return BREP_OUTPUT_FAILED;
	}
    }
    return BREP_WRITE_SUCCESS;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
