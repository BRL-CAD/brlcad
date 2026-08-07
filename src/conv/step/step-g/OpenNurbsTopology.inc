/* BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file step/OpenNurbsTopology.inc
 *
 * STEP shell, face, loop, edge, and trim topology construction.  Included
 * only by OpenNurbsInterfaces.cpp after the pullback helper fragment.
 */

/* A STEP loop may contain a distinct-vertex edge whose complete bounded curve
 * locus is smaller than the declared model uncertainty.  Safe repair removes
 * that zero-length topological excursion and merges its two coincident
 * vertices.  Keep a non-index sentinel until loop construction filters the
 * removed edge; ordinary negative values continue to mean conversion failure.
 */
constexpr int kCollapsedStepEdgeIndex = -2;
constexpr int kCollapsedStepEdgeValidationSegments = 32;

ON_Brep *
AdvancedBrepShapeRepresentation::GetONBrep()
{
    ON_Brep *brep = ON_Brep::New();

    if (!brep) {
	std::cerr << "ERROR: INTERNAL MEMORY ALLOCATION FAILURE in " << __FILE__ << ":" << __LINE__ << std::endl;
	return NULL;
    }

    if (!LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << std::endl;
	//still return brep may contain something useful to diagnose
	return brep;
    }

    return brep;
}


bool
AdvancedBrepShapeRepresentation::LoadONBrep(ON_Brep *brep)
{
    LIST_OF_REPRESENTATION_ITEMS::iterator i;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    for (i = items.begin(); i != items.end(); i++) {
	if (!(*i)->LoadONBrep(brep)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }

    return true;
}

ON_Brep *
ShellBasedSurfaceModel::GetONBrep()
{
    ON_Brep *brep = ON_Brep::New();

    if (!brep) {
	std::cerr << "ERROR: INTERNAL MEMORY ALLOCATION FAILURE in " << __FILE__ << ":" << __LINE__ << std::endl;
	return NULL;
    }

    if (!LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << std::endl;
	//still return brep may contain something useful to diagnose
	return brep;
    }

    return brep;
}


bool
ShellBasedSurfaceModel::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    LIST_OF_SHELL_BOUNDARIES::iterator i;
    for (i = sbsm_boundary.begin(); i != sbsm_boundary.end(); ++i) {
	if (step && step->ImportOptions().skip_open_shells &&
		dynamic_cast<OpenShell *>(*i))
	    continue;
	if (!(*i)->LoadONBrep(brep)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }
    return true;
}

//
// Curve handlers
//
void
FaceSurface::AddFace(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    ON_BrepFace &face = brep->NewFace(face_geometry->GetONId());
    if (same_sense == BTrue) {
	face.m_bRev = false;
    } else {
	/* STEP same_sense is a face-orientation property.  Preserve it as the
	 * corresponding OpenNURBS face reversal; changing a surface parameter
	 * direction instead erases the distinction between two reciprocal caps
	 * and can make both select the same physical pole during trim repair. */
	face.m_bRev = true;
    }

    SetONId(face.m_face_index);
}


bool
FaceSurface::PrepareONBrep(ON_Brep *brep,
    std::vector<Face::PreparedBound> *prepared_bounds)
{
    if (!brep || !prepared_bounds || !face_geometry) {
	if (step && !face_geometry) {
	    /* No curve choice can replace an absent required surface.  Preserve
	     * the ordinary diagnostic and prevent a costly whole-item inference
	     * rebuild even if another edge in this solid happens to be an eligible
	     * curve-inference candidate. */
	    step->BlockCurveInferenceRetry();
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"FACE_SURFACE", "face_geometry",
		"cannot construct a BREP face without its required surface");
	}
	/* nothing to do */
	return false;
    }

    prepared_bounds->clear();

    if (step)
	step->SetProgressDetail("computing exact STEP face edge bounds", id);

    // need edge bounds to determine extents for some of the infinitely
    // defined surfaces like cones/cylinders/planes
    ON_BoundingBox *bb = GetEdgeBounds(brep);
    if (!bb) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error calculating openNURBS brep bounds." << std::endl;
	return false;
    }
	if (brlcad::PullbackWorkCancelled()) {
	    delete bb;
	    return false;
	}

    /* A conical face needs to know whether all of its exact boundary curves
     * lie on one side of the apex.  The world-axis-aligned bounding box is
     * sufficient to size an analytic surface, but its eight synthetic corners
     * can straddle an oblique cone axis even when a circular boundary is
     * planar.  Preserve a conservative projection of the positive-weight
     * NURBS control hull for the nappe decision. */
    ConicalSurface *cone = dynamic_cast<ConicalSurface *>(face_geometry);
    if (cone) {
	ON_3dPoint cone_origin(cone->GetOrigin());
	ON_3dVector cone_axis(cone->GetNormal());
	cone_origin *= LocalUnits::length;
	const double angle = cone->GetSemiAngle() * LocalUnits::planeangle;
	const double tangent = tan(angle);
	if (cone_axis.Unitize() && fabs(tangent) > ON_SQRT_EPSILON) {
	    const ON_3dPoint apex = cone_origin -
		(cone->GetRadius() * LocalUnits::length / tangent) * cone_axis;
	    double axial_minimum = 0.0;
	    double axial_maximum = 0.0;
	    if (GetEdgeAxisProjectionBounds(brep, apex, cone_axis,
		    &axial_minimum, &axial_maximum))
		cone->SetCurveAxisProjectionBounds(axial_minimum,
		    axial_maximum);
	}
    }
    face_geometry->SetCurveBounds(bb);
    delete bb;

    if (step)
	step->SetProgressDetail("building exact STEP face surface", id);
    /* A small subset of unbounded STEP loci is represented by a finite
     * OpenNURBS surface sized from this face's trim box.  Distinct faces may
     * use disjoint parts of that locus (the LS3 pump cone has several axial
     * bands), so adapters without a safe extension path request a face-local
     * instance.  Retain shared identity for exact bounded surfaces and
     * adapters such as Plane which explicitly extend their common domain:
     * periodic seam changes must then remain coherent across adjacent faces. */
    if (face_geometry->RequiresFaceLocalONSurface())
	face_geometry->ResetONState();
    if (!face_geometry->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    AddFace(brep);
	if (brlcad::PullbackWorkCancelled()) return false;

    //TODO: remove debugging code
    if ((false) && (GetONId() == 72)) {
	std::cerr << "Debug:LoadONBrep for FaceSurface:" << GetONId() << std::endl;
    }
    if (step)
	step->SetProgressDetail("constructing exact STEP face loops", id);
    if (!Face::PrepareONBrepLoops(brep, prepared_bounds)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname
		<< "::PrepareONBrep() - Error loading openNURBS face topology."
		<< std::endl;
	return false;
    }

    return true;
}


bool
FaceSurface::FinishONBrep(ON_Brep *brep,
    const std::vector<Face::PreparedBound> &prepared_bounds,
    double item_scale_override)
{
    if (!brep || !Face::FinishONBrepLoops(brep, prepared_bounds,
	    item_scale_override)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname
		<< "::FinishONBrep() - Error loading openNURBS face trims."
		<< std::endl;
	return false;
    }

    if (reverse) {
	ON_BrepFace *face = brep->Face(GetONId());
	if (!face)
	    return false;
	face->Reverse(1);
	face->m_bRev = face->m_bRev ? false : true;
    }
    return true;
}


bool
FaceSurface::LoadONBrep(ON_Brep *brep)
{
    std::vector<Face::PreparedBound> prepared_bounds;
    return PrepareONBrep(brep, &prepared_bounds) &&
	FinishONBrep(brep, prepared_bounds);
}


bool
OrientedFace::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // need edge bounds to determine extents for some of the infinitely
    // defined surfaces like cones/cylinders/planes
    if (!face_element->LoadONBrep(brep)) {
#ifndef AP242
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
#endif
	return false;
    }

    return true;
}


void
Point::AddVertex(ON_Brep *UNUSED(brep))
{
    std::cerr << "Warning: " << entityname << "::AddVertex() should be overridden by parent class." << std::endl;
}


void
CartesianPoint::AddVertex(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    if (GetONId() < 0) {
	ON_3dPoint p(coordinates[0] * LocalUnits::length, coordinates[1] * LocalUnits::length, coordinates[2] * LocalUnits::length);
	ON_BrepVertex &v = brep->NewVertex(p);
	v.m_tolerance = LocalUnits::tolerance;
	SetONId(v.m_vertex_index);
    }
}


void
BSplineCurve::AddPolyLine(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    if (GetONId() < 0) {
	int num_control_points = control_points_list.size();

	if ((degree == 1) && (num_control_points >= 2)) {
	    LIST_OF_POINTS::iterator i = control_points_list.begin();
	    CartesianPoint *cp = (*i);
	    while ((++i) != control_points_list.end()) {
		ON_3dPoint start_point(cp->X() * LocalUnits::length, cp->Y() * LocalUnits::length, cp->Z() * LocalUnits::length);
		cp = (*i);
		ON_3dPoint end_point(cp->X() * LocalUnits::length, cp->Y() * LocalUnits::length, cp->Z() * LocalUnits::length);
		ON_LineCurve *line = new ON_LineCurve(ON_3dPoint(start_point), ON_3dPoint(end_point));
		brep->m_C3.Append(line);
	    }
	} else if (num_control_points > 2) {
	    ON_NurbsCurve *c = ON_NurbsCurve::New(3, false, degree + 1, num_control_points);
	    /* FIXME: do something with c */
	    delete c;
	} else {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading polyLine." << std::endl;
	}
    }
}


void
VertexPoint::AddVertex(ON_Brep *brep)
{
    vertex_geometry->AddVertex(brep);
}


ON_BoundingBox *
Face::GetEdgeBounds(ON_Brep *brep)
{
    ON_BoundingBox *u = NULL;
    LIST_OF_FACE_BOUNDS::iterator i;
    for (i = bounds.begin(); i != bounds.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return NULL;
	ON_BoundingBox *bb = (*i)->GetEdgeBounds(brep);
	if (bb != NULL) {
	    if (u == NULL) {
		u = new ON_BoundingBox();
	    }
	    u->Union(*bb);
	}
	delete bb;
    }
    return u;
}


bool
Face::GrowTopologyVertexBounds(ON_BoundingBox *vertex_bounds)
{
    if (!vertex_bounds || bounds.empty()) return false;
    for (LIST_OF_FACE_BOUNDS::iterator bound = bounds.begin();
	    bound != bounds.end(); ++bound) {
	if (!*bound || !(*bound)->GrowTopologyVertexBounds(vertex_bounds))
	    return false;
    }
    return vertex_bounds->IsValid();
}


bool
Face::GetEdgeAxisProjectionBounds(ON_Brep *brep,
    const ON_3dPoint &origin, const ON_3dVector &axis,
    double *minimum, double *maximum)
{
    if (!brep || !minimum || !maximum || !axis.IsUnitVector() ||
	    bounds.empty())
	return false;
    bool have_bounds = false;
    double result_minimum = DBL_MAX;
    double result_maximum = -DBL_MAX;
    for (LIST_OF_FACE_BOUNDS::iterator bound = bounds.begin();
	    bound != bounds.end(); ++bound) {
	double bound_minimum = 0.0;
	double bound_maximum = 0.0;
	if (!*bound || !(*bound)->GetEdgeAxisProjectionBounds(brep, origin,
		axis, &bound_minimum, &bound_maximum))
	    return false;
	result_minimum = std::min(result_minimum, bound_minimum);
	result_maximum = std::max(result_maximum, bound_maximum);
	have_bounds = true;
    }
    if (!have_bounds)
	return false;
    *minimum = result_minimum;
    *maximum = result_maximum;
    return true;
}


bool
Face::LoadONBrep(ON_Brep *brep)
{
    std::vector<PreparedBound> prepared_bounds;
    return PrepareONBrepLoops(brep, &prepared_bounds) &&
	FinishONBrepLoops(brep, prepared_bounds);
}


/* STEP represents the collapsed end of some periodic faces with a separate
 * VERTEX_LOOP in addition to the ordinary outer EDGE_LOOP.  Those are two
 * topological bounds in STEP, but they cannot be two independent OpenNURBS
 * loops: the periodic parameter rectangle needs one outer loop containing
 * the ordinary trims, two opposite uses of a seam edge, and a singular pole
 * trim.  Path::LoadONTrimmingCurves constructs that exact pole cut after it
 * has validated the full-period boundary pullback.
 *
 * This topology is not specific to an analytic cone: STEP writers commonly
 * encode the same locus as a rational B-spline surface.  Absorb only the
 * representation-independent, geometrically proven form: one vertex loop,
 * one path which is either explicitly outer or not contradicted by an outer
 * vertex designation, exactly one closed surface parameter, exactly one
 * singular surface side matching the STEP vertex, and an exact match between
 * that vertex and both ends of the collapsed side.  A sphere may have two
 * singular sides, but only its matching pole is absorbed.  Other multi-bound
 * vertex-loop forms retain their ordinary handling. */
static FaceBound *
step_absorbable_periodic_vertex_bound(const LIST_OF_FACE_BOUNDS &face_bounds,
	ON_Brep *brep, int face_index)
{
    if (!brep || face_index < 0 || face_index >= brep->m_F.Count() ||
	    face_bounds.size() != 2)
	return NULL;

    FaceBound *vertex_bound = NULL;
    FaceBound *path_bound = NULL;
    for (LIST_OF_FACE_BOUNDS::const_iterator bound = face_bounds.begin();
	    bound != face_bounds.end(); ++bound) {
	if (!*bound)
	    return NULL;
	if (dynamic_cast<VertexLoop *>((*bound)->GetBound())) {
	    if (vertex_bound)
		return NULL;
	    vertex_bound = *bound;
	} else if (dynamic_cast<Path *>((*bound)->GetBound())) {
	    if (path_bound)
		return NULL;
	    path_bound = *bound;
	} else {
	    return NULL;
	}
    }
    if (!vertex_bound || !path_bound ||
	    (!path_bound->IsOuter() && vertex_bound->IsOuter()))
	return NULL;

    const ON_Surface *surface = brep->m_F[face_index].SurfaceOf();
    if (!surface)
	return NULL;
    int closed_direction = -1;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	if (closed_direction >= 0)
	    return NULL;
	closed_direction = direction;
    }
    if (closed_direction < 0)
	return NULL;

    VertexLoop *vertex_loop = dynamic_cast<VertexLoop *>(
	vertex_bound->GetBound());
    const double *coordinates = vertex_loop && vertex_loop->LoopVertex() ?
	vertex_loop->LoopVertex()->Point3d() : NULL;
    if (!coordinates)
	return NULL;
    const ON_3dPoint vertex(coordinates[0] * LocalUnits::length,
	coordinates[1] * LocalUnits::length,
	coordinates[2] * LocalUnits::length);
    const ON_Interval u = surface->Domain(0);
    const ON_Interval v = surface->Domain(1);
    const double tolerance = std::max(LocalUnits::tolerance,
	ON_ZERO_TOLERANCE);
    if (!vertex.IsValid())
	return NULL;
    int matching_singular_side = -1;
    for (int side = 0; side < 4; ++side) {
	if (!surface->IsSingular(side))
	    continue;
	ON_3dPoint pole_start;
	ON_3dPoint pole_end;
	switch (side) {
	    case 0:
		pole_start = surface->PointAt(u.Min(), v.Min());
		pole_end = surface->PointAt(u.Max(), v.Min());
		break;
	    case 1:
		pole_start = surface->PointAt(u.Max(), v.Min());
		pole_end = surface->PointAt(u.Max(), v.Max());
		break;
	    case 2:
		pole_start = surface->PointAt(u.Max(), v.Max());
		pole_end = surface->PointAt(u.Min(), v.Max());
		break;
	    case 3:
		pole_start = surface->PointAt(u.Min(), v.Max());
		pole_end = surface->PointAt(u.Min(), v.Min());
		break;
	    default:
		continue;
	}
	if (!pole_start.IsValid() || !pole_end.IsValid() ||
		pole_start.DistanceTo(pole_end) > tolerance ||
		vertex.DistanceTo(pole_start) > tolerance)
	    continue;
	if (matching_singular_side >= 0)
	    return NULL;
	matching_singular_side = side;
    }
    if (matching_singular_side < 0)
	return NULL;
    return vertex_bound;
}


bool
Face::PrepareONBrepLoops(ON_Brep *brep,
    std::vector<PreparedBound> *prepared_bounds)
{
    if (!brep || !prepared_bounds)
	return false;
    prepared_bounds->clear();
    prepared_bounds->reserve(bounds.size());
    //TODO: Check for Outer bound if none check for
    // direction perhaps offer input option possibly
    // check for outer spanning to bounds
    LIST_OF_FACE_BOUNDS::iterator i;
    /* Register every STEP face bound before constructing any of its trimming
     * curves.  Periodic-loop repair needs to distinguish a genuinely
     * single-bound spherical cap from the first bound of a multi-loop face;
     * processing and registering one loop at a time made that distinction
     * depend incorrectly on source aggregate order. */
    FaceBound *absorbed_vertex_bound =
	step_absorbable_periodic_vertex_bound(bounds, brep, GetONId());
    for (i = bounds.begin(); i != bounds.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return false;
	if (*i == absorbed_vertex_bound)
	    continue;
	(*i)->SetFaceIndex(GetONId());
	if (!(*i)->CreateONLoop(brep, bounds.size() == 1)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname
		    << "::LoadONBrep() - Error registering openNURBS face loop."
		    << std::endl;
	    return false;
	}
    }
    for (i = bounds.begin(); i != bounds.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return false;
	if (*i == absorbed_vertex_bound)
	    continue;
	(*i)->SetFaceIndex(GetONId());
	PreparedBound prepared;
	prepared.bound = *i;
	prepared.loop_index = (*i)->GetONId();
	if (!(*i)->PrepareONBrep(brep, &prepared.edge_indices)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname
		    << "::PrepareONBrepLoops() - Error loading openNURBS edge topology."
		    << std::endl;
	    return false;
	}
	prepared_bounds->push_back(prepared);
    }
    if (absorbed_vertex_bound && step) {
	Loop *absorbed_loop = absorbed_vertex_bound->GetBound();
	step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
	    absorbed_loop ? absorbed_loop->STEPid() : absorbed_vertex_bound->STEPid(),
	    "VERTEX_LOOP", "loop_vertex",
	    "represented the exact periodic pole vertex loop as the singular "
	    "side of the adjacent periodic outer loop");
    }
    return true;
}


bool
Face::FinishONBrepLoops(ON_Brep *brep,
    const std::vector<PreparedBound> &prepared_bounds,
    double item_scale_override)
{
    if (!brep || prepared_bounds.empty())
	return false;
    /* Dense open spline surfaces are immutable during loop normalization.
     * Share their expensive span-box index across all bounds on this face;
     * each edge still forks private solver state and telemetry.  Closed
     * surfaces keep loop-local caches because periodic seam repair may alter
     * the surface between bounds. */
    std::shared_ptr<brlcad::PullbackContext> face_surface_cache;
    if (!prepared_bounds.empty()) {
	const int loop_index = prepared_bounds.front().loop_index;
	const ON_BrepLoop *loop = loop_index >= 0 &&
	    loop_index < brep->m_L.Count() ? &brep->m_L[loop_index] : NULL;
	const ON_BrepFace *face = loop ? loop->Face() : NULL;
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (surface && !surface->IsClosed(0) && !surface->IsClosed(1))
	    face_surface_cache.reset(new brlcad::PullbackContext());
    }
    for (std::vector<PreparedBound>::const_iterator prepared =
	    prepared_bounds.begin(); prepared != prepared_bounds.end(); ++prepared) {
	if (brlcad::PullbackWorkCancelled()) return false;
	if (step)
	    step->SetProgressDetail("constructing exact STEP face loop",
		prepared->bound ? prepared->bound->STEPid() : id, 0, 0,
		std::string(), "face=#" + std::to_string(id));
	if (!prepared->bound || !prepared->bound->FinishONBrep(brep,
		prepared->loop_index, prepared->edge_indices,
		item_scale_override, face_surface_cache)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname
		    << "::FinishONBrepLoops() - Error loading openNURBS trims."
		    << std::endl;
	    return false;
	}
    }
    return true;
}


bool
FaceOuterBound::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    SetOuter();

    if (!FaceBound::LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


ON_BoundingBox *
FaceBound::GetEdgeBounds(ON_Brep *brep)
{
    VertexLoop *vertex_loop = dynamic_cast<VertexLoop *>(bound);
    if (!vertex_loop)
	return bound ? bound->GetEdgeBounds(brep) : NULL;
    const double *coordinates = vertex_loop->LoopVertex() ?
	vertex_loop->LoopVertex()->Point3d() : NULL;
    if (!coordinates)
	return NULL;
    const ON_3dPoint point(coordinates[0] * LocalUnits::length,
	coordinates[1] * LocalUnits::length,
	coordinates[2] * LocalUnits::length);
    return point.IsValid() ? new ON_BoundingBox(point, point) : NULL;
}


bool
FaceBound::GetEdgeAxisProjectionBounds(ON_Brep *brep,
    const ON_3dPoint &origin, const ON_3dVector &axis,
    double *minimum, double *maximum)
{
    Path *path = dynamic_cast<Path *>(bound);
    if (path)
	return path->GetEdgeAxisProjectionBounds(brep, origin, axis,
	    minimum, maximum);
    VertexLoop *vertex_loop = dynamic_cast<VertexLoop *>(bound);
    const double *coordinates = vertex_loop && vertex_loop->LoopVertex() ?
	vertex_loop->LoopVertex()->Point3d() : NULL;
    if (!minimum || !maximum || !axis.IsUnitVector() || !coordinates)
	return false;
    const ON_3dPoint point(coordinates[0] * LocalUnits::length,
	coordinates[1] * LocalUnits::length,
	coordinates[2] * LocalUnits::length);
    if (!point.IsValid())
	return false;
    *minimum = *maximum = (point - origin) * axis;
    return true;
}


bool
FaceBound::CreateONLoop(ON_Brep *brep, bool sole_face_bound)
{
    if (!brep || ON_face_index < 0 || ON_face_index >= brep->m_F.Count())
	return false;

    if (GetONId() < 0) {
	/* A sole VERTEX_LOOP is the STEP topological representation of some
	 * complete closed faces, including spheres and doubly periodic tori.  It
	 * has no geometric boundary to turn into a singular trim.  OpenNURBS
	 * represents the same untrimmed locus with explicit artificial seams,
	 * which NewOuterLoop constructs without changing the surface.  Ordinary
	 * vertex loops on a surface pole continue through the singular-trim path
	 * below. */
	VertexLoop *vertex_loop = dynamic_cast<VertexLoop *>(bound);
	const ON_Surface *surface = brep->m_F[ON_face_index].SurfaceOf();
	if (sole_face_bound && vertex_loop && surface && surface->IsSolid()) {
	    ON_BrepLoop *loop = brep->NewOuterLoop(ON_face_index);
	    if (!loop)
		return false;
	    SetONId(loop->m_loop_index);
	    return true;
	}
	enum ON_BrepLoop::TYPE btype;
	if (IsInner()) {
	    btype = ON_BrepLoop::inner;
	} else {
	    btype = ON_BrepLoop::outer;
	}
	ON_BrepLoop &loop = brep->NewLoop(btype, brep->m_F[ON_face_index]);
	SetONId(loop.m_loop_index);
    }
    return true;
}


bool
FaceBound::LoadONBrep(ON_Brep *brep)
{
    if (!CreateONLoop(brep))
	return false;

    bound->SetLoopIndex(GetONId());
    if (!bound->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!Oriented()) {
	ON_BrepLoop &loop = brep->m_L[GetONId()];
	brep->FlipLoop(loop);
    }
    /* lets use the cues from STEP (Oriented) for now but leave this here
     * in case we have to go back to brute force

     if (IsInner()) {
     ON_BrepLoop& loop = brep->m_L[ON_id];
     if (brep->LoopDirection((const ON_BrepLoop&) loop) > 0) {
     brep->FlipLoop(loop);
     }
     } else {
     ON_BrepLoop& loop = brep->m_L[ON_id];
     if (brep->LoopDirection((const ON_BrepLoop&) loop) < 0) {
     brep->FlipLoop(loop);
     }
     }
    */

    return true;
}


bool
FaceBound::PrepareONBrep(ON_Brep *brep, std::vector<int> *edge_indices)
{
    if (!edge_indices || !CreateONLoop(brep))
	return false;
    Path *path = dynamic_cast<Path *>(bound);
    if (path) {
	path->SetPathIndex(GetONId());
	return path->PrepareONBrepEdges(brep, edge_indices);
    }

    /* VERTEX_LOOP is a legal face bound for a surface pole and for closed
     * surfaces without ordinary boundary edges.  It has no immutable 3-D
     * edge topology to prepare; its singular trim is constructed in the
     * finish phase after every face loop has been registered.  The split
     * prepare/finish path originally accepted only Path, unintentionally
     * dropping support which FaceBound::LoadONBrep previously provided. */
    VertexLoop *vertex_loop = dynamic_cast<VertexLoop *>(bound);
    if (!vertex_loop)
	return false;
    vertex_loop->SetLoopIndex(GetONId());
    edge_indices->clear();
    return true;
}


bool
FaceBound::FinishONBrep(ON_Brep *brep, int loop_index,
    const std::vector<int> &edge_indices, double item_scale_override,
    std::shared_ptr<brlcad::PullbackContext> surface_cache)
{
    Path *path = dynamic_cast<Path *>(bound);
    if (!brep || loop_index < 0 || loop_index >= brep->m_L.Count())
	return false;

    bool loaded = false;
    if (path) {
	loaded = path->LoadONTrimmingCurves(brep, loop_index, edge_indices,
	    item_scale_override, surface_cache);
    } else {
	VertexLoop *vertex_loop = dynamic_cast<VertexLoop *>(bound);
	if (!vertex_loop || !edge_indices.empty())
	    return false;
	ON_BrepLoop &loop = brep->m_L[loop_index];
	const ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (loop.m_ti.Count() > 0 && surface && surface->IsSolid()) {
	    loaded = true;
	} else {
	    vertex_loop->SetLoopIndex(loop_index);
	    loaded = vertex_loop->LoadONBrep(brep);
	}
    }
    if (!loaded)
	return false;
    if (!Oriented())
	brep->FlipLoop(brep->m_L[loop_index]);
    return true;
}


bool
FaceBound::GrowTopologyVertexBounds(ON_BoundingBox *vertex_bounds)
{
    Path *path = dynamic_cast<Path *>(bound);
    if (path)
	return path->GrowTopologyVertexBounds(vertex_bounds);
    VertexLoop *vertex_loop = dynamic_cast<VertexLoop *>(bound);
    const double *coordinates = vertex_loop && vertex_loop->LoopVertex() ?
	vertex_loop->LoopVertex()->Point3d() : NULL;
    if (!vertex_bounds || !coordinates)
	return false;
    const ON_3dPoint point(coordinates[0] * LocalUnits::length,
	coordinates[1] * LocalUnits::length,
	coordinates[2] * LocalUnits::length);
    if (!point.IsValid())
	return false;
    vertex_bounds->Set(point, true);
    return true;
}


/* Build the smallest topology-anchored completion of an authored open curve
 * fragment.  Exactly one fragment endpoint must already equal its asserted
 * STEP vertex.  The retained fragment is oriented to the topology, densely
 * checked against the topology chord and path-length bounds, and joined to
 * the other asserted vertex by one straight segment.  This proof is shared by
 * ordinary increasing/reversed source intervals and by terminal fragments for
 * which both vertex projections select the same source parameter. */
static bool
bounded_topology_vertex_bridge(const ON_Curve *source_fragment,
    const ON_3dPoint &start, const ON_3dPoint &end, bool construct_curve,
    std::unique_ptr<ON_Curve> *completed_curve, double *bridge_limit)
{
    if (completed_curve) completed_curve->reset();
    if (bridge_limit) *bridge_limit = 0.0;
    if (!source_fragment || !source_fragment->IsValid() ||
	    !start.IsValid() || !end.IsValid() ||
	    ON_PolyCurve::Cast(source_fragment) != NULL)
	return false;

    std::unique_ptr<ON_Curve> fragment(source_fragment->DuplicateCurve());
    if (!fragment || !fragment->IsValid())
	return false;

    const ON_3dPoint fragment_start = fragment->PointAtStart();
    const ON_3dPoint fragment_end = fragment->PointAtEnd();
    if (!fragment_start.IsValid() || !fragment_end.IsValid())
	return false;

    /* Select the orientation whose one exact endpoint has the least gap.
     * The other endpoint must remain non-exact: a fragment already bounded by
     * both topology vertices needs no inferred connector. */
    const double forward_start_gap = fragment_start.DistanceTo(start);
    const double forward_end_gap = fragment_end.DistanceTo(end);
    const double reverse_start_gap = fragment_end.DistanceTo(start);
    const double reverse_end_gap = fragment_start.DistanceTo(end);
    const double forward_exact_gap = std::min(forward_start_gap,
	forward_end_gap);
    const double reverse_exact_gap = std::min(reverse_start_gap,
	reverse_end_gap);
    if (reverse_exact_gap < forward_exact_gap && !fragment->Reverse())
	return false;

    const bool start_endpoint_exact =
	fragment->PointAtStart().DistanceTo(start) <= LocalUnits::tolerance;
    const bool end_endpoint_exact =
	fragment->PointAtEnd().DistanceTo(end) <= LocalUnits::tolerance;
    if (start_endpoint_exact == end_endpoint_exact)
	return false;

    const double topology_span = start.DistanceTo(end);
    const ON_3dVector topology_chord = end - start;
    const double topology_span_squared = topology_chord * topology_chord;
    if (!std::isfinite(topology_span) ||
	    topology_span <= LocalUnits::tolerance ||
	    !(topology_span_squared > 0.0))
	return false;

    const ON_3dPoint missing_topology_endpoint =
	start_endpoint_exact ? end : start;
    const ON_3dPoint fragment_endpoint =
	start_endpoint_exact ? fragment->PointAtEnd() :
	fragment->PointAtStart();
    const double connector_length =
	missing_topology_endpoint.DistanceTo(fragment_endpoint);
    const double tube_limit = std::max(LocalUnits::tolerance,
	topology_span * kMaximumPermissiveTopologyBridgeTubeFraction);
    double fragment_traversal = 0.0;
    bool fragment_valid = fragment_endpoint.IsValid() &&
	std::isfinite(connector_length) &&
	connector_length <= topology_span *
	    kMaximumPermissiveTopologyBridgeFraction;
    bool fragment_in_chord_envelope = fragment_valid;
    ON_3dPoint previous = fragment->PointAtStart();
    for (int sample = 0; fragment_valid &&
	    sample <= kDenseLiftValidationSegments; ++sample) {
	const double fraction = static_cast<double>(sample) /
	    kDenseLiftValidationSegments;
	const ON_3dPoint point = fragment->PointAt(
	    fragment->Domain().ParameterAt(fraction));
	if (!point.IsValid() || !previous.IsValid()) {
	    fragment_valid = false;
	    break;
	}
	if (sample > 0)
	    fragment_traversal += previous.DistanceTo(point);
	const double chord_fraction = ((point - start) * topology_chord) /
	    topology_span_squared;
	if (!std::isfinite(chord_fraction) || chord_fraction < 0.0 ||
		chord_fraction > 1.0) {
	    fragment_in_chord_envelope = false;
	} else {
	    const ON_3dPoint chord_point = start +
		chord_fraction * topology_chord;
	    const double chord_distance = point.DistanceTo(chord_point);
	    fragment_in_chord_envelope = fragment_in_chord_envelope &&
		std::isfinite(chord_distance) && chord_distance <= tube_limit;
	}
	fragment_valid = fragment_valid &&
	    std::isfinite(fragment_traversal);
	previous = point;
    }
    const bool short_chord_completion = fragment_valid &&
	fragment_in_chord_envelope &&
	connector_length + fragment_traversal <= topology_span *
	    kMaximumPermissiveTopologyBridgePathFraction;
    const bool bounded_curved_completion = fragment_valid &&
	fragment_traversal <= topology_span *
	    kMaximumPermissiveTopologyFragmentTraversalFraction &&
	connector_length + fragment_traversal <= topology_span *
	    kMaximumPermissiveTopologyCurvedPathFraction;
    if (!short_chord_completion && !bounded_curved_completion)
	return false;

    if (bridge_limit)
	*bridge_limit = topology_span *
	    kMaximumPermissiveTopologyBridgeFraction;
    if (!construct_curve)
	return true;

    std::unique_ptr<ON_PolyCurve> joined(new ON_PolyCurve());
    joined->Reserve(2);
    if (!start_endpoint_exact) {
	ON_LineCurve *connector = new ON_LineCurve(start,
	    fragment->PointAtStart());
	if (!connector->IsValid() || !joined->Append(connector)) {
	    delete connector;
	    return false;
	}
    }
    ON_Curve *owned_fragment = fragment.release();
    if (!joined->Append(owned_fragment)) {
	delete owned_fragment;
	return false;
    }
    if (!end_endpoint_exact) {
	ON_LineCurve *connector = new ON_LineCurve(joined->PointAtEnd(), end);
	if (!connector->IsValid() || !joined->Append(connector)) {
	    delete connector;
	    return false;
	}
    }
    if (!joined->IsValid() ||
	    joined->PointAtStart().DistanceTo(start) > LocalUnits::tolerance ||
	    joined->PointAtEnd().DistanceTo(end) > LocalUnits::tolerance)
	return false;
    if (completed_curve)
	*completed_curve = std::move(joined);
    return completed_curve && completed_curve->get();
}


/* STEP EDGE_CURVE vertices bound the edge's topological interval; the
 * referenced curve is not required to begin and end at those vertices.  This
 * distinction matters for spline exporters that reuse one long intersection
 * curve for several consecutive edges.  OpenNURBS NewEdge() accepts the
 * corresponding curve subdomain, but the historical importer always passed
 * the complete curve and consequently attempted to pull irrelevant portions
 * back to each adjacent face.
 *
 * Return a source-curve interval only when the complete curve is not already
 * bounded by the requested vertices.  Closed curves may require an owned,
 * seam-relocated duplicate when the exact interval wraps across the source
 * parameter domain.  Safe mode first applies a
 * bounded-edge local scale ceiling because the whole-item scale is unavailable
 * during topology preparation; trim construction then applies the ordinary
 * edge/surface and item checks before output.  --exact continues to require
 * the declared model tolerance here. */
static bool
step_edge_curve_subdomain(const ON_Curve *curve,
	const ON_3dPoint &start, const ON_3dPoint &end, STEPWrapper *step,
	int64_t step_entity_id, bool same_topology_vertex,
	ON_Interval *subdomain, bool *use_subdomain, double *edge_tolerance,
	std::unique_ptr<ON_Curve> *wrapped_curve, bool *collapse_edge,
	std::string *failure)
{
    if (use_subdomain) *use_subdomain = false;
    if (edge_tolerance) *edge_tolerance = LocalUnits::tolerance;
    if (wrapped_curve) wrapped_curve->reset();
    if (collapse_edge) *collapse_edge = false;
    if (failure) failure->clear();
    if (!curve || !curve->IsValid() || !start.IsValid() || !end.IsValid() ||
	    !subdomain || !use_subdomain || !edge_tolerance ||
	    !wrapped_curve || !collapse_edge ||
	    !(LocalUnits::tolerance > 0.0)) {
	if (failure) *failure = "invalid source curve or STEP edge vertices";
	return false;
    }

    const ON_Interval domain = curve->Domain();
    if (!domain.IsIncreasing()) {
	if (failure) *failure = "source curve has a non-increasing domain";
	return false;
    }
    const double direct_start = curve->PointAtStart().DistanceTo(start);
    const double direct_end = curve->PointAtEnd().DistanceTo(end);
    if (direct_start <= LocalUnits::tolerance &&
	    direct_end <= LocalUnits::tolerance &&
	    (!curve->IsClosed() || same_topology_vertex))
	return true;

    /* A one-vertex edge intentionally represents a complete closed curve (or
     * a source-asserted almost-closed curve handled by the bounded endpoint
     * repair below).  For a genuinely closed curve, the STEP vertex selects
     * the topological start of that full traversal; it is not required to
     * coincide with the curve's stored knot seam.  OpenNURBS does require its
     * closed edge curve to begin and end at the edge vertex, so relocate an
     * owned duplicate's seam to the vertex without trimming or changing the
     * curve locus.  This is an exact representation normalization and is
     * therefore also permitted by --exact.
     *
     * Do not run the ordinary two-end interval selection for a one-vertex
     * edge: both closest-point solves would select the same parameter and
     * incorrectly collapse the complete curve to zero length. */
    if (same_topology_vertex && curve->IsClosed()) {
	const CurveDistanceEvaluator evaluator(curve);
	double vertex_parameter = ON_UNSET_VALUE;
	double vertex_distance = DBL_MAX;
	if (!evaluator.IsValid() ||
		!evaluator.ClosestParameter(start, &vertex_parameter,
		    &vertex_distance) ||
		!domain.Includes(vertex_parameter)) {
	    if (failure)
		*failure = "could not locate the closed STEP edge vertex on "
		    "the source curve";
	    return false;
	}
	const bool allow_adjustment = step && !step->ImportOptions().exact &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
	const double accepted_limit = allow_adjustment ?
	    maximum_verified_edge_vertex_tolerance(curve,
		LocalUnits::tolerance) : LocalUnits::tolerance;
	if (!std::isfinite(vertex_distance) ||
		vertex_distance > accepted_limit) {
	    if (failure) {
		std::ostringstream reason;
		reason << "closed STEP edge vertex misses the source curve by "
		    << vertex_distance << " mm (accepted " << accepted_limit
		    << ", declared " << LocalUnits::tolerance << ')';
		*failure = reason.str();
	    }
	    return false;
	}

	std::unique_ptr<ON_Curve> relocated(curve->DuplicateCurve());
	/* OpenNURBS deliberately rounds a split parameter which is within
	 * ON_SQRT_EPSILON of a knot onto that knot.  ChangeClosedCurveSeam()
	 * consequently returns false when a closest-point solve reports a value
	 * just inside the native end span: its internal split is rounded onto the
	 * existing seam.  Use the same numerical scale here and then retain the
	 * geometric endpoint-distance checks below.  The previous 1e-12
	 * parameter fraction was tighter than OpenNURBS' own split contract and
	 * rejected an unchanged exact seam as if relocation had failed. */
	const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	    domain.Length() * ON_SQRT_EPSILON);
	const bool already_at_native_seam =
	    fabs(vertex_parameter - domain.Min()) <= parameter_guard ||
	    fabs(vertex_parameter - domain.Max()) <= parameter_guard;
	const bool seam_changed = relocated && (already_at_native_seam ||
	    relocated->ChangeClosedCurveSeam(vertex_parameter));
	const bool relocated_valid = seam_changed && relocated->IsValid();
	const bool relocated_closed = relocated_valid && relocated->IsClosed();
	const double relocated_start_distance = relocated_closed ?
	    relocated->PointAtStart().DistanceTo(start) : DBL_MAX;
	const double relocated_end_distance = relocated_closed ?
	    relocated->PointAtEnd().DistanceTo(start) : DBL_MAX;
	const double maximum_vertex_distance = std::max(vertex_distance,
	    std::max(relocated_start_distance, relocated_end_distance));
	if (!relocated || !seam_changed || !relocated_valid ||
		!relocated_closed ||
		relocated_start_distance > accepted_limit ||
		relocated_end_distance > accepted_limit) {
	    if (failure) {
		std::ostringstream reason;
		reason << "could not relocate the closed source-curve seam to "
		    << vertex_parameter << " in domain " << domain.Min() << ':'
		    << domain.Max() << " (native="
		    << (already_at_native_seam ? 1 : 0) << ", duplicate="
		    << (relocated ? 1 : 0) << ", seam="
		    << (seam_changed ? 1 : 0) << ", valid="
		    << (relocated_valid ? 1 : 0) << ", closed="
		    << (relocated_closed ? 1 : 0) << ", endpoint distances "
		    << relocated_start_distance << '/' << relocated_end_distance
		    << ", accepted " << accepted_limit << ')';
		*failure = reason.str();
	    }
	    return false;
	}

	*wrapped_curve = std::move(relocated);
	if (maximum_vertex_distance > LocalUnits::tolerance) {
	    *edge_tolerance = maximum_vertex_distance *
		kMeasuredToleranceSafetyFactor;
	    if (step) {
		step->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning,
		    step_entity_id, "EDGE_CURVE", "edge_geometry",
		    "closed source curve missed its topology vertex; used a "
		    "measured local edge tolerance while preserving the exact "
		    "curve locus");
		step->RecordRepair(step_entity_id, "EDGE_CURVE",
		    "edge_geometry",
		    already_at_native_seam ?
		    "retained the native closed source-curve seam within its "
		    "measured STEP vertex tolerance" :
		    "relocated a closed source-curve seam within its measured "
		    "STEP vertex tolerance");
	    }
	} else if (step && !already_at_native_seam) {
	    step->RecordDiagnostic(
		brlcad::step::DiagnosticSeverity::Information,
		step_entity_id, "EDGE_CURVE", "edge_geometry",
		"relocated the exact closed source-curve seam to its asserted "
		"STEP topology vertex");
	}
	return true;
    }
    if (same_topology_vertex)
	return true;

    const CurveDistanceEvaluator evaluator(curve);
    double start_parameter = ON_UNSET_VALUE;
    double end_parameter = ON_UNSET_VALUE;
    double start_distance = DBL_MAX;
    double end_distance = DBL_MAX;
    if (!evaluator.IsValid() ||
	    !evaluator.ClosestParameter(start, &start_parameter,
		&start_distance) ||
	    !evaluator.ClosestParameter(end, &end_parameter, &end_distance) ||
	    !domain.Includes(start_parameter) || !domain.Includes(end_parameter)) {
	if (failure) *failure =
	    "could not locate both STEP topology vertices on the source curve";
	return false;
    }

    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	domain.Length() * 1.0e-12);
    /* The two ends of a closed curve domain are the same geometric point.
     * A closest-point solve is therefore free to report domain.Max() for an
     * EDGE_CURVE start that STEP (and the increasing traversal) expresses as
     * domain.Min(), or domain.Min() for an end expressed as domain.Max().
     * Normalize only these endpoint-equivalent cases before deciding whether
     * the interval wraps.  Asking openNURBS to move a seam from domain.Max()
     * to its already equivalent location can fail, and adding a full period
     * would select the complementary curve segment. */
    bool normalized_closed_endpoint = false;
    if (curve->IsClosed()) {
	const bool start_at_maximum =
	    fabs(start_parameter - domain.Max()) <= parameter_guard;
	const bool start_at_minimum =
	    fabs(start_parameter - domain.Min()) <= parameter_guard;
	const bool end_at_maximum =
	    fabs(end_parameter - domain.Max()) <= parameter_guard;
	const bool end_at_minimum =
	    fabs(end_parameter - domain.Min()) <= parameter_guard;
	if (start_at_maximum && !end_at_minimum) {
	    start_parameter = domain.Min();
	    normalized_closed_endpoint = true;
	}
	if (end_at_minimum && !start_at_maximum) {
	    end_parameter = domain.Max();
	    normalized_closed_endpoint = true;
	}
	/* Preserve a complete closed traversal when the topology supplies the
	 * native minimum and maximum explicitly. */
	if (start_at_minimum && end_at_maximum) {
	    start_parameter = domain.Min();
	    end_parameter = domain.Max();
	}
    }
    const bool direct_interval = end_parameter - start_parameter >
	parameter_guard;
    /* An open STEP curve can be shared by multiple EDGE_CURVEs, with
     * SAME_SENSE selecting a traversal opposite to the curve's stored
     * parameterization.  Its vertex-bounded interval is still exact: trim the
     * increasing source interval and reverse an owned duplicate rather than
     * rejecting the decreasing parameter pair. */
    const bool reversed_open_interval = !curve->IsClosed() &&
	start_parameter - end_parameter > parameter_guard;
    const bool wrapped_interval = !direct_interval && curve->IsClosed() &&
	end_parameter + domain.Length() - start_parameter > parameter_guard;
    if (!direct_interval && !reversed_open_interval && !wrapped_interval) {
	/* Some exporters truncate an open NURBS domain a few decimal places
	 * before an asserted terminal vertex.  When both topology vertices select
	 * the same domain end, recover only an exact continuation of the terminal
	 * polynomial span.  This is not a tangent-line approximation: openNURBS
	 * extends the existing de Boor span, and the resulting tiny subcurve must
	 * pass the asserted vertex at the original declared tolerance. */
	const bool allow_exact_endpoint_extension = step &&
	    !step->ImportOptions().exact &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
	    !curve->IsClosed() && ON_NurbsCurve::Cast(curve) != NULL;
	const bool both_at_minimum =
	    fabs(start_parameter - domain.Min()) <= parameter_guard &&
	    fabs(end_parameter - domain.Min()) <= parameter_guard;
	const bool both_at_maximum =
	    fabs(start_parameter - domain.Max()) <= parameter_guard &&
	    fabs(end_parameter - domain.Max()) <= parameter_guard;
	if (allow_exact_endpoint_extension &&
		(both_at_minimum || both_at_maximum)) {
	    const double terminal_parameter = both_at_maximum ?
		domain.Max() : domain.Min();
	    const ON_3dPoint terminal_point = curve->PointAt(terminal_parameter);
	    const double start_terminal_gap = terminal_point.IsValid() ?
		terminal_point.DistanceTo(start) : DBL_MAX;
	    const double end_terminal_gap = terminal_point.IsValid() ?
		terminal_point.DistanceTo(end) : DBL_MAX;
	    const bool start_at_terminal =
		start_terminal_gap <= LocalUnits::tolerance;
	    const bool end_at_terminal =
		end_terminal_gap <= LocalUnits::tolerance;
	    const ON_3dPoint outside_vertex = start_at_terminal ? end : start;
	    const double outside_gap = start_at_terminal ?
		end_terminal_gap : start_terminal_gap;
	    const ON_BoundingBox curve_bounds = curve->BoundingBox();
	    const double curve_scale = curve_bounds.IsValid() ?
		curve_bounds.Diagonal().Length() : 0.0;
	    const double extension_limit = std::min(
		curve_scale * kMaximumRelativeNurbsEndpointExtension,
		LocalUnits::tolerance *
		    kMaximumDeclaredNurbsEndpointExtensionFactor);
	    ON_3dPoint derivative_point;
	    ON_3dVector derivative;
	    const bool derivative_valid = curve->Ev1Der(terminal_parameter,
		derivative_point, derivative) && derivative_point.IsValid() &&
		derivative.IsValid() &&
		derivative.LengthSquared() >
		    ON_ZERO_TOLERANCE * ON_ZERO_TOLERANCE;
	    const ON_3dVector displacement = outside_vertex - terminal_point;
	    const double parameter_delta = derivative_valid ?
		(displacement * derivative) / derivative.LengthSquared() : 0.0;
	    const bool outward = both_at_maximum ?
		parameter_delta > parameter_guard :
		parameter_delta < -parameter_guard;
	    if (start_at_terminal != end_at_terminal && outward &&
		    extension_limit > 0.0 && outside_gap <= extension_limit) {
		std::unique_ptr<ON_Curve> extended(curve->DuplicateCurve());
		const double proposed_parameter = terminal_parameter +
		    2.0 * parameter_delta;
		ON_Interval extended_domain = domain;
		if (both_at_maximum)
		    extended_domain.Set(domain.Min(), proposed_parameter);
		else
		    extended_domain.Set(proposed_parameter, domain.Max());
		bool extended_valid = extended && extended_domain.IsIncreasing() &&
		    extended->Extend(extended_domain) && extended->IsValid();
		double outside_parameter = ON_UNSET_VALUE;
		double continuation_error = DBL_MAX;
		CurveDistanceEvaluator extended_distance(extended.get());
		extended_valid = extended_valid && extended_distance.IsValid() &&
		    extended_distance.ClosestParameter(outside_vertex,
			&outside_parameter, &continuation_error) &&
		    ((both_at_maximum &&
			outside_parameter > domain.Max() + parameter_guard) ||
		     (both_at_minimum &&
			outside_parameter < domain.Min() - parameter_guard)) &&
		    continuation_error <= LocalUnits::tolerance;
		const ON_Interval continuation_interval = both_at_maximum ?
		    ON_Interval(domain.Max(), outside_parameter) :
		    ON_Interval(outside_parameter, domain.Min());
		extended_valid = extended_valid &&
		    continuation_interval.IsIncreasing() &&
		    extended->Trim(continuation_interval) &&
		    extended->IsValid();
		if (extended_valid) {
		    const double forward_gap =
			extended->PointAtStart().DistanceTo(start) +
			extended->PointAtEnd().DistanceTo(end);
		    const double reverse_gap =
			extended->PointAtStart().DistanceTo(end) +
			extended->PointAtEnd().DistanceTo(start);
		    if (reverse_gap < forward_gap)
			extended_valid = extended->Reverse();
		}
		extended_valid = extended_valid &&
		    extended->PointAtStart().DistanceTo(start) <=
			LocalUnits::tolerance &&
		    extended->PointAtEnd().DistanceTo(end) <=
			LocalUnits::tolerance;
		double continuation_length = 0.0;
		ON_3dPoint previous = extended_valid ?
		    extended->PointAtStart() : ON_3dPoint::UnsetPoint;
		for (int sample = 1; extended_valid &&
			sample <= kCollapsedStepEdgeValidationSegments; ++sample) {
		    const ON_3dPoint point = extended->PointAt(
			extended->Domain().ParameterAt(
			    static_cast<double>(sample) /
				kCollapsedStepEdgeValidationSegments));
		    extended_valid = previous.IsValid() && point.IsValid() &&
			point.DistanceTo(terminal_point) <= extension_limit;
		    if (extended_valid)
			continuation_length += previous.DistanceTo(point);
		    previous = point;
		}
		extended_valid = extended_valid &&
		    continuation_length <=
			extension_limit * kMeasuredToleranceSafetyFactor;
		if (extended_valid) {
		    *wrapped_curve = std::move(extended);
		    if (step) {
			step->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Warning,
			    step_entity_id, "EDGE_CURVE", "edge_geometry",
			    "source NURBS domain ended before an asserted STEP "
			    "vertex; used the exact bounded continuation of its "
			    "terminal polynomial span");
			step->RecordRepair(step_entity_id, "EDGE_CURVE",
			    "edge_geometry",
			    "extended an open NURBS terminal span to its "
			    "asserted STEP vertex");
		    }
		    return true;
		}
	    }
	}
	/* Some exporters retain a separate edge for a feature that collapsed
	 * below their own uncertainty.  Removing it is safe only when both STEP
	 * vertices and the complete vertex-bounded curve locus are densely proven
	 * to fit inside one declared-tolerance ball.  This is intentionally a
	 * safe-mode topology simplification; --exact reports the source defect. */
	const bool allow_collapse = step && !step->ImportOptions().exact &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
	    start.DistanceTo(end) <= LocalUnits::tolerance &&
	    start_distance <= LocalUnits::tolerance &&
	    end_distance <= LocalUnits::tolerance;
	bool locus_collapsed = allow_collapse;
	const ON_Interval collapsed_interval(
	    std::min(start_parameter, end_parameter),
	    std::max(start_parameter, end_parameter));
	for (int sample = 0; locus_collapsed &&
		sample <= kCollapsedStepEdgeValidationSegments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		kCollapsedStepEdgeValidationSegments;
	    const ON_3dPoint point = curve->PointAt(
		collapsed_interval.ParameterAt(fraction));
	    locus_collapsed = point.IsValid() &&
		point.DistanceTo(start) <= LocalUnits::tolerance;
	}
	if (locus_collapsed) {
	    *collapse_edge = true;
	    return true;
	}
	/* Both topology vertices can select one end of a truncated open source
	 * curve even though its complete authored locus is a short terminal
	 * fragment of the asserted edge.  Reuse the same bounded bridge proof as
	 * ordinary increasing and reversed intervals.  The ordinary pass records
	 * only the candidate; construction is confined to the transaction whose
	 * complete BREP must subsequently validate. */
	std::unique_ptr<ON_Curve> topology_bridged_curve;
	double topology_bridge_limit = 0.0;
	const bool topology_bridge_candidate =
	    permissive_curve_inference_allowed(step) &&
	    bounded_topology_vertex_bridge(curve, start, end,
		permissive_curve_inference_enabled(step),
		&topology_bridged_curve, &topology_bridge_limit);
	if (topology_bridge_candidate)
	    step->NoteCurveInferenceCandidate();
	if (topology_bridged_curve && permissive_curve_inference_enabled(step)) {
	    const double inferred_discrepancy =
		std::max(start_distance, end_distance);
	    *wrapped_curve = std::move(topology_bridged_curve);
	    *edge_tolerance = LocalUnits::tolerance;
	    std::ostringstream warning;
	    warning << "both topology vertices selected one source parameter; "
		<< "permissive import retained the complete terminal curve "
		<< "fragment and inserted the missing topology-anchored segment";
	    step->RecordDiagnostic(
		brlcad::step::DiagnosticSeverity::Warning,
		step_entity_id, "EDGE_CURVE", "edge_geometry", warning.str());
	    step->RecordInferredCurve(step_entity_id,
		"topology_vertex_bridge_3d_curve", inferred_discrepancy,
		LocalUnits::tolerance, topology_bridge_limit,
		LocalUnits::tolerance,
		"inserted the minimal topology-vertex connector after both "
		"vertex projections selected the same end of a densely bounded "
		"authored curve fragment");
	    return true;
	}
	if (failure) {
	    std::ostringstream reason;
		reason << "STEP edge vertex parameters do not define a usable "
		    "source interval (" << start_parameter << ':' << end_parameter
		<< " in " << domain.Min() << ':' << domain.Max();
	    if (topology_bridge_candidate)
		reason << ", topology bridge ceiling " << topology_bridge_limit;
	    reason << ')';
	    *failure = reason.str();
	}
	return false;
    }

    *subdomain = direct_interval ? ON_Interval(start_parameter, end_parameter) :
	(reversed_open_interval ? ON_Interval(end_parameter, start_parameter) :
	ON_Interval(start_parameter, end_parameter + domain.Length()));
    std::unique_ptr<ON_Curve> bounded(curve->DuplicateCurve());
    if (!bounded) {
	if (failure) *failure =
	    "could not duplicate the vertex-bounded source curve";
	return false;
    }
    if (wrapped_interval &&
	    !bounded->ChangeClosedCurveSeam(start_parameter)) {
	if (failure) {
	    std::ostringstream reason;
	    reason << "could not move the closed source-curve seam to "
		<< start_parameter << " in domain " << domain.Min() << ':'
		<< domain.Max();
	    *failure = reason.str();
	}
	return false;
    }
    const ON_Interval bounded_domain = bounded->Domain();
    if (!bounded->Trim(*subdomain)) {
	if (failure) {
	    std::ostringstream reason;
	    reason << "could not trim the source curve to vertex-derived "
		"interval " << subdomain->Min() << ':' << subdomain->Max()
		<< " from " << bounded_domain.Min() << ':'
		<< bounded_domain.Max() << " (wrapped="
		<< (wrapped_interval ? 1 : 0) << ')';
	    *failure = reason.str();
	}
	return false;
    }
    if (reversed_open_interval && !bounded->Reverse()) {
	if (failure) {
	    std::ostringstream reason;
	    reason << "could not reverse the vertex-bounded open source curve "
		<< "interval " << subdomain->Min() << ':' << subdomain->Max();
	    *failure = reason.str();
	}
	return false;
    }
    if (!bounded->IsValid()) {
	if (failure) {
	    std::ostringstream reason;
	    reason << "the source curve became invalid after trimming to "
		"vertex-derived interval " << subdomain->Min() << ':'
		<< subdomain->Max() << " from " << bounded_domain.Min() << ':'
		<< bounded_domain.Max();
	    *failure = reason.str();
	}
	return false;
    }

    const bool allow_adjustment = step && !step->ImportOptions().exact &&
	step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
    /* A vertex-selected interval can be usable yet collapse completely inside
     * one declared-tolerance ball.  This occurs when two distinct STEP
     * vertices select neighboring parameters on a much longer source spline.
     * Treat the selected interval, rather than the unrelated complete source
     * curve, as the authoritative edge locus.  Dense containment makes this
     * the same bounded safe topology simplification used above when both
     * closest-point solves return one parameter. */
    bool bounded_interval_collapsed = allow_adjustment &&
	start.DistanceTo(end) <= LocalUnits::tolerance;
    for (int sample = 0; bounded_interval_collapsed &&
	    sample <= kCollapsedStepEdgeValidationSegments; ++sample) {
	const ON_3dPoint point = bounded->PointAt(
	    bounded->Domain().ParameterAt(static_cast<double>(sample) /
		kCollapsedStepEdgeValidationSegments));
	bounded_interval_collapsed = point.IsValid() &&
	    point.DistanceTo(start) <= LocalUnits::tolerance;
    }
    if (bounded_interval_collapsed) {
	*collapse_edge = true;
	return true;
    }

    const double maximum_distance = std::max(start_distance, end_distance);
    const double measured_tolerance = maximum_distance > LocalUnits::tolerance ?
	maximum_distance * kMeasuredToleranceSafetyFactor : LocalUnits::tolerance;
    const double bounded_start_distance =
	bounded->PointAtStart().DistanceTo(start);
    const double bounded_end_distance = bounded->PointAtEnd().DistanceTo(end);
    const double accepted_limit = allow_adjustment ?
	maximum_verified_edge_vertex_tolerance(bounded.get(),
	    LocalUnits::tolerance) : LocalUnits::tolerance;
    /* Apply a local feature-size ceiling here, before whole-item scale is
     * available.  Trim construction will subsequently apply the ordinary
     * edge/surface and whole-item validation as an additional requirement. */
    if (!std::isfinite(maximum_distance) ||
	    !std::isfinite(bounded_start_distance) ||
	    !std::isfinite(bounded_end_distance) ||
	    maximum_distance > accepted_limit ||
	    bounded_start_distance > accepted_limit ||
	    bounded_end_distance > accepted_limit) {
	const double inferred_discrepancy = std::max(maximum_distance,
	    std::max(bounded_start_distance, bounded_end_distance));
	const double permissive_limit =
	    maximum_permissive_edge_vertex_tolerance(bounded.get(),
		LocalUnits::tolerance);
	const bool interval_inference_candidate =
	    permissive_curve_inference_allowed(step) &&
	    std::isfinite(inferred_discrepancy) &&
	    inferred_discrepancy <= permissive_limit;

	/* Some tolerant-topology exporters retain only the terminal source-curve
	 * fragment next to one exact EDGE_CURVE vertex, while the other asserted
	 * vertex identifies the end of an omitted span.  Merely enlarging the edge
	 * tolerance would leave a geometric gap.  In permissive mode, construct
	 * the minimal straight connector and preserve the complete source fragment
	 * only after dense chord-envelope and path-length proofs establish that it
	 * is a local completion of this topology edge.  Curved or displaced
	 * fragments which do not have this evidence continue to fail or use the
	 * ordinary bounded-interval inference above. */
	std::unique_ptr<ON_Curve> topology_bridged_curve;
	double topology_bridge_limit = 0.0;
	const bool topology_bridge_candidate =
	    permissive_curve_inference_allowed(step) &&
	    (direct_interval || reversed_open_interval) &&
	    bounded_topology_vertex_bridge(bounded.get(), start, end,
		permissive_curve_inference_enabled(step),
		&topology_bridged_curve, &topology_bridge_limit);
	const bool inference_candidate = interval_inference_candidate ||
	    topology_bridge_candidate;
	if (inference_candidate)
	    step->NoteCurveInferenceCandidate();
	if (topology_bridged_curve && permissive_curve_inference_enabled(step)) {
	    *wrapped_curve = std::move(topology_bridged_curve);
	    *edge_tolerance = LocalUnits::tolerance;
	    std::ostringstream warning;
	    warning << "source curve omitted " << inferred_discrepancy
		<< " mm next to one exact topology endpoint, beyond the bounded "
		<< "safe-repair limit " << accepted_limit << "; permissive "
		<< "import inserted a topology-anchored segment and preserved "
		<< "the complete authored curve fragment";
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		step_entity_id, "EDGE_CURVE", "edge_geometry", warning.str());
	    step->RecordInferredCurve(step_entity_id,
		"topology_vertex_bridge_3d_curve", inferred_discrepancy,
		accepted_limit, topology_bridge_limit, LocalUnits::tolerance,
		"inserted the minimal topology-vertex connector after one exact "
		"fragment endpoint and dense chord-envelope/path proofs");
	    return true;
	}
	if (interval_inference_candidate &&
		permissive_curve_inference_enabled(step)) {
	    if (wrapped_interval || reversed_open_interval)
		*wrapped_curve = std::move(bounded);
	    else
		*use_subdomain = true;
	    *edge_tolerance = std::max(LocalUnits::tolerance,
		inferred_discrepancy * kMeasuredToleranceSafetyFactor);
	    std::ostringstream warning;
	    warning << "source curve and topology vertices contradict by "
		<< inferred_discrepancy << " mm, beyond the bounded safe-repair "
		<< "limit " << accepted_limit << "; permissive import retained "
		<< "the vertex-selected curve interval with inferred tolerance "
		<< *edge_tolerance;
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		step_entity_id, "EDGE_CURVE", "edge_geometry", warning.str());
	    step->RecordInferredCurve(step_entity_id,
		"vertex_bounded_3d_curve", inferred_discrepancy,
		accepted_limit, permissive_limit, LocalUnits::tolerance,
		"retained the source curve locus and inferred its association "
		"with the authored topology vertices");
	    return true;
	}
	if (failure) {
	    std::ostringstream reason;
	    reason << "STEP topology vertices miss the bounded source curve by "
		<< maximum_distance << " mm (accepted " << accepted_limit
		<< ", declared " << LocalUnits::tolerance;
	    if (permissive_curve_inference_enabled(step)) {
		reason << ", permissive interval ceiling " << permissive_limit;
		if (topology_bridge_candidate)
		    reason << ", topology bridge ceiling " << topology_bridge_limit;
	    }
	    reason << ')';
	    *failure = reason.str();
	}
	return false;
    }

    if (wrapped_interval || reversed_open_interval) {
	*wrapped_curve = std::move(bounded);
	if (step) {
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
		step_entity_id, "EDGE_CURVE", "edge_geometry",
		wrapped_interval ?
		"selected the exact closed source-curve interval across its "
		"parameter seam" :
		"selected an exact reversed open source-curve interval from its "
		"STEP topology vertices");
	}
    } else {
	*use_subdomain = true;
    }
    if (maximum_distance > LocalUnits::tolerance) {
	*edge_tolerance = measured_tolerance;
	if (step) {
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		step_entity_id, "EDGE_CURVE", "edge_geometry",
		"source curve missed a topology vertex; used a measured local "
		"edge tolerance while preserving the exact bounded curve interval");
	}
    }
    if (normalized_closed_endpoint && step) {
	step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
	    step_entity_id, "EDGE_CURVE", "edge_geometry",
	    "normalized an equivalent closed source-curve domain endpoint "
	    "before selecting the exact STEP edge interval");
    }
    return true;
}

bool
EdgeCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0 || GetONId() == kCollapsedStepEdgeIndex) {
	return true;    // already loaded
    }

    if ((false) && (brep->m_E.Count() == 484)) {
	std::cerr << "Debug:LoadONBrep for EdgeCurve:" << brep->m_E.Count() << std::endl;
    }

    Vertex *start = NULL;
    Vertex *end = NULL;
    if (same_sense == 1) {
	start = edge_start;
	end = edge_end;
    } else {
	start = edge_end;
	end = edge_start;
    }

    /* Face topology normally reaches an EDGE_CURVE through ORIENTED_EDGE,
     * which materializes the two endpoint vertices before calling here.
     * EDGE_BASED_WIREFRAME_MODEL and SHELL_BASED_WIREFRAME_MODEL may instead
     * contain an EDGE_CURVE directly.  Make that legal path self-contained
     * rather than indexing m_V with the endpoints' initial -1 ON ids. */
    if (!start || !end || !edge_geometry)
	return false;
    if (start->GetONId() < 0 && !start->LoadONBrep(brep))
	return false;
    if (end->GetONId() < 0 && !end->LoadONBrep(brep))
	return false;

    /* A LINE has a one-to-one parameterization.  Consequently, an
     * EDGE_CURVE whose two topology ends are the same VERTEX_POINT cannot
     * denote a complete closed curve: both ends select the same line
     * parameter and the edge has zero extent.  Building an ON_LineCurve from
     * those coincident ends produces an invalid openNURBS curve before the
     * ordinary bounded-edge repair can inspect it.
     *
     * In safe mode, discard this exact zero-parameter topology edge only after
     * proving that its asserted vertex coincides with the source STEP line's
     * anchor within the declared model uncertainty.  Merely lying somewhere
     * on the infinite line is insufficient: production files use an offset
     * line anchor to expose an accidentally omitted topology vertex and the
     * intended nonzero segment.  The surrounding loop remains connected
     * through the identical vertex.  --exact and --repair none retain the
     * source contradiction and fail normally.  Do not generalize this test to
     * other curve types: a same-vertex circle or closed spline intentionally
     * represents a complete closed edge.
     *
     * A malformed producer may make the LINE itself a point by supplying a
     * zero direction and vector magnitude.  When that point is also the one
     * asserted topology vertex, removing it is even more constrained: the
     * authored curve has no other locus which could encode a missing segment.
     * Preserve that distinct source defect in the repair diagnostics. */
    if (start && end && start->GetONId() >= 0 &&
	    start->GetONId() == end->GetONId() && step &&
	    !step->ImportOptions().exact &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	Line *source_line = dynamic_cast<Line *>(edge_geometry);
	if (source_line) {
	    double raw_origin[3] = {0.0, 0.0, 0.0};
	    double raw_direction_end[3] = {0.0, 0.0, 0.0};
	    source_line->StartPoint(raw_origin);
	    source_line->EndPoint(raw_direction_end);
	    const ON_3dPoint origin(
		raw_origin[0] * LocalUnits::length,
		raw_origin[1] * LocalUnits::length,
		raw_origin[2] * LocalUnits::length);
	    const ON_3dPoint direction_end(
		raw_direction_end[0] * LocalUnits::length,
		raw_direction_end[1] * LocalUnits::length,
		raw_direction_end[2] * LocalUnits::length);
	    const ON_3dPoint vertex = brep->m_V[start->GetONId()].point;
	    const ON_3dVector direction = direction_end - origin;
	    const double direction_squared = direction * direction;
	    const double vertex_anchor_distance =
		origin.IsValid() && vertex.IsValid() ?
		origin.DistanceTo(vertex) : DBL_MAX;
	    const double vertex_parameter = origin.IsValid() && vertex.IsValid() &&
		direction.IsValid() && direction_squared > 0.0 ?
		((vertex - origin) * direction) / direction_squared : 0.0;
	    const ON_3dPoint projected_vertex = origin.IsValid() &&
		direction.IsValid() && std::isfinite(vertex_parameter) ?
		origin + vertex_parameter * direction : ON_3dPoint::UnsetPoint;
	    const double vertex_line_distance = projected_vertex.IsValid() &&
		vertex.IsValid() ? projected_vertex.DistanceTo(vertex) : DBL_MAX;
	    const bool authored_zero_direction =
		std::isfinite(direction_squared) &&
		direction_squared <= ON_ZERO_TOLERANCE * ON_ZERO_TOLERANCE;
	    if (origin.IsValid() && direction_end.IsValid() &&
		    vertex.IsValid() && std::isfinite(direction_squared) &&
		    std::isfinite(vertex_anchor_distance) &&
		    vertex_anchor_distance <= LocalUnits::tolerance) {
		SetONId(kCollapsedStepEdgeIndex);
		step->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Information, id,
		    "EDGE_CURVE", "edge_geometry",
		    authored_zero_direction ?
		    "removed an authored zero-direction same-vertex STEP line "
		    "after proving its source anchor equals the asserted vertex" :
		    "removed a same-vertex STEP line edge after proving its zero "
		    "parameter span and source-anchor coincidence");
		step->RecordRepair(id, "EDGE_CURVE", "edge_geometry",
		    authored_zero_direction ?
		    "removed a zero-locus line edge at its asserted STEP vertex" :
		    "removed a zero-parameter line edge at its asserted STEP vertex");
		return true;
	    }
	    /* LINE.pnt locates an unbounded line; it is not a segment endpoint.
	     * When a nonzero line passes through the one asserted topology vertex,
	     * both EDGE_CURVE ends select the same exact parameter and therefore
	     * define a zero-length edge even if the arbitrary LINE anchor is far
	     * away.  Source data may instead have omitted a second vertex, so this
	     * is not a bounded safe repair.  In permissive mode, try removal only as
	     * a tagged inference transaction: the surrounding loop and complete
	     * BREP must validate before the choice is published. */
	    const bool zero_parameter_inference = !authored_zero_direction &&
		permissive_curve_inference_allowed(step) &&
		std::isfinite(vertex_line_distance) &&
		vertex_line_distance <= LocalUnits::tolerance;
	    if (zero_parameter_inference)
		step->NoteCurveInferenceCandidate();
	    if (zero_parameter_inference &&
		    permissive_curve_inference_enabled(step)) {
		SetONId(kCollapsedStepEdgeIndex);
		std::ostringstream warning;
		warning << "same-vertex STEP line used an arbitrary source anchor "
		    << vertex_anchor_distance << " mm from its topology vertex; "
		    << "permissive import treated the identical projected line "
		    << "parameters as a zero-length topology spur";
		step->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, id,
		    "EDGE_CURVE", "edge_geometry", warning.str());
		std::ostringstream detail;
		detail << "removed the zero-parameter line edge after proving its "
		    << "single topology vertex lies " << vertex_line_distance
		    << " mm from the authored infinite line; LINE anchor offset "
		    << vertex_anchor_distance << " mm is not a segment length";
		step->RecordInferredCurve(id,
		    "same_vertex_line_zero_parameter_removal",
		    vertex_line_distance, LocalUnits::tolerance,
		    LocalUnits::tolerance, LocalUnits::tolerance, detail.str());
		return true;
	    }
	    std::ostringstream reason;
	    reason << "same-vertex STEP line has its source anchor "
		<< vertex_anchor_distance << " mm from the asserted vertex and "
		<< "the vertex is " << vertex_line_distance
		<< " mm from the infinite line; refusing to remove a possibly "
		<< "intended segment or invent the missing topology vertex";
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"EDGE_CURVE", "edge_geometry", reason.str());
	    return false;
	}
    }
    edge_geometry->Start(start);
    edge_geometry->End(end);

    ON_BrepVertex &start_vertex = brep->m_V[start->GetONId()];
    ON_BrepVertex &end_vertex = brep->m_V[end->GetONId()];
    bool geometry_loaded = edge_geometry->LoadONBrep(brep);
    int curve_index = geometry_loaded ? edge_geometry->GetONId() : -1;
    if (!geometry_loaded) {
	/* An unbounded STEP LINE and its explicit EDGE_CURVE vertices can
	 * contradict in a particularly diagnostic way: both vertices project to
	 * the same line parameter even though they define a nonzero topology
	 * segment.  Retaining the vertex segment discards the authored direction,
	 * so it is not a safe local repair.  Permit it only in the globally bounded
	 * whole-solid inference transaction, and only when the complete segment's
	 * maximum separation from the source infinite line is below the item-scale
	 * catastrophe guard.  Final manifold/solid/orientation validation remains
	 * the authority for accepting the transaction. */
	Line *source_line = dynamic_cast<Line *>(edge_geometry);
	double raw_origin[3] = {0.0, 0.0, 0.0};
	double raw_direction_end[3] = {0.0, 0.0, 0.0};
	if (source_line) {
	    source_line->StartPoint(raw_origin);
	    source_line->EndPoint(raw_direction_end);
	}
	const ON_3dPoint source_origin(
	    raw_origin[0] * LocalUnits::length,
	    raw_origin[1] * LocalUnits::length,
	    raw_origin[2] * LocalUnits::length);
	const ON_3dPoint source_direction_end(
	    raw_direction_end[0] * LocalUnits::length,
	    raw_direction_end[1] * LocalUnits::length,
	    raw_direction_end[2] * LocalUnits::length);
	const ON_3dVector source_direction =
	    source_direction_end - source_origin;
	const double direction_squared = source_direction * source_direction;
	const auto line_distance = [&source_origin, &source_direction,
		direction_squared](const ON_3dPoint &point) {
	    if (!source_origin.IsValid() || !source_direction.IsValid() ||
		    !(direction_squared >
			ON_ZERO_TOLERANCE * ON_ZERO_TOLERANCE) ||
		    !point.IsValid())
		return DBL_MAX;
	    const double parameter = ((point - source_origin) *
		source_direction) / direction_squared;
	    const ON_3dPoint projected = source_origin +
		parameter * source_direction;
	    return projected.IsValid() ? projected.DistanceTo(point) : DBL_MAX;
	};
	const double start_separation = line_distance(start_vertex.point);
	const double end_separation = line_distance(end_vertex.point);
	const double maximum_separation = std::max(start_separation,
	    end_separation);
	const double start_parameter = direction_squared > 0.0 ?
	    ((start_vertex.point - source_origin) * source_direction) /
		direction_squared : 0.0;
	const double end_parameter = direction_squared > 0.0 ?
	    ((end_vertex.point - source_origin) * source_direction) /
		direction_squared : 0.0;
	const double projected_span = std::isfinite(start_parameter) &&
	    std::isfinite(end_parameter) ?
	    fabs(end_parameter - start_parameter) * sqrt(direction_squared) :
	    DBL_MAX;
	const ON_BoundingBox item_bounds = brep->BoundingBox();
	const double item_scale = item_bounds.IsValid() ?
	    item_bounds.Diagonal().Length() : 0.0;
	const double whole_item_limit = item_scale *
	    kMaximumWholeItemInferenceRelativeMismatch;
	const bool candidate = source_line && start_vertex.point.IsValid() &&
	    end_vertex.point.IsValid() &&
	    start_vertex.point.DistanceTo(end_vertex.point) >
		ON_ZERO_TOLERANCE &&
	    projected_span <= LocalUnits::tolerance &&
	    std::isfinite(maximum_separation) && whole_item_limit > 0.0 &&
	    maximum_separation <= whole_item_limit &&
	    permissive_curve_inference_allowed(step);
	if (candidate)
	    step->NoteCurveInferenceCandidate();
	if (candidate && step->WholeItemCurveInferenceTrialEnabled()) {
	    std::unique_ptr<ON_LineCurve> inferred_line(new ON_LineCurve(
		start_vertex.point, end_vertex.point));
	    if (inferred_line && inferred_line->IsValid()) {
		curve_index = brep->AddEdgeCurve(inferred_line.release());
		geometry_loaded = curve_index >= 0;
	    }
	    if (geometry_loaded) {
		const double safe_limit = std::max(LocalUnits::tolerance,
		    item_scale * kMaximumRelativeItemMismatch);
		step->RecordInferredCurve(id,
		    "whole_item_topology_vertex_line_segment",
		    maximum_separation, safe_limit, whole_item_limit,
		    LocalUnits::tolerance,
		    "replaced an unbounded STEP line whose direction collapsed "
		    "its two asserted vertex parameters with the explicit "
		    "topology-vertex segment under whole-solid validation");
		step->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, id,
		    "EDGE_CURVE", "edge_geometry",
		    "source LINE direction contradicted its explicit nonzero "
		    "topology-vertex interval; whole-solid inference retained the "
		    "vertex segment and recorded its displacement");
	    }
	}
	if (geometry_loaded)
	    edge_geometry->SetONId(curve_index);
    }
    if (!geometry_loaded) {
	if (step) {
	    std::ostringstream reason;
	    reason << "could not construct source curve #"
		<< edge_geometry->STEPid()
		<< " between the asserted STEP topology vertices";
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"EDGE_CURVE", "edge_geometry", reason.str());
	    if (step->Verbose())
		std::cerr << "Error: " << entityname << " #" << id
		    << "::LoadONBrep() - " << reason.str() << std::endl;
	}
	return false;
    }

    if (curve_index < 0 || curve_index >= brep->m_C3.Count())
	return false;
    ON_Interval curve_subdomain;
    bool use_curve_subdomain = false;
    double edge_tolerance = LocalUnits::tolerance;
    std::unique_ptr<ON_Curve> wrapped_curve;
    bool collapse_edge = false;
    std::string subdomain_failure;
    if (!step_edge_curve_subdomain(brep->m_C3[curve_index],
	    start_vertex.point, end_vertex.point, step, id,
	    start_vertex.m_vertex_index == end_vertex.m_vertex_index,
	    &curve_subdomain, &use_curve_subdomain, &edge_tolerance,
	    &wrapped_curve, &collapse_edge, &subdomain_failure)) {
	if (step)
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"EDGE_CURVE", "edge_geometry",
		"could not bound the source curve to its STEP topology vertices: " +
		subdomain_failure);
	return false;
    }
    if (collapse_edge) {
	const int retained_vertex_index = start_vertex.m_vertex_index;
	const int removed_vertex_index = end_vertex.m_vertex_index;
	if (retained_vertex_index < 0 || removed_vertex_index < 0 ||
		retained_vertex_index == removed_vertex_index ||
		!brep->ChangeVertex(removed_vertex_index,
		    retained_vertex_index, false))
	    return false;
	start->SetONId(retained_vertex_index);
	end->SetONId(retained_vertex_index);
	SetONId(kCollapsedStepEdgeIndex);
	if (step) {
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
		id, "EDGE_CURVE", "edge_geometry",
		"removed a densely validated sub-tolerance STEP edge and merged "
		"its coincident topology vertices");
	    step->RecordRepair(id, "EDGE_CURVE", "edge_geometry",
		"removed a zero-length topology edge within model uncertainty");
	}
	return true;
    }
    if (wrapped_curve) {
	curve_index = brep->AddEdgeCurve(wrapped_curve.release());
	if (curve_index < 0)
	    return false;
    }
    const ON_Interval *curve_domain = use_curve_subdomain ?
	&curve_subdomain : NULL;
    ON_BrepEdge &edge = brep->NewEdge(start_vertex, end_vertex, curve_index,
	curve_domain, edge_tolerance);
    edge.m_tolerance = edge_tolerance;
    edge.m_edge_user.i = id;
    SetONId(edge.m_edge_index);
    if (same_sense != 1) {
	edge.Reverse();
    }

    /* STEP permits a closed topological edge to reference an explicitly open
     * spline.  Production files occasionally use that form with one endpoint
     * rounded just beyond the declared uncertainty even though both ends are
     * unambiguously associated with the same VERTEX_POINT.  OpenNURBS cannot
     * represent that mismatch: an edge with one topology vertex must have a
     * geometrically closed 3-D curve.  In safe mode only, close the duplicate
     * edge curve when both source endpoints lie inside the same scale-bounded
     * mismatch ceiling used by pullback validation, and densely prove that
     * the edit moves no point farther than the resulting local tolerance.
     * --exact retains the source curve and lets structural validation reject
     * it. */
    if (edge.m_vi[0] == edge.m_vi[1] && !edge.IsClosed() && step &&
	    !step->ImportOptions().exact && step->ImportOptions().repair ==
		brlcad::step::RepairMode::Safe) {
	const ON_3dPoint &vertex = brep->m_V[edge.m_vi[0]].point;
	const ON_3dPoint source_start = edge.PointAtStart();
	const ON_3dPoint source_end = edge.PointAtEnd();
	const double start_gap = source_start.IsValid() ?
	    source_start.DistanceTo(vertex) : DBL_MAX;
	const double end_gap = source_end.IsValid() ?
	    source_end.DistanceTo(vertex) : DBL_MAX;
	/* This edit reconciles a source curve endpoint with an authoritative STEP
	 * topology vertex.  Use the dedicated vertex-mismatch ceiling, whose
	 * dense chord-length scale handles nearly closed splines without granting
	 * the same allowance to ordinary edge/surface locus errors. */
	const double adjustment_limit = maximum_verified_edge_vertex_tolerance(
	    edge.EdgeCurveOf(), LocalUnits::tolerance);
	if (start_gap <= adjustment_limit && end_gap <= adjustment_limit) {
	    std::unique_ptr<ON_Curve> candidate(edge.DuplicateCurve());
	    const ON_Interval source_domain = edge.Domain();
	    bool valid = candidate && source_domain.IsIncreasing();
	    const bool start_set = valid && candidate->SetStartPoint(vertex);
	    valid = valid && start_set;
	    const bool end_set = valid && candidate->SetEndPoint(vertex);
	    valid = valid && end_set;
	    const bool dimension_set = valid && candidate->ChangeDimension(3);
	    valid = valid && dimension_set;
	    const bool curve_valid = valid && candidate->IsValid();
	    valid = valid && curve_valid;
	    const bool curve_closed = valid && candidate->IsClosed();
	    valid = valid && curve_closed;
	    double maximum_displacement = 0.0;
	    for (int sample = 0; valid &&
		    sample <= kDenseLiftValidationSegments; ++sample) {
		const double fraction = static_cast<double>(sample) /
		    kDenseLiftValidationSegments;
		const double parameter = source_domain.ParameterAt(fraction);
		const ON_3dPoint original_point = edge.PointAt(parameter);
		const ON_3dPoint candidate_point = candidate->PointAt(parameter);
		const double displacement = original_point.IsValid() &&
		    candidate_point.IsValid() ?
		    original_point.DistanceTo(candidate_point) : DBL_MAX;
		maximum_displacement = std::max(maximum_displacement,
		    displacement);
		valid = displacement <= adjustment_limit;
	    }
	    const double adjusted_tolerance = maximum_displacement *
		kMeasuredToleranceSafetyFactor;
	    valid = valid && adjusted_tolerance <= adjustment_limit;
	    if (valid) {
		const int repaired_curve = brep->AddEdgeCurve(candidate.release());
		if (repaired_curve >= 0 && brep->SetEdgeCurve(edge,
			repaired_curve)) {
		    edge.m_tolerance = std::max(edge.m_tolerance,
			adjusted_tolerance);
		    step->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Warning, id,
			"EDGE_CURVE", "edge_geometry",
			"closed STEP edge curve endpoints exceeded the declared "
			"tolerance; snapped them to the asserted vertex within a "
			"densely measured local tolerance");
		    step->RecordRepair(id, "EDGE_CURVE", "edge_geometry",
			"closed an open spline edge at its asserted STEP topology vertex");
		}
	    } else if (step->Verbose()) {
		std::cerr << "EDGE_CURVE #" << id
		    << ": bounded closed-edge endpoint repair rejected"
		    << " gaps=" << start_gap << '/' << end_gap
		    << " limit=" << adjustment_limit
		    << " start-set=" << start_set
		    << " end-set=" << end_set
		    << " dimension-set=" << dimension_set
		    << " curve-valid=" << curve_valid
		    << " curve-closed=" << curve_closed
		    << " maximum-displacement=" << maximum_displacement
		    << " adjusted-tolerance=" << adjusted_tolerance
		    << std::endl;
	    }
	} else if (step->Verbose()) {
	    std::cerr << "EDGE_CURVE #" << id
		<< ": bounded closed-edge endpoint repair rejected by gaps="
		<< start_gap << '/' << end_gap << " limit="
		<< adjustment_limit << std::endl;
	}
    }

    return true;
}

bool
OrientedEdge::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0 || GetONId() == kCollapsedStepEdgeIndex) {
	return true;    //already loaded
    }

    //TODO: remove debugging code
    //if ((false) && (brep->m_E.Count() == 5)) {
    //std::cerr << "Debug:LoadONBrep for OrientedEdge:" << brep->m_E.Count() << std::endl;
    //}

    if (!edge_start->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!edge_end->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!edge_element->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    SetONId(edge_element->GetONId());

    //TODO: remove debugging code
    //if ((true) && (ON_id == 12)) {
    //std::cerr << "Debug:LoadONBrep for OrientedEdge:" << ON_id << std::endl;
    //}
    return true;
}


ON_BoundingBox *
Path::GetEdgeBounds(ON_Brep *brep)
{
    ON_BoundingBox *u = NULL;

    LIST_OF_ORIENTED_EDGES::iterator i;
    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) {
	    delete u;
	    return NULL;
	}
	if (!(*i)->LoadONBrep(brep)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    delete u;
	    return NULL;
	}
	if ((*i)->GetONId() == kCollapsedStepEdgeIndex)
	    continue;
	if (u == NULL) {
	    u = new ON_BoundingBox();
	}
	const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	const ON_Curve *curve = edge->EdgeCurveOf();
	u->Union(curve->BoundingBox());
    }

    return u;
}


bool
Path::GrowTopologyVertexBounds(ON_BoundingBox *vertex_bounds)
{
    if (!vertex_bounds || edge_list.empty()) return false;
    bool have_vertex = false;
    for (LIST_OF_ORIENTED_EDGES::iterator oriented = edge_list.begin();
	    oriented != edge_list.end(); ++oriented) {
	if (!*oriented) return false;
	Vertex *vertices[2] = {(*oriented)->GetEdgeStart(),
	    (*oriented)->GetEdgeEnd()};
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const double *coordinates = vertices[endpoint] ?
		vertices[endpoint]->Point3d() : NULL;
	    if (!coordinates) return false;
	    const ON_3dPoint point(coordinates[0] * LocalUnits::length,
		coordinates[1] * LocalUnits::length,
		coordinates[2] * LocalUnits::length);
	    if (!point.IsValid()) return false;
	    vertex_bounds->Set(point, true);
	    have_vertex = true;
	}
    }
    return have_vertex;
}


bool
Path::GetEdgeAxisProjectionBounds(ON_Brep *brep,
    const ON_3dPoint &origin, const ON_3dVector &axis,
    double *minimum, double *maximum)
{
    if (!brep || !minimum || !maximum || !axis.IsUnitVector() ||
	    edge_list.empty())
	return false;
    double result_minimum = DBL_MAX;
    double result_maximum = -DBL_MAX;
    bool have_control_point = false;
    for (LIST_OF_ORIENTED_EDGES::iterator oriented = edge_list.begin();
	    oriented != edge_list.end(); ++oriented) {
	if (!*oriented || !(*oriented)->LoadONBrep(brep))
	    return false;
	const int edge_index = (*oriented)->GetONId();
	if (edge_index == kCollapsedStepEdgeIndex)
	    continue;
	const ON_BrepEdge *edge = edge_index >= 0 &&
	    edge_index < brep->m_E.Count() ? &brep->m_E[edge_index] : NULL;
	const ON_Curve *curve = edge ? edge->EdgeCurveOf() : NULL;
	ON_NurbsCurve nurbs;
	if (!curve || !curve->GetNurbForm(nurbs) || nurbs.CVCount() < 1)
	    return false;
	/* With positive weights, a rational NURBS curve is contained by the
	 * convex hull of its Euclidean control points.  Projecting that hull onto
	 * the cone axis gives a conservative exact nappe test without the false
	 * axial extent introduced by a world-axis-aligned curve bounding box. */
	for (int cv = 0; cv < nurbs.CVCount(); ++cv) {
	    const double weight = nurbs.Weight(cv);
	    ON_3dPoint point;
	    if (!std::isfinite(weight) || !(weight > 0.0) ||
		    !nurbs.GetCV(cv, point) || !point.IsValid())
		return false;
	    const double projection = (point - origin) * axis;
	    if (!std::isfinite(projection))
		return false;
	    result_minimum = std::min(result_minimum, projection);
	    result_maximum = std::max(result_maximum, projection);
	    have_control_point = true;
	}
    }
    if (!have_control_point)
	return false;
    *minimum = result_minimum;
    *maximum = result_maximum;
    return true;
}


bool
Path::PrepareONBrepEdges(ON_Brep *brep, std::vector<int> *edge_indices)
{
    if (!brep || !edge_indices)
	return false;
    edge_indices->clear();
    edge_indices->reserve(edge_list.size());
    LIST_OF_ORIENTED_EDGES::iterator i;
    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return false;
	if (!*i || !(*i)->LoadONBrep(brep)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
	const int edge_index = (*i)->GetONId();
	if (edge_index != kCollapsedStepEdgeIndex &&
		(edge_index < 0 || edge_index >= brep->m_E.Count()))
	    return false;
	edge_indices->push_back(edge_index);
    }
    return edge_indices->size() == edge_list.size();
}


bool
Path::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;
    std::vector<int> edge_indices;

    if ((false) && (id == 29429)) {
	std::cerr << "Debug:LoadONBrep for Path:" << id << std::endl;
    }

    if (!PrepareONBrepEdges(brep, &edge_indices))
	return false;
    //TODO: remove debugging code
    if ((false) && (id == 26089)) {
	std::cerr << "Debug:LoadONBrep for Path:" << id << std::endl;
    }
    if (!LoadONTrimmingCurves(brep, ON_path_index, edge_indices)) {
	return false;
    }

    return true;
}


bool
Path::ShiftSurfaceSeam(ON_Brep *brep, double *t)
{
    const ON_BrepLoop *loop = NULL;
    const ON_BrepFace *face = NULL;
    const ON_Surface *surface = NULL;
    double ang_min = 0.0;
    double smin, smax;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    loop = &brep->m_L[ON_path_index];
    if (!loop) {
	/* nothing to do */
	return false;
    }

    face = loop->Face();
    if (!face) {
	/* nothing to do */
	return false;
    }

    surface = face->SurfaceOf();
    if (!surface) {
	/* nothing to do */
	return false;
    }

    if (surface->IsCone() || surface->IsCylinder()) {
	if (surface->IsClosed(0)) {
	    surface->GetDomain(0, &smin, &smax);
	} else {
	    surface->GetDomain(1, &smin, &smax);
	}

	LIST_OF_ORIENTED_EDGES::iterator i;
	for (i = edge_list.begin(); i != edge_list.end(); i++) {
	    if ((*i)->GetONId() == kCollapsedStepEdgeIndex)
		continue;
	    // grab the curve for this edge, face and surface
	    const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	    const ON_Curve *curve = edge->EdgeCurveOf();
	    double tmin, tmax;
	    curve->GetDomain(&tmin, &tmax);

	    if (((tmin < 0.0) && (tmax > 0.0)) && ((tmin > smin) || (tmax < smax))) {
		V_MIN(ang_min, tmin);
	    }

	}

	if (ang_min < 0.0) {
	    *t = ang_min;
	    return true;
	}
    }
    return false;
}


#ifdef _DEBUG_TESTING_
bool _debug_print_ = false;
#endif


/* Insert the standard exact BREP representation for a face boundary that
 * winds around a surface pole.  The two trims of the new seam edge have
 * opposite 3-D orientation and therefore add no geometric boundary; the
 * singular trim connects their UV endpoints across the collapsed pole. */
bool
step_insert_periodic_pole_cut(ON_Brep *brep, ON_BrepLoop &loop,
    const ON_Surface *surface, const ON_BrepTrim &boundary_trim,
    const ON_2dPoint &boundary_end, const ON_2dPoint &boundary_start,
    double tolerance, std::string *failure_reason)
{
    const auto reject = [failure_reason](const char *reason) {
	if (failure_reason)
	    *failure_reason = reason;
	return false;
    };
    if (failure_reason)
	failure_reason->clear();
    if (!brep || !surface || !(tolerance > 0.0) ||
	    boundary_trim.m_vi[1] < 0 ||
	    boundary_trim.m_vi[1] >= brep->m_V.Count())
	return reject("invalid BREP, surface, tolerance, or boundary vertex");
    const ON_BrepFace *face = loop.Face();
    /* A one-loop face has an unambiguous outer boundary even when a producer
     * encoded it with the FACE_BOUND base type and its derived loop flag has
     * not been refreshed yet.  A multi-loop spherical cap may also have an
     * intrinsic full-period outer boundary plus ordinary inner loops (holes).
     * In that case the source/validated outer classification is authoritative
     * and the holes remain untouched.  Never route an inner loop through the
     * pole: that creates an invalid same-loop seam. */
    if (!face || (face->m_li.Count() != 1 &&
	    loop.m_type != ON_BrepLoop::outer))
	return reject("the candidate was not an outer face boundary");

    int seam_direction = -1;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	const double gap = fabs(boundary_start[direction] -
	    boundary_end[direction]);
	const double parameter_tolerance = kPeriodicParameterSnapFraction *
	    std::max(1.0, period);
	const bool endpoints_are_seam_sides =
	    (fabs(boundary_end[direction] - domain.Min()) <= parameter_tolerance &&
	     fabs(boundary_start[direction] - domain.Max()) <= parameter_tolerance) ||
	    (fabs(boundary_end[direction] - domain.Max()) <= parameter_tolerance &&
	     fabs(boundary_start[direction] - domain.Min()) <= parameter_tolerance);
	if (fabs(gap - period) <= parameter_tolerance && endpoints_are_seam_sides) {
	    seam_direction = direction;
	    break;
	}
    }
    if (seam_direction < 0)
	return reject("the boundary endpoints were not on opposite native seam sides");

    const int singular_direction = 1 - seam_direction;
    const ON_Interval singular_domain = surface->Domain(singular_direction);
    const int minimum_surface_side = singular_direction == 0 ? 3 : 0;
    const int maximum_surface_side = singular_direction == 0 ? 1 : 2;
    const bool minimum_is_singular = surface->IsSingular(minimum_surface_side);
    const bool maximum_is_singular = surface->IsSingular(maximum_surface_side);
    int singular_side = -1;
    if (minimum_is_singular && maximum_is_singular) {
	/* A closed boundary on a sphere divides the surface into two exact caps.
	 * Distance to a pole cannot select the intended cap at an equator and is
	 * wrong for a deliberately large complementary cap.  The directed outer
	 * loop is authoritative: increasing traversal in the closed parameter has
	 * its interior on the increasing side of the other parameter, while a
	 * decreasing traversal has its interior on the decreasing side.  STEP
	 * same_sense is retained on the OpenNURBS face rather than folded into a
	 * surface-parameter reversal, so reciprocal cap boundaries keep reciprocal
	 * parameter directions.  A rare inner one-loop encoding exchanges the
	 * selected side. */
	const bool increasing = boundary_end[seam_direction] >
	    boundary_start[seam_direction];
	bool interior_increases = increasing;
	if (loop.m_type == ON_BrepLoop::inner)
	    interior_increases = !interior_increases;
	singular_side = interior_increases ? 1 : 0;
    } else if (minimum_is_singular) {
	singular_side = 0;
    } else if (maximum_is_singular) {
	singular_side = 1;
    }
    if (singular_side < 0)
	return reject("the surface had no singular side in the open parameter direction");

    ON_2dPoint first_pole = boundary_end;
    ON_2dPoint second_pole = boundary_start;
    first_pole[singular_direction] = singular_domain[singular_side];
    second_pole[singular_direction] = singular_domain[singular_side];
    const ON_3dPoint boundary_lift = surface->PointAt(
	boundary_end.x, boundary_end.y);
    const ON_3dPoint start_lift = surface->PointAt(
	boundary_start.x, boundary_start.y);
    const ON_3dPoint first_pole_lift = surface->PointAt(
	first_pole.x, first_pole.y);
    const ON_3dPoint second_pole_lift = surface->PointAt(
	second_pole.x, second_pole.y);
    const ON_BrepVertex &boundary_vertex =
	brep->m_V[boundary_trim.m_vi[1]];
    if (!boundary_lift.IsValid() || !start_lift.IsValid() ||
	    !first_pole_lift.IsValid() || !second_pole_lift.IsValid() ||
	    boundary_lift.DistanceTo(start_lift) > tolerance ||
	    boundary_lift.DistanceTo(boundary_vertex.Point()) > tolerance ||
	    first_pole_lift.DistanceTo(second_pole_lift) > tolerance)
	return reject("the boundary or pole lifts exceeded topology tolerance");

    std::unique_ptr<ON_Curve> seam_curve(surface->IsoCurve(
	singular_direction, boundary_end[seam_direction]));
    const ON_Interval seam_subdomain(
	std::min(boundary_end[singular_direction],
	    singular_domain[singular_side]),
	std::max(boundary_end[singular_direction],
	    singular_domain[singular_side]));
    if (!seam_curve || !seam_subdomain.IsIncreasing() ||
	    !seam_curve->Trim(seam_subdomain) || !seam_curve->IsValid())
	return reject("the exact surface seam subcurve was invalid");
    if (seam_curve->PointAtStart().DistanceTo(boundary_lift) >
	    seam_curve->PointAtEnd().DistanceTo(boundary_lift) &&
	    !seam_curve->Reverse())
	return reject("the exact surface seam could not be oriented from the boundary");
    if (seam_curve->PointAtStart().DistanceTo(boundary_lift) > tolerance ||
	    seam_curve->PointAtEnd().DistanceTo(first_pole_lift) > tolerance)
	return reject("the exact surface seam endpoints exceeded topology tolerance");

    std::unique_ptr<ON_LineCurve> first_trim_curve(
	new ON_LineCurve(boundary_end, first_pole));
    std::unique_ptr<ON_LineCurve> singular_trim_curve(
	new ON_LineCurve(first_pole, second_pole));
    std::unique_ptr<ON_LineCurve> second_trim_curve(
	new ON_LineCurve(second_pole, boundary_start));
    if (!first_trim_curve->ChangeDimension(2) ||
	    !singular_trim_curve->ChangeDimension(2) ||
	    !second_trim_curve->ChangeDimension(2) ||
	    !first_trim_curve->IsValid() || !singular_trim_curve->IsValid() ||
	    !second_trim_curve->IsValid())
	return reject("the seam or singular pcurve was invalid");

    const int seam_curve_index = brep->AddEdgeCurve(seam_curve.release());
    const int first_trim_index = brep->AddTrimCurve(first_trim_curve.release());
    const int singular_trim_index = brep->AddTrimCurve(
	singular_trim_curve.release());
    const int second_trim_index = brep->AddTrimCurve(second_trim_curve.release());
    if (seam_curve_index < 0 || first_trim_index < 0 ||
	    singular_trim_index < 0 || second_trim_index < 0)
	return reject("the seam geometry could not be added to the BREP");

    ON_BrepVertex &pole_vertex = brep->NewVertex(first_pole_lift, tolerance);
    ON_BrepEdge &seam_edge = brep->NewEdge(
	brep->m_V[boundary_trim.m_vi[1]], pole_vertex, seam_curve_index,
	NULL, tolerance);
    /* NewTrim/NewSingularTrim may grow m_T.  Retain indices until all three
     * trims exist; references obtained before a later append can be invalidated
     * and leave required tolerance/ISO fields at OpenNURBS sentinel values. */
    const int first_seam_index = brep->NewTrim(seam_edge, false, loop,
	first_trim_index).m_trim_index;
    const ON_Surface::ISO singular_iso = singular_direction == 0 ?
	(singular_side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
	(singular_side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
    const int pole_trim_index = brep->NewSingularTrim(pole_vertex, loop,
	singular_iso, singular_trim_index).m_trim_index;
    const int second_seam_index = brep->NewTrim(seam_edge, true, loop,
	second_trim_index).m_trim_index;
    ON_BrepTrim &first_seam = brep->m_T[first_seam_index];
    ON_BrepTrim &singular_trim = brep->m_T[pole_trim_index];
    ON_BrepTrim &second_seam = brep->m_T[second_seam_index];

    const ON_Interval seam_domain = surface->Domain(seam_direction);
    const bool first_is_min = fabs(boundary_end[seam_direction] -
	seam_domain.Min()) <= fabs(boundary_end[seam_direction] -
	seam_domain.Max());
    first_seam.m_type = ON_BrepTrim::seam;
    second_seam.m_type = ON_BrepTrim::seam;
    first_seam.m_iso = seam_direction == 0 ?
	(first_is_min ? ON_Surface::W_iso : ON_Surface::E_iso) :
	(first_is_min ? ON_Surface::S_iso : ON_Surface::N_iso);
    second_seam.m_iso = seam_direction == 0 ?
	(first_is_min ? ON_Surface::E_iso : ON_Surface::W_iso) :
	(first_is_min ? ON_Surface::N_iso : ON_Surface::S_iso);
    singular_trim.m_iso = singular_iso;
    first_seam.m_tolerance[0] = first_seam.m_tolerance[1] = tolerance;
    second_seam.m_tolerance[0] = second_seam.m_tolerance[1] = tolerance;
    singular_trim.m_tolerance[0] = singular_trim.m_tolerance[1] = tolerance;
    seam_edge.m_tolerance = tolerance;
    return true;
}


bool
Path::LoadONTrimmingCurves(ON_Brep *brep)
{
    std::vector<int> edge_indices;
    edge_indices.reserve(edge_list.size());
    for (LIST_OF_ORIENTED_EDGES::const_iterator edge = edge_list.begin();
	    edge != edge_list.end(); ++edge) {
	if (!*edge)
	    return false;
	edge_indices.push_back((*edge)->GetONId());
    }
    return LoadONTrimmingCurves(brep, ON_path_index, edge_indices);
}


bool
Path::LoadONTrimmingCurves(ON_Brep *brep, int loop_index,
    const std::vector<int> &edge_indices, double item_scale_override,
    std::shared_ptr<brlcad::PullbackContext> surface_cache)
{
    LIST_OF_ORIENTED_EDGES::iterator i;
    list<PBCData *> curve_pullback_samples;
    std::map<PBCData *, ON_Curve *> exact_pullbacks;
    std::set<PBCData *> rejoined_periodic_pullbacks;
    std::set<PBCData *> split_periodic_pullbacks;
    std::set<PBCData *> native_full_period_pullbacks;

    if (!brep || loop_index < 0 || loop_index >= brep->m_L.Count() ||
	    edge_indices.size() != edge_list.size()) {
	/* nothing to do */
	return false;
    }

    ON_BrepLoop *loop = &brep->m_L[loop_index];
    loop->m_loop_user.i = id;
    ON_BrepFace *face = loop->Face();
    const ON_Surface *surface = face->SurfaceOf();
    const int initial_trim_count = loop->TrimCount();
    const ON_BoundingBox item_bounds = brep->BoundingBox();
    const double local_item_scale = item_bounds.IsValid() ?
	item_bounds.Diagonal().Length() : 0.0;
    const double item_scale = std::max(local_item_scale,
	item_scale_override);
#if 0
    if (surface) {
	double surface_width, surface_height;
	if (surface->GetSurfaceSize(&surface_width, &surface_height)) {
	    // reparameterization of the face's surface and transforms the "u"
	    // and "v" coordinates of all the face's parameter space trimming
	    // curves to minimize distortion in the map from parameter space to 3d..
	    face->SetDomain(0, 0.0, surface_width);
	    face->SetDomain(1, 0.0, surface_height);
	}
    }
#endif
#ifdef _DEBUG_TESTING_
    if (_debug_print_) {
	int curve_cnt = 0;
	for (i = edge_list.begin(); i != edge_list.end(); i++) {
	    // grab the curve for this edge, face and surface
	    const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	    const ON_Curve *curve = edge->EdgeCurveOf();

	    ON_Interval interval = curve->Domain();
	    double delta = interval.Length()/100.0;
	    for(int j =0; j < 100; j++) {
		ON_3dPoint p = curve->PointAt(interval.m_t[0] + j*delta);
		std::cerr << "in pt_" << curve_cnt << " sph " << p.x << " " << p.y << " " << p.z << " 0.1000"  << std::endl;
		curve_cnt++;
	    }
	}
    }
#endif
    // build surface tree making sure not to remove trimmed subsurfaces
    // since currently building trims and need full tree
    // bool removeTrimmed = false;

    //TODO: remove debugging code
    if ((false) && (id == 24894)) {
	std::cerr << "Debug:LoadONTrimmingCurves for Path:" << id << std::endl;
    }
    struct EdgePullbackResult {
	PBCData *data = NULL;
	ON_Curve *exact_curve = NULL;
	std::unique_ptr<ON_Curve> stable_edge_curve;
	const ON_BrepEdge *edge = NULL;
	OrientedEdge *oriented_edge = NULL;
	std::shared_ptr<brlcad::PullbackContext> pullback_context;
	bool orient_with_curve = true;
	bool diagnostic_recorded = false;
	bool inferred_pcurve = false;
	double inferred_discrepancy = 0.0;
	double inferred_safe_limit = 0.0;
    };
    std::vector<OrientedEdge *> ordered_edges;
    std::vector<int> active_edge_indices;
    ordered_edges.reserve(edge_list.size());
    active_edge_indices.reserve(edge_indices.size());
    LIST_OF_ORIENTED_EDGES::const_iterator oriented = edge_list.begin();
    for (size_t edge_index = 0; oriented != edge_list.end();
	    ++oriented, ++edge_index) {
	if (edge_indices[edge_index] == kCollapsedStepEdgeIndex)
	    continue;
	ordered_edges.push_back(*oriented);
	active_edge_indices.push_back(edge_indices[edge_index]);
    }
    if (ordered_edges.empty())
	return false;
    std::vector<EdgePullbackResult> edge_results(ordered_edges.size());
    /* Surface span boxes are immutable and expensive for dense spline faces.
     * Edge jobs keep private solver state and telemetry, but lazily construct
     * this loop-local search cache only once and share it read-only.  Keep the
     * cache bounded to one loop because seam repair may mutate the surface
     * before a later loop on the same face is processed. */
    std::shared_ptr<brlcad::PullbackContext> loop_surface_cache = surface_cache;
    if (!loop_surface_cache)
	loop_surface_cache.reset(new brlcad::PullbackContext());
    const auto construct_edge_pullback = [&](size_t edge_index) {
	EdgePullbackResult &result = edge_results[edge_index];
	result.oriented_edge = ordered_edges[edge_index];
	const int brep_edge_index = active_edge_indices[edge_index];
	if (!result.oriented_edge || brep_edge_index < 0 ||
		brep_edge_index >= brep->m_E.Count()) return;
	result.edge = &brep->m_E[brep_edge_index];
	result.orient_with_curve = result.oriented_edge->OrientWithEdge();
	/* Edge helpers run concurrently.  Keep the mutable closest-point cache and
	 * telemetry private to this edge while reusing it across all bounded retry
	 * passes for that edge.  The surface itself remains shared and immutable. */
	result.pullback_context =
	    loop_surface_cache->ForkWithSharedSurfaceCache();
	/* The parent item reports its bounded-work expiry.  Suppress a second,
	 * misleading "edge #-1" diagnostic when a helper observes that shared
	 * deadline before beginning this particular edge. */
	if (brlcad::PullbackWorkCancelled()) {
	    result.diagnostic_recorded = true;
	    return;
	}
	if (step)
	    step->SetProgressDetail("constructing exact STEP trim pullback",
		result.oriented_edge->STEPid(), 0, 0, std::string(),
		"loop=#" + std::to_string(id));
	/* ON_BrepEdge is a relocatable proxy into the shared BREP edge array, while
	 * EdgeCurveOf() can expose a wider underlying curve than this edge's
	 * authoritative proxy subdomain.  Duplicate the proxy once: openNURBS
	 * preserves its bounded/reversed locus and the retained pullback data gets
	 * stable storage for the complete loop conversion. */
	result.stable_edge_curve.reset(result.edge->DuplicateCurve());
	const ON_Curve *curve = result.stable_edge_curve.get();
	if (!curve || !curve->IsValid()) {
	    result.diagnostic_recorded = true;
	    return;
	}
	if (step && step->Verbose() && step->CurveInferenceTrialEnabled()) {
	    const double start_vertex_gap = result.edge->Vertex(0) ?
		curve->PointAtStart().DistanceTo(
		    result.edge->Vertex(0)->Point()) : DBL_MAX;
	    const double end_vertex_gap = result.edge->Vertex(1) ?
		curve->PointAtEnd().DistanceTo(
		    result.edge->Vertex(1)->Point()) : DBL_MAX;
	    if (start_vertex_gap > LocalUnits::tolerance ||
		    end_vertex_gap > LocalUnits::tolerance) {
		const ON_ClassId *curve_class = curve->ClassId();
		const ON_Curve *underlying = result.edge->EdgeCurveOf();
		const ON_ClassId *underlying_class = underlying ?
		    underlying->ClassId() : NULL;
		std::cerr << "STEP edge #" << result.edge->m_edge_user.i
		    << ": proxy endpoint gaps=" << start_vertex_gap << '/'
		    << end_vertex_gap << ", tolerance="
		    << result.edge->m_tolerance << ", proxy="
		    << (curve_class ? curve_class->ClassName() : "unknown")
		    << ", underlying=" << (underlying_class ?
			underlying_class->ClassName() : "unknown") << std::endl;
	    }
	}
	PBCData *edge_data = NULL;

	if ((false) && (id == 34193)) {
	    std::cerr << "Debug:LoadONTrimmingCurves for Path:" << id << std::endl;
	}
	/* Candidate generation must first establish the local surface branch.
	 * Even a small rounding allowance can select a different periodic image
	 * where a long generator lies on a small cylinder.  Keep this initial
	 * search strictly at the declared tolerance.  Safe mode measures any real
	 * source mismatch in the bounded retry below after branch identity is
	 * known. */
	const double planar_tolerance_limit = LocalUnits::tolerance;
	edge_data = exact_planar_pullback(surface, curve, LocalUnits::tolerance,
	    planar_tolerance_limit, &result.exact_curve);
	if (!edge_data)
	    edge_data = exact_isoparametric_line_pullback(surface, curve,
		LocalUnits::tolerance, planar_tolerance_limit,
		result.edge->Vertex(0)->Point(), result.edge->Vertex(1)->Point(),
		&result.exact_curve, result.edge->m_edge_user.i,
		step && step->Verbose());
	/* The strict probe above establishes that no exact surface-boundary
	 * candidate satisfies the declaration.  In safe mode, give those same
	 * finite, topology-anchored candidates the ordinary measured-tolerance
	 * ceiling before falling back to an unconstrained two-dimensional
	 * pullback.  A boundary pcurve only a few exchange ulps outside the
	 * declaration can otherwise lose its coherent native branch and later
	 * appear to have a topology-scale periodic gap.  The helper still densely
	 * validates the complete immutable edge against the exact surface
	 * isocurve; --exact and --repair none retain the strict result. */
	double measured_boundary_limit = planar_tolerance_limit;
	if (!edge_data && step && !step->ImportOptions().exact &&
		step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	    /* Exact isoparametric recognition is a feature-local shortcut, not a
	     * whole-solid repair.  A cumulative shell bounding box can be orders of
	     * magnitude larger than one edge; using that scale here makes every
	     * interior knot isocurve on a dense NURBS surface look plausible and
	     * can exhaust the item budget before ordinary pullback is attempted.
	     * Bound this preflight by the immutable edge/surface feature itself.
	     * The general pullback path below still has the whole-item allowance and
	     * dense validation needed for a genuine endpoint-anchored repair. */
	    measured_boundary_limit =
		maximum_verified_curve_surface_tolerance(curve, surface,
		    LocalUnits::tolerance, 0.0,
		    result.pullback_context.get());
	    if (measured_boundary_limit > planar_tolerance_limit)
		edge_data = exact_isoparametric_line_pullback(surface, curve,
		    LocalUnits::tolerance, measured_boundary_limit,
		    result.edge->Vertex(0)->Point(),
		    result.edge->Vertex(1)->Point(), &result.exact_curve,
		    result.edge->m_edge_user.i, step->Verbose());
	    if (edge_data && edge_data->tolerance_adjusted) {
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "retained an exact NURBS-boundary pcurve within a densely measured safe tolerance");
		step->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, id,
		    "EDGE_LOOP", "edge_list",
		    "an exact NURBS-boundary pcurve exceeded the declared "
		    "tolerance; used its densely validated measured tolerance");
	    }
	}
	/* A source edge can be continuously projectable at the declared
	 * tolerance while still contradicting the face's complete periodic
	 * boundary topology.  In that case the generic closest-point pcurve is a
	 * locally plausible but globally open branch.  After both strict and
	 * bounded exact-boundary proofs fail, permissive mode may instead retain
	 * the topology-coherent exact surface boundary.  This is explicitly an
	 * inference: cap it by feature/item scale, preserve the immutable 3-D edge,
	 * authorize only the measured local BREP tolerance, and record provenance.
	 * Whole-BREP validation remains mandatory before the result is written as
	 * a region. */
	if (!edge_data && permissive_curve_inference_allowed(step)) {
	    const double permissive_boundary_limit =
		maximum_permissive_curve_surface_tolerance(curve, surface,
		    LocalUnits::tolerance, item_scale,
		    result.pullback_context.get(),
		    step->WholeItemCurveInferenceTrialEnabled());
	    if (step->CurveInferenceTrialEnabled() && step->Verbose()) {
		const ON_BoundingBox curve_bounds = curve->BoundingBox();
		std::cerr << "STEP edge #" << result.edge->m_edge_user.i
		    << ": exact boundary inference limits safe="
		    << measured_boundary_limit << " permissive="
		    << permissive_boundary_limit << " edge-scale="
		    << (curve_bounds.IsValid() ?
			curve_bounds.Diagonal().Length() : 0.0)
		    << " item-scale=" << item_scale << std::endl;
	    }
	    /* The permissive scale limit bounds the measured discrepancy.  The
	     * helper's maximum_tolerance bounds the installed local tolerance,
	     * which deliberately includes the same five-percent measurement
	     * margin used by the generic permissive inference path. */
	    PBCData *inferred_boundary_data = NULL;
	    ON_Curve *inferred_exact_curve = NULL;
	    /* The ordinary build needs only a conservative witness that an
	     * inference retry could help.  Computing the exact minimum separation
	     * for every rejected boundary candidate can require thousands of
	     * global curve searches on a dense edge, even when the ordinary
	     * source-faithful pullback which follows is valid.  Pay for that exact
	     * discrepancy proof only inside the inference transaction which may
	     * actually retain the replacement curve. */
	    if (permissive_boundary_limit > measured_boundary_limit)
		inferred_boundary_data = exact_isoparametric_line_pullback(surface,
		    curve,
		    LocalUnits::tolerance, permissive_boundary_limit *
			kMeasuredToleranceSafetyFactor,
		    result.edge->Vertex(0)->Point(),
		    result.edge->Vertex(1)->Point(), &inferred_exact_curve,
		    result.edge->m_edge_user.i,
		    step->CurveInferenceTrialEnabled() && step->Verbose(),
		    step->CurveInferenceTrialEnabled());
	    if (inferred_boundary_data)
		step->NoteCurveInferenceCandidate();
	    if (inferred_boundary_data &&
		    permissive_curve_inference_enabled(step)) {
		edge_data = inferred_boundary_data;
		result.exact_curve = inferred_exact_curve;
		const double inferred_discrepancy = edge_data->tolerance >
		    LocalUnits::tolerance ? edge_data->tolerance /
		    kMeasuredToleranceSafetyFactor : edge_data->tolerance;
		edge_data->maximum_recovery_tolerance =
		    permissive_boundary_limit * kMeasuredToleranceSafetyFactor;
		ON_BrepEdge *inferred_edge =
		    const_cast<ON_BrepEdge *>(result.edge);
		inferred_edge->m_tolerance = std::max(
		    inferred_edge->m_tolerance, edge_data->tolerance);
		result.inferred_pcurve = true;
		result.inferred_discrepancy = inferred_discrepancy;
		result.inferred_safe_limit = measured_boundary_limit;
		step->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, id,
		    "EDGE_LOOP", "edge_list",
		    "source edge and periodic surface boundary contradicted "
		    "beyond the bounded safe-repair limit; permissive import "
		    "retained the topology-coherent surface-boundary pcurve; "
		    "per-edge bounds are recorded in inferred_curves");
		if (step->Verbose())
		    std::cerr << "STEP edge #" << result.edge->m_edge_user.i
			<< ": periodic boundary discrepancy="
			<< inferred_discrepancy << " mm, safe limit="
			<< measured_boundary_limit << " mm, inference limit="
			<< permissive_boundary_limit << " mm" << std::endl;
		step->RecordInferredCurve(result.edge->m_edge_user.i,
		    step->WholeItemCurveInferenceTrialEnabled() ?
		    "whole_item_topology_coherent_surface_boundary_pcurve" :
		    "topology_coherent_surface_boundary_pcurve",
		    inferred_discrepancy, measured_boundary_limit,
		    permissive_boundary_limit, LocalUnits::tolerance,
		    step->WholeItemCurveInferenceTrialEnabled() ?
		    "retained the exact face-boundary pcurve under a globally "
		    "bounded whole-solid inference after the authored 3-D edge "
		    "could not satisfy that periodic boundary within the bounded "
		    "safe-repair limit" :
		    "retained the exact face-boundary pcurve after the authored "
		    "3-D edge could not satisfy that periodic boundary within "
		    "the bounded safe-repair limit");
	    } else {
		destroy_pullback_data(inferred_boundary_data);
		delete inferred_exact_curve;
	    }
	}
	if (!edge_data) {
	    if (step && step->Verbose() && ON_PlaneSurface::Cast(surface))
		std::cerr << "EDGE_LOOP #" << id
		    << ": exact planar trim rejected for edge #"
		    << result.oriented_edge->STEPid()
		    << "; using bounded pullback" << std::endl;
	    /* A strict first pass on a genuinely two-direction multi-span NURBS
	     * surface can reject every sample before establishing any continuous UV
	     * seed, forcing a global surface-tree search at every refinement point.
	     * In safe mode only, start within the same scale-bounded mismatch ceiling
	     * used by the measured retry.  Analytic and one-direction periodic
	     * surfaces retain strict-first projection because their equivalent
	     * parameter branches are cheap to find and must not be selected using an
	     * enlarged neighboring-edge tolerance.  Closed surfaces whose
	     * uncertainty is below item_scale*sqrt(machine epsilon) also retain
	     * strict-first projection, since a wider initial search cannot
	     * numerically resolve their competing branches.  An open surface has no
	     * equivalent periodic image, so cumulative whole-item scale must not
	     * disable its bounded continuity seed.  Dense locus validation remains
	     * authoritative for every accepted NURBS result. */
	    const double scale_relative_projection_floor = item_scale > 0.0 ?
		item_scale * sqrt(DBL_EPSILON) : 0.0;
	    const bool open_nurbs_surface = !surface->IsClosed(0) &&
		!surface->IsClosed(1);
	    const bool bounded_nurbs_first_pass = step &&
		!step->ImportOptions().exact &&
		step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
		ON_NurbsSurface::Cast(surface) != NULL &&
		surface->SpanCount(0) > 1 && surface->SpanCount(1) > 1 &&
		LocalUnits::tolerance >=
		    kMinimumBoundedNurbsFirstPassToleranceMillimeters &&
		LocalUnits::tolerance <=
		    kMaximumBoundedNurbsFirstPassToleranceMillimeters &&
		(open_nurbs_surface ||
		 LocalUnits::tolerance >= scale_relative_projection_floor);
	    const double bounded_adjustment_limit = bounded_nurbs_first_pass ?
		maximum_verified_edge_tolerance(curve, LocalUnits::tolerance,
		    item_scale) : LocalUnits::tolerance;
	    const double bounded_projection_limit = bounded_nurbs_first_pass ?
		std::max(LocalUnits::tolerance, bounded_adjustment_limit /
		    kMeasuredToleranceSafetyFactor) : LocalUnits::tolerance;
	    const double feature_resolution = short_curve_pullback_resolution(
		curve, LocalUnits::tolerance);
	    /* Keep ordinary edges on the established model-tolerance path.  Only
	     * when the curve itself proves that its feature scale is below that
	     * tolerance do we tighten the numerical solve and flatness criteria.
	     * within_distance_tol remains the independent source-geometry
	     * acceptance bound; conflating it with convergence is what previously
	     * collapsed real sub-tolerance edges to a single UV. */
	    const bool sub_tolerance_feature = feature_resolution <
		0.1 * LocalUnits::tolerance *
		    (1.0 - 16.0 * DBL_EPSILON);
	    const double numerical_resolution = sub_tolerance_feature ?
		feature_resolution : LocalUnits::tolerance;
	    const bool stop_after_decisive_rejection = step &&
		step->ImportOptions().repair == brlcad::step::RepairMode::None;
	    edge_data = pullback_samples(surface, curve, LocalUnits::tolerance,
		numerical_resolution, numerical_resolution,
		bounded_projection_limit, result.pullback_context,
		stop_after_decisive_rejection);
	    if (edge_data && bounded_nurbs_first_pass &&
		edge_data->rejected_projection_samples == 0 &&
		edge_data->maximum_projection_distance > LocalUnits::tolerance) {
		const double measured_tolerance =
		    edge_data->maximum_projection_distance *
		    kMeasuredToleranceSafetyFactor;
		if (measured_tolerance <= bounded_adjustment_limit) {
		    edge_data->tolerance = measured_tolerance;
		    edge_data->flatness = measured_tolerance;
		    edge_data->tolerance_adjusted = true;
		    edge_data->declared_tolerance = LocalUnits::tolerance;
		}
	    }
	    if (edge_data && pullback_sample_count(edge_data) < 2) {
		const double resolution = short_curve_pullback_resolution(curve,
		    LocalUnits::tolerance);
		PBCData *refined = pullback_samples(surface, curve,
		    LocalUnits::tolerance, resolution, resolution,
		    bounded_projection_limit, result.pullback_context);
		if (refined && pullback_sample_count(refined) >= 2) {
		    destroy_pullback_data(edge_data);
		    edge_data = refined;
		    if (step && step->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe)
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "refined pullback resolution for a feature below model tolerance");
		} else {
		    destroy_pullback_data(refined);
		}
	    }
	    /* A STEP edge shared by two faces may establish a densely measured
	     * edge tolerance on the first face before the second face's periodic
	     * pullback collapses to one seed.  Preserve the strict first projection
	     * used for branch identity, then let collapsed-path recovery reuse that
	     * already validated shared-edge tolerance.  This is narrower than a new
	     * tolerance search and --exact never inherits a safe-mode adjustment. */
	    if (edge_data && pullback_sample_count(edge_data) < 2 && result.edge &&
		    step && !step->ImportOptions().exact &&
		    step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
		    result.edge->m_tolerance > edge_data->tolerance) {
		const double adjustment_limit =
		    maximum_verified_curve_surface_tolerance(curve, surface,
			LocalUnits::tolerance, item_scale,
			result.pullback_context.get());
		if (result.edge->m_tolerance <= adjustment_limit) {
		    edge_data->tolerance = result.edge->m_tolerance;
		    edge_data->tolerance_adjusted = true;
		    edge_data->declared_tolerance = LocalUnits::tolerance;
		}
	    }
	    if (edge_data && edge_data->rejected_projection_samples > 0 && step &&
		    !step->ImportOptions().exact &&
		    step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
		    edge_data->maximum_projection_distance > LocalUnits::tolerance &&
		    edge_data->rejected_projection_samples >
			edge_data->failed_projection_samples) {
		bool endpoint_anchored_extension = false;
		const double adjustment_limit =
		    maximum_verified_curve_surface_tolerance(curve, surface,
			LocalUnits::tolerance, item_scale,
			result.pullback_context.get(),
			&endpoint_anchored_extension);
		const double ordinary_adjustment_limit =
		    maximum_verified_edge_tolerance(curve,
			LocalUnits::tolerance, item_scale);
		double effective_tolerance = edge_data->maximum_projection_distance *
		    kMeasuredToleranceSafetyFactor;
		double measured_tolerance = effective_tolerance;
		/* An open surface has no equivalent periodic parameter branches.
		 * Search its complete scale-bounded safe-repair radius in one pass,
		 * while retaining the declared tolerance for adaptive sampling and
		 * flatness.  The accepted edge tolerance is still reduced to the
		 * largest distance actually measured.  Periodic surfaces keep the
		 * incremental search needed to establish the correct branch. */
		const bool unambiguous_open_surface =
		    !surface->IsClosed(0) && !surface->IsClosed(1);
		if (unambiguous_open_surface)
		    effective_tolerance = adjustment_limit;
		PBCData *accepted_adjustment = NULL;
		for (int attempt = 0;
			attempt < kMaximumMeasuredToleranceRetries &&
			effective_tolerance <= adjustment_limit; ++attempt) {
		    const double sampling_tolerance = unambiguous_open_surface ?
			LocalUnits::tolerance : effective_tolerance;
		    PBCData *adjusted = pullback_samples(surface, curve,
			sampling_tolerance, sampling_tolerance,
			sampling_tolerance, effective_tolerance,
			result.pullback_context);
		    if (adjusted && pullback_sample_count(adjusted) >= 2 &&
			    adjusted->rejected_projection_samples == 0) {
			accepted_adjustment = adjusted;
			measured_tolerance = std::max(measured_tolerance,
			    adjusted->maximum_projection_distance *
			    kMeasuredToleranceSafetyFactor);
			accepted_adjustment->tolerance = measured_tolerance;
			accepted_adjustment->flatness = measured_tolerance;
			break;
		    }
		    double next_tolerance = 0.0;
		    if (adjusted) {
			/* Preserve the most informative bounded failure in case no
			 * retry can validate every sample. */
			edge_data->failure_reason = adjusted->failure_reason;
			edge_data->projection_samples = adjusted->projection_samples;
			edge_data->rejected_projection_samples =
			    adjusted->rejected_projection_samples;
			edge_data->failed_projection_samples =
			    adjusted->failed_projection_samples;
			edge_data->maximum_projection_distance =
			    adjusted->maximum_projection_distance;
			if (adjusted->maximum_projection_distance > 0.0) {
			    measured_tolerance = std::max(measured_tolerance,
				adjusted->maximum_projection_distance *
				kMeasuredToleranceSafetyFactor);
			    next_tolerance = std::max(measured_tolerance,
				effective_tolerance * 2.0);
			}
		    }
		    destroy_pullback_data(adjusted);
		    if (!(next_tolerance > effective_tolerance)) break;
		    effective_tolerance = next_tolerance;
		}
		if (accepted_adjustment) {
		    destroy_pullback_data(edge_data);
		    edge_data = accepted_adjustment;
		    edge_data->tolerance_adjusted = true;
		    edge_data->declared_tolerance = LocalUnits::tolerance;
		    if (endpoint_anchored_extension &&
			    measured_tolerance > ordinary_adjustment_limit)
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "used an endpoint-anchored measured tolerance for interior source curve/surface drift");
		}
		    }
		    if (edge_data && edge_data->rejected_projection_samples > 0 &&
			    edge_data->failure_reason ==
				PullbackFailureReason::SurfaceDistanceExceeded &&
			    std::isfinite(edge_data->maximum_projection_distance) &&
			    edge_data->maximum_projection_distance > 0.0 &&
			    permissive_curve_inference_allowed(step)) {
			const double permissive_limit =
			    maximum_permissive_curve_surface_tolerance(curve,
				surface, LocalUnits::tolerance, item_scale,
				result.pullback_context.get(),
				step->WholeItemCurveInferenceTrialEnabled());
			if (edge_data->maximum_projection_distance <=
				permissive_limit)
			    step->NoteCurveInferenceCandidate();
		    }
		    /* When the source EDGE_CURVE and its owning surface are explicitly
	     * associated but contradict one another beyond the safe-repair limit,
	     * permissive import may still construct the closest continuous pcurve.
	     * This is an inference, not a safe tolerance repair: cap the search by
	     * feature/item scale, record its provenance, and retain the wider
	     * authorization for every subsequent dense-locus check.  The complete
	     * B-rep must still validate before it is written as a region. */
	    if (edge_data && edge_data->rejected_projection_samples > 0 &&
		    edge_data->failure_reason ==
			PullbackFailureReason::SurfaceDistanceExceeded &&
		    std::isfinite(edge_data->maximum_projection_distance) &&
		    edge_data->maximum_projection_distance > 0.0 &&
		    permissive_curve_inference_enabled(step)) {
	const double safe_limit =
	    maximum_verified_curve_surface_tolerance(curve, surface,
		LocalUnits::tolerance, item_scale,
		result.pullback_context.get());
	const double ordinary_permissive_limit =
	    maximum_permissive_curve_surface_tolerance(curve, surface,
		LocalUnits::tolerance, item_scale,
		result.pullback_context.get(),
		step->WholeItemCurveInferenceTrialEnabled());
	const double topology_bridge_limit =
	    topology_bridge_curve_surface_tolerance(step,
		result.edge ? result.edge->m_edge_user.i : -1, curve, surface,
		LocalUnits::tolerance, item_scale, result.pullback_context);
	const double permissive_limit = std::max(ordinary_permissive_limit,
	    topology_bridge_limit);
	const bool projected_topology_bridge = topology_bridge_limit >
	    ordinary_permissive_limit;
	double search_tolerance = std::max(LocalUnits::tolerance,
		    edge_data->maximum_projection_distance *
			kMeasuredToleranceSafetyFactor);
		PBCData *inferred = NULL;
		double inferred_tolerance = search_tolerance;
		for (int attempt = 0;
			attempt < kMaximumMeasuredToleranceRetries &&
			search_tolerance <= permissive_limit *
			    kMeasuredToleranceSafetyFactor; ++attempt) {
		    PBCData *candidate = pullback_samples(surface, curve,
			LocalUnits::tolerance, LocalUnits::tolerance,
			LocalUnits::tolerance, search_tolerance,
			result.pullback_context);
		    if (candidate && pullback_sample_count(candidate) >= 2 &&
			    candidate->rejected_projection_samples == 0) {
			inferred = candidate;
			inferred_tolerance = std::max(LocalUnits::tolerance,
			    candidate->maximum_projection_distance *
				kMeasuredToleranceSafetyFactor);
			break;
		    }
		    double next_tolerance = 0.0;
		    if (candidate &&
			    candidate->maximum_projection_distance > 0.0 &&
			    std::isfinite(candidate->maximum_projection_distance))
			next_tolerance = std::max(
			    candidate->maximum_projection_distance *
				kMeasuredToleranceSafetyFactor,
			    search_tolerance * 2.0);
		    destroy_pullback_data(candidate);
		    if (!(next_tolerance > search_tolerance))
			break;
		    search_tolerance = next_tolerance;
		}
		if (inferred && inferred_tolerance <= permissive_limit *
			kMeasuredToleranceSafetyFactor) {
		    destroy_pullback_data(edge_data);
		    edge_data = inferred;
		    edge_data->tolerance = inferred_tolerance;
		    edge_data->flatness = std::max(edge_data->flatness,
			inferred_tolerance);
		    edge_data->tolerance_adjusted = true;
		    edge_data->declared_tolerance = LocalUnits::tolerance;
		    edge_data->maximum_recovery_tolerance =
			permissive_limit * kMeasuredToleranceSafetyFactor;
		    if (result.edge) {
			ON_BrepEdge *inferred_edge =
			    const_cast<ON_BrepEdge *>(result.edge);
			inferred_edge->m_tolerance = std::max(
			    inferred_edge->m_tolerance, inferred_tolerance);
		    }
		    result.inferred_pcurve = true;
		    result.inferred_discrepancy =
			edge_data->maximum_projection_distance;
		    result.inferred_safe_limit = safe_limit;
		    const int edge_id = result.edge ?
			result.edge->m_edge_user.i : -1;
	    step->RecordDiagnostic(
		brlcad::step::DiagnosticSeverity::Warning, id,
		"EDGE_LOOP", "edge_list",
		projected_topology_bridge ?
		"an inference-added topology connector left the owning face; "
		"permissive import projected that bounded connector after the "
		"complete authored curve fragment independently passed surface "
		"validation; per-edge bounds are recorded in inferred_curves" :
		"source curve/surface separation exceeded the bounded "
		"safe-repair limit; permissive import constructed a "
		"surface-projected pcurve with a bounded inferred tolerance; "
		"per-edge bounds are recorded in inferred_curves");
		    if (step->Verbose())
			std::cerr << "STEP edge #" << edge_id
			    << ": curve/surface discrepancy="
			    << result.inferred_discrepancy << ", safe limit="
			    << safe_limit << ", inferred tolerance="
			    << inferred_tolerance << ", inference limit="
			    << permissive_limit << std::endl;
	    step->RecordInferredCurve(edge_id,
		step->WholeItemCurveInferenceTrialEnabled() ?
		(projected_topology_bridge ?
		 "whole_item_topology_bridge_surface_projected_pcurve" :
		 "whole_item_surface_projected_pcurve") :
		(projected_topology_bridge ?
		 "topology_bridge_surface_projected_pcurve" :
		 "surface_projected_pcurve"),
		result.inferred_discrepancy, safe_limit, permissive_limit,
		LocalUnits::tolerance,
		step->WholeItemCurveInferenceTrialEnabled() ?
		(projected_topology_bridge ?
		 "globally bounded whole-solid inference projected only the "
		 "inference-added topology connector after independently "
		 "validating the retained authored curve fragment on the "
		 "explicitly associated face surface" :
		 "globally bounded whole-solid inference projected the authored "
		 "3-D edge onto its explicitly associated face surface") :
		(projected_topology_bridge ?
		"projected only the inference-added topology connector after "
		"independently validating the retained authored curve fragment "
		"on the explicitly associated face surface" :
		"projected the authored 3-D edge onto its explicitly "
		"associated face surface"));
		} else {
		    destroy_pullback_data(inferred);
		}
	    }
	    if (edge_data && edge_data->rejected_projection_samples > 0) {
		if (step) {
		    std::ostringstream reason;
		    const int edge_id = result.edge ? result.edge->m_edge_user.i : -1;
		    if (edge_data->failure_reason == PullbackFailureReason::Cancelled) {
			reason << "the enclosing item's work budget expired while "
			    << "validating STEP edge #" << edge_id;
		    } else if (edge_data->failure_reason == PullbackFailureReason::SurfaceDistanceExceeded) {
			reason << "source curve/surface separation "
			    << edge_data->maximum_projection_distance << " exceeds ";
			if (step->ImportOptions().exact)
			    reason << "the exact model tolerance " << LocalUnits::tolerance;
			else
			    reason << "what could be validated within the bounded "
				"safe-repair limit "
				<< maximum_verified_curve_surface_tolerance(curve,
				    surface, LocalUnits::tolerance, item_scale,
				    result.pullback_context.get())
				<< " (declared "
				<< LocalUnits::tolerance << ')';
			reason << " on STEP edge #" << edge_id
			    << " (" << edge_data->rejected_projection_samples << '/'
			    << edge_data->projection_samples << " projection samples rejected)";
		    } else {
			reason << "bounded closest-point projection failed for STEP edge #"
			    << edge_id << " (" << edge_data->rejected_projection_samples << '/'
			    << edge_data->projection_samples << " samples rejected, "
			    << edge_data->failed_projection_samples
			    << " without a finite closest-point candidate) in STEP loop #"
			    << id;
			const ON_ClassId *surface_class = surface ? surface->ClassId() : NULL;
			if (surface_class)
			    reason << " on " << surface_class->ClassName();
			if (edge_data->maximum_projection_distance > 0.0)
			    reason << "; largest finite rejected distance was "
				<< edge_data->maximum_projection_distance;
		    }
		    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
			"EDGE_LOOP", "edge_list", reason.str());
		    result.diagnostic_recorded = true;
		}
		destroy_pullback_data(edge_data);
		edge_data = NULL;
	    }
	}
	if (edge_data && result.edge && result.edge->Vertex(0) &&
		result.edge->Vertex(1)) {
	    /* An EDGE_CURVE also asserts that its 3-D curve terminates at its two
	     * VERTEX_POINTs.  Some exchange files violate that assertion even when
	     * an analytic or spline curve produced an otherwise exact pullback.
	     * Apply this check after every pullback path, not only the generic
	     * closest-point path.  Reflect the measured endpoint separation in the
	     * affected OpenNURBS edge tolerance only after applying the dedicated
	     * local vertex-mismatch bound already used while constructing that
	     * edge.  The pullback above independently proves the curve/surface
	     * locus, so applying the tighter generic edge/surface bound here would
	     * contradict an accepted vertex association and discard valid trim
	     * data. */
	    const double start_gap = curve->PointAtStart().DistanceTo(
		result.edge->Vertex(0)->Point());
	    const double end_gap = curve->PointAtEnd().DistanceTo(
		result.edge->Vertex(1)->Point());
	    const double endpoint_gap = std::max(start_gap, end_gap);
	    if (ON_IsValid(endpoint_gap) && endpoint_gap > edge_data->tolerance) {
		const double adjustment_limit =
		    maximum_verified_edge_vertex_tolerance(curve,
			LocalUnits::tolerance);
		const bool open_one_vertex_edge = result.edge->m_vi[0] ==
		    result.edge->m_vi[1] && !result.edge->IsClosed();
		const double one_vertex_curve_limit = open_one_vertex_edge ?
		    maximum_permissive_edge_vertex_tolerance(curve,
			LocalUnits::tolerance) : 0.0;
		const double one_vertex_item_limit = std::max(
		    LocalUnits::tolerance, item_scale *
			kZeroLengthTopologyMaximumRelativeItemMismatch);
		const double one_vertex_inference_limit = std::min(
		    one_vertex_curve_limit, one_vertex_item_limit);
		const bool one_vertex_inference_candidate =
		    open_one_vertex_edge &&
		    permissive_curve_inference_allowed(step) &&
		    std::isfinite(endpoint_gap) &&
		    item_scale > 0.0 &&
		    endpoint_gap <= one_vertex_inference_limit;
		if (one_vertex_inference_candidate)
		    step->NoteCurveInferenceCandidate();
		const double effective_tolerance = endpoint_gap *
		    kMeasuredToleranceSafetyFactor;
		const bool inferred_edge_association =
		    step && result.edge && step->HasInferredCurve(
			result.edge->m_edge_user.i, "vertex_bounded_3d_curve") &&
		    result.edge->m_tolerance >= effective_tolerance;
		if ((step && !step->ImportOptions().exact &&
			step->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe &&
			endpoint_gap <= adjustment_limit) ||
			inferred_edge_association) {
		    edge_data->tolerance = effective_tolerance;
		    edge_data->flatness = effective_tolerance;
		    edge_data->tolerance_adjusted = true;
		    edge_data->declared_tolerance = LocalUnits::tolerance;
		} else if (one_vertex_inference_candidate &&
			permissive_curve_inference_enabled(step)) {
		    /* The source graph is already contradictory: one STEP vertex
		     * asserts zero topological extent, while the explicitly associated
		     * open curve ends elsewhere.  Retain that curve only long enough
		     * for the whole-item repair to apply its independent neighboring-
		     * feature and item-scale ceilings and transactionally remove the
		     * sole boundary use.  The inference transaction is published only
		     * if the completed BREP subsequently validates. */
		    edge_data->tolerance = effective_tolerance;
		    edge_data->flatness = effective_tolerance;
		    edge_data->tolerance_adjusted = true;
		    edge_data->declared_tolerance = LocalUnits::tolerance;
		    edge_data->maximum_recovery_tolerance =
			one_vertex_inference_limit *
			kMeasuredToleranceSafetyFactor;
		    ON_BrepEdge *inferred_edge =
			const_cast<ON_BrepEdge *>(result.edge);
		    inferred_edge->m_tolerance = std::max(
			inferred_edge->m_tolerance, effective_tolerance);
		    std::ostringstream warning;
		    warning << "open source curve and one-vertex STEP topology "
			"contradict by " << endpoint_gap
			<< " mm, beyond the bounded safe-repair limit "
			<< adjustment_limit << "; permissive import retained the "
			"spur only for a whole-item bounded zero-length topology "
			"repair";
		    step->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Warning,
			result.edge->m_edge_user.i, "EDGE_CURVE",
			"geometry_inference", warning.str());
		    step->RecordInferredCurve(result.edge->m_edge_user.i,
			"zero_length_topology_spur_removal", endpoint_gap,
			adjustment_limit, one_vertex_inference_limit,
			LocalUnits::tolerance,
			"retained an open curve on an asserted one-vertex edge "
			"only for the independently bounded whole-item topology "
			"repair");
		} else {
		    if (step) {
			std::ostringstream reason;
			reason << "source edge curve endpoint separation "
			    << endpoint_gap << " exceeds ";
			if (step->ImportOptions().exact)
			    reason << "the exact model tolerance "
				<< LocalUnits::tolerance;
			else
			    reason << "what could be validated within the bounded "
				"safe-repair limit " << adjustment_limit
				<< " (declared " << LocalUnits::tolerance << ')';
			reason << " on STEP edge #" << result.edge->m_edge_user.i;
			step->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Error, id,
			    "EDGE_LOOP", "edge_list", reason.str());
			result.diagnostic_recorded = true;
		    }
		    destroy_pullback_data(edge_data);
		    edge_data = NULL;
		}
	    }
	}
	result.data = edge_data;
    };

    if (step)
	step->ParallelForGeometry(edge_results.size(), construct_edge_pullback);
    else
	for (size_t edge_index = 0; edge_index < edge_results.size(); ++edge_index)
	    construct_edge_pullback(edge_index);

    PBCData *data = NULL;
    for (size_t edge_index = 0; edge_index < edge_results.size(); ++edge_index) {
	EdgePullbackResult &result = edge_results[edge_index];
	data = result.data;
	if (data == NULL) {
	    if (step && !result.diagnostic_recorded) {
		std::ostringstream reason;
		reason << "STEP edge #" << (result.edge ?
		    result.edge->m_edge_user.i : -1)
		    << " did not produce bounded pullback data";
		step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		    "EDGE_LOOP", "edge_list", reason.str());
	    }
	    if (step && step->Verbose())
		std::cerr << "EDGE_LOOP #" << id << ": could not construct a validated trim for edge #"
		    << (result.oriented_edge ? result.oriented_edge->STEPid() : -1)
		    << std::endl;
	    continue;
	}
	if (result.exact_curve)
	    exact_pullbacks[data] = result.exact_curve;
	if (step && data->failed_seam_crossing_searches > 0) {
	    for (size_t failure = 0;
		    failure < data->failed_seam_crossing_searches; ++failure)
		step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
		    id, "EDGE_LOOP", "edge_list",
		    "closed-curve seam crossing was deferred to bounded loop-level resolution");
	}

	if (!result.orient_with_curve) {
	    data->order_reversed = true;
	    std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.find(data);
	    if (exact != exact_pullbacks.end() && exact->second && !exact->second->Reverse()) {
		delete exact->second;
		exact_pullbacks.erase(exact);
	    }
	} else {
	    data->order_reversed = false;
	}
	data->edge = result.edge;
	curve_pullback_samples.push_back(data);
	if (!result.orient_with_curve) {
	    list<ON_2dPointArray *>::iterator si;
	    si = data->segments->begin();
	    list<ON_2dPointArray *> rsegs;
	    while (si != data->segments->end()) {
		ON_2dPointArray *samples = (*si);
		samples->Reverse();
		rsegs.push_front(samples);
		si++;
	    }
	    data->segments->clear();
	    si = rsegs.begin();
	    while (si != rsegs.end()) {
		ON_2dPointArray *samples = (*si);
		data->segments->push_back(samples);
		si++;
	    }
	    rsegs.clear();
	}
    }
    if (curve_pullback_samples.size() != ordered_edges.size()) {
	if (step)
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"EDGE_LOOP", "edge_list",
		"one or more STEP edges did not produce validated pullback data");
	for (std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.begin();
	     exact != exact_pullbacks.end(); ++exact)
	    delete exact->second;
	record_pullback_context_statistics(step, curve_pullback_samples);
	while (!curve_pullback_samples.empty()) {
	    data = curve_pullback_samples.front();
	    while (data->segments && !data->segments->empty()) {
		delete data->segments->front();
		data->segments->pop_front();
	    }
	    delete data->segments;
	    delete data;
	    curve_pullback_samples.pop_front();
	}
	return false;
    }
    if (step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	bool removed_excursion = true;
	while (removed_excursion && curve_pullback_samples.size() > 2) {
	    removed_excursion = false;
	    for (std::list<PBCData *>::iterator first =
		    curve_pullback_samples.begin(); first !=
		    curve_pullback_samples.end(); ++first) {
		std::list<PBCData *>::iterator second = first;
		++second;
		if (second == curve_pullback_samples.end())
		    break;
		std::string cancellation_failure;
		if (!pullback_edges_cancel(*first, *second,
			LocalUnits::tolerance, &cancellation_failure)) {
		    if (step->Verbose() && !cancellation_failure.empty())
			std::cerr << "EDGE_LOOP #" << id << ": "
			    << cancellation_failure << std::endl;
		    continue;
		}
		PBCData *first_data = *first;
		PBCData *second_data = *second;
		std::map<PBCData *, ON_Curve *>::iterator exact =
		    exact_pullbacks.find(first_data);
		if (exact != exact_pullbacks.end()) {
		    delete exact->second;
		    exact_pullbacks.erase(exact);
		}
		exact = exact_pullbacks.find(second_data);
		if (exact != exact_pullbacks.end()) {
		    delete exact->second;
		    exact_pullbacks.erase(exact);
		}
		curve_pullback_samples.erase(first);
		curve_pullback_samples.erase(second);
		destroy_pullback_data(first_data);
		destroy_pullback_data(second_data);
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "removed an exact zero-area adjacent STEP edge excursion");
		removed_excursion = true;
		break;
	    }
	}
    }
    if (step && !step->ImportOptions().exact &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	std::string pole_repair_failure;
	if (repair_near_degenerate_nurbs_boundary(curve_pullback_samples,
		brep, face, surface, LocalUnits::tolerance, item_scale,
		step, id, &pole_repair_failure)) {
	    for (std::map<PBCData *, ON_Curve *>::iterator exact =
		    exact_pullbacks.begin(); exact !=
		    exact_pullbacks.end(); ++exact)
		delete exact->second;
	    exact_pullbacks.clear();
	    step->RecordRepair(id, "EDGE_LOOP", "face_geometry",
		"collapsed an identity-proven near-degenerate NURBS pole boundary");
	} else if (step->Verbose() && !pole_repair_failure.empty()) {
	    std::cerr << "EDGE_LOOP #" << id
		<< ": near-degenerate NURBS pole repair retained source: "
		<< pole_repair_failure << std::endl;
	}
    }
    const auto log_adjusted_endpoints = [this](const char *stage,
	const std::list<PBCData *> &pullbacks) {
	if (!step || !step->Verbose()) return;
	for (std::list<PBCData *>::const_iterator current = pullbacks.begin();
	     current != pullbacks.end(); ++current) {
	    const PBCData *current_data = *current;
	    if (!current_data || !current_data->tolerance_adjusted ||
		    !current_data->segments || current_data->segments->empty() ||
		    !current_data->segments->front() ||
		    current_data->segments->front()->Count() == 0)
		continue;
	    const ON_2dPointArray &segment =
		*current_data->segments->front();
	    const ON_2dPoint uv = segment[0];
	    const ON_3dPoint lift = current_data->surf->PointAt(uv.x, uv.y);
	    std::cerr << "EDGE_LOOP #" << id << ": " << stage
		<< " adjusted STEP edge #"
		<< (current_data->edge ? current_data->edge->m_edge_user.i : -1)
		<< " first uv=" << uv.x << ':' << uv.y << " locus error="
		<< (lift.IsValid() ? distance_to_curve(current_data->curve, lift) : DBL_MAX)
		<< " sample-uvs=" << segment[0].x << ':' << segment[0].y;
	    if (segment.Count() > 1)
		std::cerr << ',' << segment[1].x << ':' << segment[1].y;
	    if (segment.Count() > 3)
		std::cerr << ',' << segment[segment.Count() - 2].x << ':'
		    << segment[segment.Count() - 2].y;
	    if (segment.Count() > 2)
		std::cerr << ',' << segment[segment.Count() - 1].x << ':'
		    << segment[segment.Count() - 1].y;
	    std::cerr << std::endl;
	}
    };
    const auto log_fragmented_pullbacks = [this](const char *stage,
	const std::list<PBCData *> &pullbacks, bool include_periodic_edges) {
	if (!step || !step->Verbose()) return;
	for (std::list<PBCData *>::const_iterator current = pullbacks.begin();
	     current != pullbacks.end(); ++current) {
	    const PBCData *fragment_data = *current;
	    if (!fragment_data || !fragment_data->segments ||
		    !fragment_data->surf || !fragment_data->curve)
		continue;
	    const bool periodic_edge = include_periodic_edges &&
		(fragment_data->surf->IsClosed(0) ||
		 fragment_data->surf->IsClosed(1));
	    if (fragment_data->segments->size() < 2 && !periodic_edge)
		continue;
	    const CurveDistanceEvaluator source_distance(fragment_data->curve);
	    std::cerr << "EDGE_LOOP #" << id << ": " << stage
		<< " fragmented STEP edge #"
		<< (fragment_data->edge ? fragment_data->edge->m_edge_user.i : -1)
		<< " fragments=" << fragment_data->segments->size();
	    size_t fragment_index = 0;
	    for (std::list<ON_2dPointArray *>::const_iterator fragment =
		    fragment_data->segments->begin();
		    fragment != fragment_data->segments->end();
		    ++fragment, ++fragment_index) {
		if (!*fragment || (*fragment)->Count() == 0) {
		    std::cerr << " [" << fragment_index << " empty]";
		    continue;
		}
		const ON_2dPoint start = (**fragment)[0];
		const ON_2dPoint end = (**fragment)[(*fragment)->Count() - 1];
		const ON_3dPoint start_lift = fragment_data->surf->PointAt(
		    start.x, start.y);
		const ON_3dPoint end_lift = fragment_data->surf->PointAt(
		    end.x, end.y);
		const double start_error = source_distance.IsValid() &&
		    start_lift.IsValid() ? source_distance.DistanceTo(start_lift,
			std::max(LocalUnits::tolerance,
			    fragment_data->tolerance)) : DBL_MAX;
		const double end_error = source_distance.IsValid() &&
		    end_lift.IsValid() ? source_distance.DistanceTo(end_lift,
			std::max(LocalUnits::tolerance,
			    fragment_data->tolerance)) : DBL_MAX;
		double start_parameter = ON_UNSET_VALUE;
		double end_parameter = ON_UNSET_VALUE;
		double start_parameter_error = DBL_MAX;
		double end_parameter_error = DBL_MAX;
		if (source_distance.IsValid()) {
		    (void)source_distance.ClosestParameter(start_lift,
			&start_parameter, &start_parameter_error);
		    (void)source_distance.ClosestParameter(end_lift,
			&end_parameter, &end_parameter_error);
		}
		std::cerr << " [" << fragment_index << ' ' << start.x << ':'
		    << start.y << "->" << end.x << ':' << end.y << " error="
		    << start_error << ':' << end_error << " source="
		    << start_parameter << ':' << end_parameter << " singular="
		    << IsAtSingularity(fragment_data->surf, start,
			PBC_SEAM_TOL) << ':'
		    << IsAtSingularity(fragment_data->surf, end,
			PBC_SEAM_TOL) << " seam="
		    << IsAtSeam(fragment_data->surf, start, PBC_SEAM_TOL)
		    << ':' << IsAtSeam(fragment_data->surf, end,
			PBC_SEAM_TOL) << ']';
	    }
	    std::cerr << std::endl;
	}
    };
    const auto log_pullback_endpoint_errors = [this](const char *stage,
	const std::list<PBCData *> &pullbacks) {
	if (!step || !step->Verbose()) return;
	for (std::list<PBCData *>::const_iterator current = pullbacks.begin();
	     current != pullbacks.end(); ++current) {
	    const PBCData *endpoint_data = *current;
	    if (!endpoint_data || !endpoint_data->segments ||
		    !endpoint_data->surf || !endpoint_data->curve)
		continue;
	    const CurveDistanceEvaluator source_distance(endpoint_data->curve);
	    if (!source_distance.IsValid()) continue;
	    const double endpoint_tolerance = std::max(LocalUnits::tolerance,
		endpoint_data->tolerance);
	    size_t fragment_index = 0;
	    for (std::list<ON_2dPointArray *>::const_iterator fragment =
		    endpoint_data->segments->begin();
		    fragment != endpoint_data->segments->end();
		    ++fragment, ++fragment_index) {
		if (!*fragment || (*fragment)->Count() == 0) continue;
		const ON_2dPoint endpoints[2] = {
		    (**fragment)[0],
		    (**fragment)[(*fragment)->Count() - 1]
		};
		for (int endpoint = 0; endpoint < 2; ++endpoint) {
		    const ON_3dPoint lift = endpoint_data->surf->PointAt(
			endpoints[endpoint].x, endpoints[endpoint].y);
		    const double error = lift.IsValid() ?
			source_distance.DistanceTo(lift, endpoint_tolerance) : DBL_MAX;
		    if (error <= endpoint_tolerance) continue;
		    std::cerr << "EDGE_LOOP #" << id << ": " << stage
			<< " STEP edge #"
			<< (endpoint_data->edge ?
			    endpoint_data->edge->m_edge_user.i : -1)
			<< " fragment " << fragment_index << " endpoint "
			<< endpoint << " uv=" << endpoints[endpoint].x << ':'
			<< endpoints[endpoint].y << " missed source by " << error
			<< " (limit " << endpoint_tolerance << ')' << std::endl;
		}
	    }
	}
    };
    const auto log_periodic_sample_travel = [this](const char *stage,
	const std::list<PBCData *> &pullbacks) {
	if (!step || !step->Verbose()) return;
	for (std::list<PBCData *>::const_iterator current = pullbacks.begin();
		current != pullbacks.end(); ++current) {
	    const PBCData *travel_data = *current;
	    if (!travel_data || !travel_data->surf || !travel_data->segments)
		continue;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!travel_data->surf->IsClosed(direction))
		    continue;
		const ON_Interval domain = travel_data->surf->Domain(direction);
		const double period = domain.Length();
		if (!(period > ON_ZERO_TOLERANCE))
		    continue;
		bool have_sample = false;
		double first_native = 0.0;
		double previous_native = 0.0;
		double first_unwrapped = 0.0;
		double previous_unwrapped = 0.0;
		double minimum_unwrapped = DBL_MAX;
		double maximum_unwrapped = -DBL_MAX;
		double absolute_travel = 0.0;
		double previous_delta = 0.0;
		size_t sample_count = 0;
		size_t seam_crossings = 0;
		size_t direction_changes = 0;
		for (std::list<ON_2dPointArray *>::const_iterator segment =
			travel_data->segments->begin(); segment !=
			travel_data->segments->end(); ++segment) {
		    if (!*segment) continue;
		    for (int sample = 0; sample < (*segment)->Count(); ++sample) {
			const double native = (**segment)[sample][direction];
			if (!std::isfinite(native)) continue;
			double unwrapped = native;
			if (have_sample) {
			    if (fabs(native - previous_native) > 0.5 * period)
				++seam_crossings;
			    unwrapped += round((previous_unwrapped - native) /
				period) * period;
			    const double delta = unwrapped - previous_unwrapped;
			    absolute_travel += fabs(delta);
			    const double direction_epsilon =
				1.0e-9 * std::max(1.0, period);
			    if (fabs(previous_delta) > direction_epsilon &&
				    fabs(delta) > direction_epsilon &&
				    previous_delta * delta < 0.0)
				++direction_changes;
			    if (fabs(delta) > direction_epsilon)
				previous_delta = delta;
			} else {
			    first_native = native;
			    first_unwrapped = native;
			    have_sample = true;
			}
			previous_native = native;
			previous_unwrapped = unwrapped;
			minimum_unwrapped = std::min(minimum_unwrapped, unwrapped);
			maximum_unwrapped = std::max(maximum_unwrapped, unwrapped);
			++sample_count;
		    }
		}
		if (!have_sample)
		    continue;
		const double net = previous_unwrapped - first_unwrapped;
		const double span = maximum_unwrapped - minimum_unwrapped;
		if (!travel_data->tolerance_adjusted && seam_crossings <= 2 &&
			fabs(net) <= 1.5 * period && absolute_travel <= 2.0 * period)
		    continue;
		std::cerr << "EDGE_LOOP #" << id << ": " << stage
		    << " periodic sample travel STEP edge #"
		    << (travel_data->edge ? travel_data->edge->m_edge_user.i : -1)
		    << " direction=" << direction << " samples=" << sample_count
		    << " native=" << first_native << ':' << previous_native
		    << " unwrapped-net=" << net << " span=" << span
		    << " absolute=" << absolute_travel << " crossings="
		    << seam_crossings << " reversals=" << direction_changes
		    << " period=" << period << std::endl;
	    }
	}
    };
    log_adjusted_endpoints("before seam resolution", curve_pullback_samples);
    log_fragmented_pullbacks("before seam resolution", curve_pullback_samples,
	true);
    log_pullback_endpoint_errors("before seam resolution",
	curve_pullback_samples);
    log_periodic_sample_travel("before seam resolution",
	curve_pullback_samples);
#ifdef _DEBUG_TESTING_
    //TODO: remove debugging
    if (_debug_print_) {
	std::cerr << "Face " << face->m_face_index << " id " << id << std::endl;
	print_pullback_data("Before check_pullback_data", curve_pullback_samples, false);
    }
#endif
    // Exact affine planar pullbacks cannot cross a parametric seam or end at
    // a surface singularity.  Preserve those curves verbatim; the general
    // seam resolver is intentionally limited to periodic/non-planar results.
    const bool exact_open_surface = exact_pullbacks.size() == curve_pullback_samples.size() &&
	!surface->IsClosed(0) && !surface->IsClosed(1);
    std::map<PBCData *, std::vector<ON_2dPointArray> > seam_resolution_snapshot;
    const auto pullback_samples_follow_source = [&](PBCData *pullback) {
	if (!pullback || !pullback->curve || !pullback->surf ||
		!pullback->segments)
	    return false;
	/* pullback_samples() and the exact candidate generators already compare
	 * every stored UV against the source point which produced it.  Consume
	 * that stronger parameter-correspondence proof once.  The flag is cleared
	 * below before seam and topology processing can change the samples, so all
	 * later proposals retain the parameterization-independent locus test. */
	if (pullback->samples_source_validated)
	    return true;
	const CurveDistanceEvaluator source_distance(pullback->curve);
	if (!source_distance.IsValid()) return false;
	double acceptance_tolerance = std::max(LocalUnits::tolerance,
	    pullback->tolerance);
	if (pullback->tolerance_adjusted)
	    acceptance_tolerance = std::max(acceptance_tolerance,
		maximum_authorized_edge_tolerance(pullback, item_scale));
	for (std::list<ON_2dPointArray *>::const_iterator segment =
		pullback->segments->begin(); segment !=
		pullback->segments->end(); ++segment) {
	    if (!*segment || (*segment)->Count() == 0) return false;
	    for (int point = 0; point < (*segment)->Count(); ++point) {
		const ON_2dPoint uv = (**segment)[point];
		const ON_3dPoint lift = pullback->surf->PointAt(uv.x, uv.y);
		if (!lift.IsValid() || source_distance.DistanceTo(lift,
			acceptance_tolerance) > acceptance_tolerance)
		    return false;
	    }
	}
	return true;
    };
    const auto pullback_preserves_topology_endpoints = [&](PBCData *pullback) {
	if (!pullback || !pullback->curve || !pullback->surf ||
		!pullback->segments || pullback->segments->empty() ||
		!pullback->edge)
	    return false;
	/* Distinct topology vertices can bound a real curve shorter than the
	 * declared file uncertainty.  In that case one model-tolerance ball
	 * contains both vertices, so the generic "retained either endpoint"
	 * test below cannot detect a seam operation which collapses the complete
	 * curve onto their midpoint.  Preserve the ordered source-curve endpoint
	 * correspondence at the feature-relative numerical resolution. */
	const double feature_resolution = short_curve_pullback_resolution(
	    pullback->curve, LocalUnits::tolerance);
	const bool sub_tolerance_feature = feature_resolution <
	    0.1 * LocalUnits::tolerance *
		(1.0 - 16.0 * DBL_EPSILON);
	if (sub_tolerance_feature) {
	    ON_2dPointArray *first_segment = pullback->segments->front();
	    ON_2dPointArray *last_segment = pullback->segments->back();
	    if (!first_segment || !last_segment ||
		    first_segment->Count() == 0 || last_segment->Count() == 0)
		return false;
	    const ON_2dPoint sample_endpoints[2] = {
		(*first_segment)[0],
		(*last_segment)[last_segment->Count() - 1]
	    };
	    const ON_3dPoint source_endpoints[2] = {
		pullback->order_reversed ? pullback->curve->PointAtEnd() :
		    pullback->curve->PointAtStart(),
		pullback->order_reversed ? pullback->curve->PointAtStart() :
		    pullback->curve->PointAtEnd()
	    };
	    const double identity_tolerance = std::max(feature_resolution,
		pullback->maximum_projection_distance *
		    kMeasuredToleranceSafetyFactor);
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		const ON_3dPoint lift = pullback->surf->PointAt(
		    sample_endpoints[endpoint].x,
		    sample_endpoints[endpoint].y);
		if (!lift.IsValid() || !source_endpoints[endpoint].IsValid() ||
			lift.DistanceTo(source_endpoints[endpoint]) >
			    identity_tolerance)
		    return false;
	    }
	    return true;
	}
	const int edge_vertices[2] = {pullback->edge->m_vi[0],
	    pullback->edge->m_vi[1]};
	if (edge_vertices[0] < 0 || edge_vertices[1] < 0 ||
		edge_vertices[0] >= brep->m_V.Count() ||
		edge_vertices[1] >= brep->m_V.Count())
	    return false;
	double endpoint_tolerance = std::max(LocalUnits::tolerance,
	    pullback->tolerance);
	if (pullback->edge->m_tolerance > 0.0)
	    endpoint_tolerance = std::max(endpoint_tolerance,
		pullback->edge->m_tolerance);
	bool retained_vertex[2] = {false, false};
	for (std::list<ON_2dPointArray *>::const_iterator segment =
		pullback->segments->begin(); segment !=
		pullback->segments->end(); ++segment) {
	    if (!*segment || (*segment)->Count() == 0)
		return false;
	    const ON_2dPoint endpoints[2] = {(**segment)[0],
		(**segment)[(*segment)->Count() - 1]};
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		const ON_3dPoint lift = pullback->surf->PointAt(
		    endpoints[endpoint].x, endpoints[endpoint].y);
		if (!lift.IsValid()) continue;
		for (int vertex = 0; vertex < 2; ++vertex)
		    retained_vertex[vertex] = retained_vertex[vertex] ||
			lift.DistanceTo(brep->m_V[edge_vertices[vertex]].point) <=
			endpoint_tolerance;
	    }
	}
	return retained_vertex[0] && retained_vertex[1];
    };
    bool seam_resolution_input_valid = !exact_open_surface;
    if (!exact_open_surface) {
	for (PBCData *pullback : curve_pullback_samples) {
	    seam_resolution_input_valid = seam_resolution_input_valid &&
		pullback_samples_follow_source(pullback) &&
		pullback_preserves_topology_endpoints(pullback);
	    if (!pullback || !pullback->segments) continue;
	    std::vector<ON_2dPointArray> &saved =
		seam_resolution_snapshot[pullback];
	    for (ON_2dPointArray *segment : *pullback->segments)
		if (segment) saved.push_back(*segment);
	}
    }
    for (PBCData *pullback : curve_pullback_samples)
	if (pullback) pullback->samples_source_validated = false;
    /* Seam normalization can reduce two numerically coincident UV samples to
     * one before libbrep attempts its bounded periodic recovery.  Safe mode
     * authorizes that search up to the same feature- and item-scale ceiling
     * used everywhere else, but acceptance remains tied to the largest error
     * actually measured over the complete recovered edge. */
    if (!exact_open_surface && step && !step->ImportOptions().exact &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	for (PBCData *pullback : curve_pullback_samples) {
	    if (!pullback || !pullback->curve) continue;
	    pullback->maximum_recovery_tolerance = std::max(
		pullback->maximum_recovery_tolerance,
		maximum_verified_edge_tolerance(pullback->curve,
		    LocalUnits::tolerance, item_scale));
	}
    }
    // check for seams and singularities
    if (!exact_open_surface && !check_pullback_data(curve_pullback_samples)) {
	if (step)
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"EDGE_LOOP", "edge_list",
		"could not resolve exact periodic seam or singularity topology");
	for (std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.begin();
	     exact != exact_pullbacks.end(); ++exact)
	    delete exact->second;
	record_pullback_context_statistics(step, curve_pullback_samples);
	while (!curve_pullback_samples.empty()) {
	    destroy_pullback_data(curve_pullback_samples.front());
	    curve_pullback_samples.pop_front();
	}
	return false;
    }
    const auto rollback_invalid_seam_resolution = [&]() {
	/* The rollback protects a tolerance-adjusted STEP edge from a second,
	 * much larger displacement introduced while choosing its periodic UV
	 * branch.  Ordinary periodic pullbacks can pass through temporary UV
	 * states that are resolved later as complete-face pole and topology
	 * proposals; rolling those back here regresses otherwise exact spherical
	 * and conic faces. */
	bool has_tolerance_adjusted_edge = false;
	for (PBCData *pullback : curve_pullback_samples)
	    has_tolerance_adjusted_edge = has_tolerance_adjusted_edge ||
		(pullback && pullback->tolerance_adjusted);
	/* A singly-periodic singular surface has infinitely many UV
	 * representatives at its pole.  Per-edge endpoint comparison cannot
	 * select that branch locally; later face-level pole repair validates the
	 * complete topology.  A doubly-periodic singular surface is different:
	 * neither direction supplies a unique cut, so accepting a temporary
	 * representative which loses an authoritative STEP endpoint can omit an
	 * entire source interval.  The degenerate torus case which motivated this
	 * check is closed in both directions. */
	const bool singular_surface = surface &&
	    (surface->IsSingular(0) || surface->IsSingular(1) ||
	     surface->IsSingular(2) || surface->IsSingular(3));
	const bool doubly_periodic_surface = surface &&
	    surface->IsClosed(0) && surface->IsClosed(1);
	if (exact_open_surface || !seam_resolution_input_valid ||
		(singular_surface && !doubly_periodic_surface))
	    return false;
	bool seam_resolution_locus_valid = true;
	bool seam_resolution_topology_valid = true;
	for (PBCData *pullback : curve_pullback_samples) {
	    seam_resolution_locus_valid = seam_resolution_locus_valid &&
		pullback_samples_follow_source(pullback);
	    seam_resolution_topology_valid = seam_resolution_topology_valid &&
		pullback_preserves_topology_endpoints(pullback);
	}
	/* Preserve the established rollback rule for ordinary periodic surfaces:
	 * only a tolerance-adjusted edge whose exact locus was subsequently
	 * degraded is restored here.  Topology-endpoint identity is additionally
	 * authoritative on a doubly-periodic surface, where choosing the wrong
	 * equivalent branch can silently omit a complete source interval even
	 * though every retained point still lies somewhere on the same curve. */
	const bool degraded_adjusted_locus =
	    !seam_resolution_locus_valid && has_tolerance_adjusted_edge;
	const bool degraded_doubly_periodic_proposal =
	    doubly_periodic_surface &&
	    (!seam_resolution_locus_valid || !seam_resolution_topology_valid);
	if (!degraded_adjusted_locus && !degraded_doubly_periodic_proposal)
	    return false;
	for (std::map<PBCData *, std::vector<ON_2dPointArray> >::const_iterator
		saved = seam_resolution_snapshot.begin();
		saved != seam_resolution_snapshot.end(); ++saved) {
		PBCData *pullback = saved->first;
		if (!pullback || !pullback->segments) continue;
		while (!pullback->segments->empty()) {
		    delete pullback->segments->front();
		    pullback->segments->pop_front();
		}
		for (std::vector<ON_2dPointArray>::const_iterator segment =
			saved->second.begin(); segment != saved->second.end(); ++segment)
		    pullback->segments->push_back(new ON_2dPointArray(*segment));
	}
	if (step) {
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"rolled back periodic seam resolution after exact edge-locus or topology endpoint validation failed");
	    if (step->Verbose())
		std::cerr << "EDGE_LOOP #" << id
		    << ": rolled back the complete periodic seam proposal because "
		       "it degraded an exact STEP edge locus" << std::endl;
	}
	return true;
    };
    if (step && !step->ImportOptions().exact &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	for (PBCData *pullback : curve_pullback_samples) {
	    if (!pullback ||
		    !(pullback->maximum_recovery_distance > pullback->tolerance))
		continue;
	    const double recovery_limit = std::max(pullback->tolerance,
		pullback->maximum_recovery_tolerance);
	    const double measured_tolerance = std::min(recovery_limit,
		pullback->maximum_recovery_distance *
		    kMeasuredToleranceSafetyFactor);
	    if (measured_tolerance >= pullback->maximum_recovery_distance) {
		pullback->tolerance = measured_tolerance;
		pullback->flatness = std::max(pullback->flatness,
		    measured_tolerance);
		pullback->tolerance_adjusted = true;
		pullback->declared_tolerance = LocalUnits::tolerance;
	    }
	}
    }
    const bool safe_seam_proposal = step &&
	step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
    if (!safe_seam_proposal)
	(void)rollback_invalid_seam_resolution();
    log_adjusted_endpoints("after seam resolution", curve_pullback_samples);
    log_fragmented_pullbacks("after seam resolution", curve_pullback_samples,
	true);
    log_pullback_endpoint_errors("after seam resolution",
	curve_pullback_samples);
    log_periodic_sample_travel("after seam resolution",
	curve_pullback_samples);

    if (step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	const size_t restored_vertices = restore_closed_pullback_topology_vertices(
	    curve_pullback_samples, brep, LocalUnits::tolerance);
	for (size_t repair = 0; repair < restored_vertices; ++repair)
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"restored a closed periodic pullback to its declared STEP topology vertex");
    }

    if (step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	size_t normalized_segments = 0;
	const bool normalized = normalize_periodic_pullback_segments(
	    curve_pullback_samples, surface, LocalUnits::tolerance, item_scale,
	    &normalized_segments, step, id);
	const bool seam_proposal_rolled_back =
	    rollback_invalid_seam_resolution();
	if (normalized && !seam_proposal_rolled_back) {
	    for (size_t repair = 0; repair < normalized_segments; ++repair)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "translated an exact pcurve onto the native periodic surface domain");
	}
	std::string pole_cut_failure;
	/* A pole cut is a valid Euclidean-UV representation of a spherical cap
	 * only when the STEP face has one outer bound.  Multi-bound periodic faces
	 * are reconstructed as exact bands after all loops are available; adding
	 * the cut while materializing an individual inner bound creates an invalid
	 * OpenNURBS inner seam. */
	/* A multi-edge conical cap cannot be finalized safely while one
	 * EDGE_LOOP is still being materialized: neighboring faces and shared
	 * edge uses do not yet exist, and a locally plausible cut can conflict
	 * with the completed shell orientation.  Defer that general case to the
	 * whole-BREP transaction.  A sphere is different: its standard
	 * OpenNURBS parameterization double-covers the pole-to-pole profile, so
	 * lift-equivalent endpoints can differ by pi even before a complete shell
	 * exists.  Resolve that face-local ambiguity here, on the face-local
	 * spherical surface, or a translated four-arc cap can be rejected before
	 * the later winding classifier has a Euclidean UV loop to inspect. */
	const bool spherical_pole_profile =
	    surface->IsSphere(NULL, std::max(LocalUnits::tolerance,
		ON_ZERO_TOLERANCE));
	const bool aligned_pole_cut =
	    (curve_pullback_samples.size() == 1 || spherical_pole_profile) &&
	    face->m_li.Count() == 1 &&
	    loop->m_type == ON_BrepLoop::outer &&
	    align_surface_seam_with_periodic_loop_cut(curve_pullback_samples,
		brep, face, surface, LocalUnits::tolerance, step, id,
		&pole_cut_failure);
	if (aligned_pole_cut) {
	    for (std::map<PBCData *, ON_Curve *>::iterator exact =
		    exact_pullbacks.begin(); exact != exact_pullbacks.end(); ++exact)
		delete exact->second;
	    exact_pullbacks.clear();
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"aligned a closed surface seam with an exact pole cut");
	} else if (align_nurbs_surface_seam(curve_pullback_samples, surface,
		LocalUnits::tolerance)) {
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"aligned a periodic NURBS surface seam with its closed edge");
	} else {
	    std::string relocation_failure;
	    if (relocate_surface_seam_away_from_boundary(
		    curve_pullback_samples, brep, face, surface,
		    LocalUnits::tolerance,
		    &relocation_failure)) {
	    /* Exact pcurve objects were expressed in the original surface
	     * parameterization.  The remapped samples are fully lift-validated;
	     * rebuild curves from them rather than reusing stale parameter curves. */
	    for (std::map<PBCData *, ON_Curve *>::iterator exact =
		    exact_pullbacks.begin(); exact != exact_pullbacks.end(); ++exact)
		delete exact->second;
	    exact_pullbacks.clear();
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"relocated a closed rational surface seam away from an exact STEP boundary");
	    } else if (step->Verbose() && !relocation_failure.empty()) {
		std::cerr << "EDGE_LOOP #" << id
		    << ": periodic surface seam relocation rejected: "
		    << relocation_failure << std::endl;
	    }
	    if (step->Verbose() && !pole_cut_failure.empty())
		std::cerr << "EDGE_LOOP #" << id
		    << ": periodic pole-cut alignment rejected: "
		    << pole_cut_failure << std::endl;
	}
    }

    /* Native seam splitting below is transactional across the ordered loop.
     * Its join predicate deliberately tests Euclidean UV closure: OpenNURBS
     * requires adjacent trim endpoints to coincide even when two parameter
     * images lift to the same point on a closed surface. */
    const auto face_local_join_is_exact =
	[surface](const ON_2dPoint &end, const ON_2dPoint &start) {
	    if (!surface || !end.IsValid() || !start.IsValid())
		return false;
	    for (int direction = 0; direction < 2; ++direction) {
		const ON_Interval domain = surface->Domain(direction);
		const double guard = std::max(ON_ZERO_TOLERANCE,
		    kPeriodicParameterSnapFraction *
		    std::max(1.0, domain.Length()));
		if (fabs(end[direction] - start[direction]) > guard)
		    return false;
	    }
	    return true;
	};

    if (step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	     current != curve_pullback_samples.end(); ++current) {
	    if (brlcad::PullbackWorkCancelled()) break;
	    PBCData *current_data = *current;
	    if (!current_data || !current_data->edge)
		continue;
	    size_t edge_uses = 0;
	    PBCData *paired_data = NULL;
	    for (std::list<PBCData *>::const_iterator other = curve_pullback_samples.begin();
		 other != curve_pullback_samples.end(); ++other) {
		if (*other && (*other)->edge && current_data->edge &&
			(*other)->edge->m_edge_index ==
			    current_data->edge->m_edge_index) {
		    ++edge_uses;
		    if (*other != current_data && paired_data == NULL)
			paired_data = *other;
		}
	    }
	    /* Process the pair only from its first occurrence in loop order. */
	    bool current_is_first = true;
	    for (std::list<PBCData *>::const_iterator prior = curve_pullback_samples.begin();
		 prior != current && prior != curve_pullback_samples.end(); ++prior) {
		if (*prior && (*prior)->edge && current_data->edge &&
			(*prior)->edge->m_edge_index ==
			    current_data->edge->m_edge_index) {
		    current_is_first = false;
		    break;
		}
	    }
	if (edge_uses == 2 && current_is_first && snap_seam_pullback_pair(
		curve_pullback_samples, current_data, paired_data, loop->m_type, LocalUnits::tolerance)) {
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "normalized seam pcurve within model tolerance");
	    }
	}
	/* Normalize proven closed boundary edges before closing adjacent samples.
	 * Doing this during trim emission is too late: a boundary line can move an
	 * endpoint after its neighboring seam curve has already been constructed. */
	for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	     current != curve_pullback_samples.end(); ++current) {
	    if (brlcad::PullbackWorkCancelled()) break;
	    PBCData *current_data = *current;
	    if (!current_data || !current_data->segments)
		continue;
	    for (std::list<ON_2dPointArray *>::iterator segment = current_data->segments->begin();
		 segment != current_data->segments->end(); ++segment) {
		if (!*segment || (*segment)->Count() < 2)
		    continue;
		const bool visible_full_period =
		    pullback_has_visible_full_period(current_data, **segment);
		if (!visible_full_period)
		    continue;
		std::string boundary_failure;
		ON_Curve *boundary = closed_edge_iso_line(current_data, **segment,
		    maximum_authorized_curve_surface_tolerance(current_data,
			item_scale), &boundary_failure);
		if (!boundary) {
		    if (step->Verbose() && !boundary_failure.empty())
			std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			    << (current_data->edge ?
				current_data->edge->m_edge_user.i : -1)
			    << " pre-closure full-period reconstruction rejected: "
			    << boundary_failure << std::endl;
		    continue;
		}
		const ON_Interval boundary_domain = boundary->Domain();
		for (int point = 0; point < (*segment)->Count(); ++point) {
		    const ON_3dPoint restored = boundary->PointAt(
			boundary_domain.ParameterAt(static_cast<double>(point) /
			((*segment)->Count() - 1)));
		    (*segment)->At(point)->Set(restored.x, restored.y);
		}
		std::map<PBCData *, ON_Curve *>::iterator previous_exact =
		    exact_pullbacks.find(current_data);
		if (previous_exact != exact_pullbacks.end()) {
		    delete previous_exact->second;
		    exact_pullbacks.erase(previous_exact);
		}
		exact_pullbacks[current_data] = boundary;
		/* The winding was already visible before reconstruction.  Preserve
		 * it from the generic splitter, which would otherwise manufacture an
		 * interior cut in a complete native-domain boundary.  Collapsed
		 * pullbacks never enter this path: their established singularity and
		 * seam recovery must retain the original topology evidence. */
		native_full_period_pullbacks.insert(current_data);
	    }
	}
	/* Collapsed closest-point results have now been restored to their proven
	 * full-period paths.  Pair repeated STEP edges onto opposite native seams
	 * before endpoint closure: if closure sees the still-collapsed samples, it
	 * can legitimately choose one common periodic image and erase the circle's
	 * UV traversal before the later seam pass has any evidence left. */
	std::set<int> restored_pair_edges;
	for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
		current != curve_pullback_samples.end(); ++current) {
	    PBCData *current_data = *current;
	    if (!current_data || !current_data->edge)
		continue;
	    const int edge_index = current_data->edge->m_edge_index;
	    if (!restored_pair_edges.insert(edge_index).second)
		continue;
	    PBCData *paired_data = NULL;
	    size_t edge_uses = 0;
	    for (std::list<PBCData *>::iterator other =
		    curve_pullback_samples.begin(); other !=
		    curve_pullback_samples.end(); ++other) {
		if (!*other || !(*other)->edge ||
			(*other)->edge->m_edge_index != edge_index)
		    continue;
		++edge_uses;
		if (*other != current_data)
		    paired_data = *other;
	    }
	    if (edge_uses == 2 && paired_data && snap_seam_pullback_pair(
		    curve_pullback_samples, current_data, paired_data, loop->m_type,
		    LocalUnits::tolerance))
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "normalized a restored full-period seam pair within model tolerance");
	}
	const size_t endpoint_repairs = snap_pullback_loop_endpoints(
	    curve_pullback_samples, brep, LocalUnits::tolerance);
	if (endpoint_repairs) {
	    for (size_t repair = 0; repair < endpoint_repairs; ++repair)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "snapped adjacent pcurve endpoint within validated edge tolerance");
	}
	if (endpoint_repairs) {
	    bool seam_changed = false;
	    for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
		 current != curve_pullback_samples.end(); ++current) {
		if (brlcad::PullbackWorkCancelled()) break;
		PBCData *current_data = *current;
		if (!current_data || !current_data->edge)
		    continue;
		PBCData *paired_data = NULL;
		size_t edge_uses = 0;
		bool current_is_first = true;
		bool reached_current = false;
		for (std::list<PBCData *>::iterator other = curve_pullback_samples.begin();
		     other != curve_pullback_samples.end(); ++other) {
		    if (other == current) reached_current = true;
			if (*other && (*other)->edge && current_data->edge &&
				(*other)->edge->m_edge_index ==
				    current_data->edge->m_edge_index) {
			++edge_uses;
			if (other != current && !paired_data) paired_data = *other;
			if (other != current && !reached_current) current_is_first = false;
		    }
		    }
		    if (edge_uses == 2 && current_is_first) {
			std::string seam_failure;
			if (snap_seam_pullback_pair(curve_pullback_samples,
				current_data, paired_data, loop->m_type,
				LocalUnits::tolerance, &seam_failure)) {
			    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
				"normalized seam pcurve within model tolerance");
			    seam_changed = true;
			} else if (step->Verbose() && !seam_failure.empty()) {
			    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
				<< (current_data->edge ?
				    current_data->edge->m_edge_user.i : -1)
				<< " seam-pair normalization retained: "
				<< seam_failure << std::endl;
			}
		    }
	    }
	    if (seam_changed) {
		const size_t final_endpoint_repairs = snap_pullback_loop_endpoints(
		    curve_pullback_samples, brep, LocalUnits::tolerance);
		for (size_t repair = 0; repair < final_endpoint_repairs; ++repair)
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"snapped adjacent pcurve endpoint within validated edge tolerance");
	    }
	}
	/* Exact loop closure can select the equivalent endpoint on the opposite
	 * side of a periodic domain.  Re-run whole-segment unwrapping after that
	 * endpoint choice so the newly closed join cannot leave a one-period jump
	 * between the endpoint and its first interior pullback sample. */
	size_t post_snap_normalized_segments = 0;
	if (normalize_periodic_pullback_segments(curve_pullback_samples, surface,
		LocalUnits::tolerance, item_scale,
		&post_snap_normalized_segments, step, id)) {
	    for (size_t repair = 0; repair < post_snap_normalized_segments; ++repair)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "unwrapped a periodic pcurve after exact loop closure");
	    /* Whole-segment unwrapping can legitimately move an endpoint back to
	     * the branch selected by its interior samples.  Reconcile adjacent
	     * branches once more; snap_pullback_loop_endpoints now prefers a proven
	     * whole-trim periodic translation, so this does not recreate an
	     * isolated one-period endpoint spike. */
	    std::string branch_rejection;
	    const size_t branch_repairs = snap_pullback_loop_endpoints(
		curve_pullback_samples, brep, LocalUnits::tolerance,
		&branch_rejection);
	    for (size_t repair = 0; repair < branch_repairs; ++repair)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "reconciled adjacent pcurve branches after periodic unwrapping");
	    if (step->Verbose() && !branch_rejection.empty())
		std::cerr << "EDGE_LOOP #" << id
		    << ": periodic branch reconciliation rejected: "
		    << branch_rejection << std::endl;
    }
    log_pullback_endpoint_errors("before native seam split",
	curve_pullback_samples);
    log_fragmented_pullbacks("before native seam split", curve_pullback_samples,
	true);
    std::map<PBCData *, std::vector<ON_2dPointArray> >
	native_split_snapshot;
    std::vector<PBCData *> native_split_pullbacks;
    std::vector<bool> native_split_exact_joins;
    {
	for (PBCData *pullback : curve_pullback_samples) {
	    if (!pullback || !pullback->segments ||
		    pullback->segments->empty())
		continue;
	    native_split_pullbacks.push_back(pullback);
	    std::vector<ON_2dPointArray> &saved =
		native_split_snapshot[pullback];
	    for (ON_2dPointArray *segment : *pullback->segments)
		if (segment)
		    saved.push_back(*segment);
	}
	for (size_t current = 0; current < native_split_pullbacks.size();
		++current) {
	    const size_t previous = current ? current - 1 :
		native_split_pullbacks.size() - 1;
	    const PBCData *previous_data =
		native_split_pullbacks[previous];
	    const PBCData *current_data =
		native_split_pullbacks[current];
	    const ON_2dPointArray *previous_segment =
		previous_data->segments->back();
	    const ON_2dPointArray *current_segment =
		current_data->segments->front();
	    native_split_exact_joins.push_back(face_local_join_is_exact(
		(*previous_segment)[previous_segment->Count() - 1],
		(*current_segment)[0]));
	}
    }
    for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	     current != curve_pullback_samples.end(); ++current) {
	    if (!*current || !(*current)->segments)
		continue;
	    size_t seam_splits = 0;
	    bool singular_split = pullback_requires_singular_topology_split(
		*current, std::max(LocalUnits::tolerance, (*current)->tolerance));
	    /* A full-period native-domain isocurve has already been densely
	     * validated against the complete immutable STEP edge.  Do not split
	     * it again at an interior parameter retained from an earlier private
	     * surface seam phase; that turns one physical boundary into two
	     * opposite uses of a partial edge and hides it from the annular-band
	     * builder.  A proven surface singularity still requires real topology
	     * subdivision and therefore retains the generic split path. */
	    const bool preserve_native_full_period =
		!singular_split && native_full_period_pullbacks.find(*current) !=
		    native_full_period_pullbacks.end();
	    const bool split_at_native_seam = !preserve_native_full_period &&
		split_periodic_pullback_at_native_seams(*current,
		    LocalUnits::tolerance, &seam_splits);
	    /* A collapsed full-period edge has no visible singular endpoint until
	     * the native-seam splitter restores its complete UV winding.  Recheck
	     * after that topology-changing operation; otherwise the stale
	     * pre-split classification routes a genuine pole cut through the
	     * ordinary-fragment merger and erases the connector topology. */
	    if (!singular_split && split_at_native_seam &&
		    (*current)->segments->size() > 1)
		singular_split = pullback_requires_singular_topology_split(
		    *current, std::max(LocalUnits::tolerance,
			(*current)->tolerance));
	    /* pullback_samples may already have split an open curve at parameter
	     * zero before this native-domain pass.  Rejoin all ordinary fragment
	     * lists, not only lists split by the call immediately above. */
	    std::string fragment_merge_failure;
	    if (singular_split && (*current)->segments->size() > 1) {
		const size_t original_fragment_count = (*current)->segments->size();
		size_t retained_groups = 0;
	if (merge_ordinary_periodic_pullback_fragments(*current,
			LocalUnits::tolerance, &fragment_merge_failure, true,
			&retained_groups)) {
		    singular_split = pullback_requires_singular_topology_split(
			*current, std::max(LocalUnits::tolerance,
			    (*current)->tolerance));
		    if (!singular_split && (*current)->segments->size() == 1)
			rejoined_periodic_pullbacks.insert(*current);
		    for (size_t merge = retained_groups;
			    merge < original_fragment_count; ++merge)
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "rejoined an ordinary periodic fragment around a proven "
			    "surface singularity");
		} else if (step && step->Verbose() &&
			!fragment_merge_failure.empty()) {
		    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			<< ((*current)->edge ? (*current)->edge->m_edge_user.i : -1)
			<< " singular-fragment normalization rejected: "
			<< fragment_merge_failure << std::endl;
		}
	    }
	    fragment_merge_failure.clear();
	    if (!singular_split && (*current)->segments->size() > 1 &&
		    merge_ordinary_periodic_pullback_fragments(*current,
			LocalUnits::tolerance, &fragment_merge_failure)) {
		/* Fragment translation can make the join continuous while a fitted
		 * curve still takes the wrong periodic branch.  Densely reconstruct the
		 * complete authoritative edge before accepting one merged use.  When
		 * that succeeds, retain the validated polyline and its measured local
		 * tolerance; otherwise restore exact native-seam subedges so the invalid
		 * merged branch cannot be hidden by tolerance growth. */
		ON_2dPointArray *merged_samples =
		    (*current)->segments->empty() ? NULL :
		    (*current)->segments->front();
		ON_Curve *merged_pullback = NULL;
		std::string merged_failure;
		double merged_measured_error = 0.0;
		double merged_validation_tolerance = std::max(
		    LocalUnits::tolerance, (*current)->tolerance);
		const bool measured_merge_allowed = step &&
		    !step->ImportOptions().exact &&
		    step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
		const double merged_adjustment_limit = measured_merge_allowed ?
		    maximum_authorized_edge_tolerance(*current, item_scale) :
		    merged_validation_tolerance;
		if (merged_adjustment_limit > merged_validation_tolerance)
		    merged_validation_tolerance = merged_adjustment_limit;
		const bool merged_locus_valid = merged_samples &&
		    refined_fragment_polyline(*current, *merged_samples,
			merged_validation_tolerance, &merged_pullback,
			&merged_failure, true, &merged_measured_error);
		const ON_PolylineCurve *validated_polyline =
		    merged_locus_valid ? ON_PolylineCurve::Cast(merged_pullback) : NULL;
		if (validated_polyline && validated_polyline->m_pline.Count() >= 2) {
		    merged_samples->Empty();
		    merged_samples->Reserve(validated_polyline->m_pline.Count());
		    for (int point = 0; point < validated_polyline->m_pline.Count();
			    ++point)
			merged_samples->Append(ON_2dPoint(
			    validated_polyline->m_pline[point].x,
			    validated_polyline->m_pline[point].y));
		    std::map<PBCData *, ON_Curve *>::iterator old_exact =
			exact_pullbacks.find(*current);
		    if (old_exact != exact_pullbacks.end()) {
			delete old_exact->second;
			exact_pullbacks.erase(old_exact);
		    }
		    exact_pullbacks[*current] = merged_pullback;
		    if (merged_measured_error > std::max(LocalUnits::tolerance,
			    (*current)->tolerance)) {
			const double measured_tolerance = merged_measured_error *
			    kMeasuredToleranceSafetyFactor;
			if (measured_tolerance <= merged_adjustment_limit) {
			    (*current)->tolerance = measured_tolerance;
			    (*current)->flatness = std::max((*current)->flatness,
				measured_tolerance);
			    (*current)->tolerance_adjusted = true;
			    (*current)->declared_tolerance = LocalUnits::tolerance;
			}
		    }
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"reconstructed a rejoined periodic pcurve from the complete exact STEP edge");
		    rejoined_periodic_pullbacks.insert(*current);
		    continue;
		}
		delete merged_pullback;
		size_t merged_seam_splits = 0;
		const bool merged_crosses_native_seam =
		    split_periodic_pullback_at_native_seams(*current,
			LocalUnits::tolerance, &merged_seam_splits) &&
		    (*current)->segments->size() > 1;
		if (merged_crosses_native_seam) {
		    split_periodic_pullbacks.insert(*current);
		    for (size_t split = 0; split < merged_seam_splits; ++split)
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "retained an exact native-seam split after periodic fragment rejoining");
		} else {
		    if (step && step->Verbose() && !merged_failure.empty())
			std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			    << ((*current)->edge ?
				(*current)->edge->m_edge_user.i : -1)
			    << " merged periodic pullback validation rejected: "
			    << merged_failure << std::endl;
		}
	    }
	    if (!singular_split && (*current)->segments->size() > 1 && step &&
		    step->Verbose() && !fragment_merge_failure.empty())
		std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
		    << ((*current)->edge ? (*current)->edge->m_edge_user.i : -1)
		    << " periodic-fragment merge rejected: "
		    << fragment_merge_failure << std::endl;
	    /* A rational periodic surface can evaluate two nominally equivalent
	     * parameter images differently enough that translating the retained
	     * fragments is not a defensible equivalence proof.  In safe-repair
	     * mode, reconstruct one continuous pullback directly from the complete
	     * authoritative STEP edge instead.  refined_fragment_polyline proves
	     * every dense source-curve sample against the surface before returning;
	     * retaining its samples as one edge use avoids widening the periodic
	     * lift-equivalence threshold. */
	    if (!singular_split && (*current)->segments->size() > 1) {
		ON_2dPointArray *seed_samples = NULL;
		for (std::list<ON_2dPointArray *>::const_iterator segment =
			(*current)->segments->begin(); segment !=
			(*current)->segments->end(); ++segment) {
		    if (*segment && (*segment)->Count() >= 2) {
			seed_samples = *segment;
			break;
		    }
		}
		ON_Curve *complete_pullback = NULL;
		std::string complete_failure;
		double complete_measured_error = 0.0;
		const double current_tolerance = std::max(LocalUnits::tolerance,
		    (*current)->tolerance);
		bool complete_valid = seed_samples && refined_fragment_polyline(
		    *current, *seed_samples, current_tolerance,
		    &complete_pullback, &complete_failure, true,
		    &complete_measured_error);
		/* The fragmented samples themselves can expose a slightly larger
		 * source edge/surface discrepancy than the initial adaptive pullback.
		 * Safe mode may retry the immutable complete-edge proposal at the same
		 * scale-bounded ceiling used by the rest of the importer.  A successful
		 * dense pass installs only its measured maximum (plus the standard
		 * numerical safety factor), never the search ceiling. */
		if (!complete_valid && seed_samples && step &&
			!step->ImportOptions().exact &&
			step->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe) {
		    const double adjustment_limit =
			maximum_authorized_edge_tolerance(*current, item_scale);
		    if (adjustment_limit > current_tolerance) {
			double retry_tolerance = current_tolerance;
			for (int retry = 0; !complete_valid &&
				retry < kMaximumMeasuredToleranceRetries; ++retry) {
			    const double measured_retry = complete_measured_error > 0.0 ?
				complete_measured_error *
				    kMeasuredToleranceSafetyFactor : 0.0;
			    retry_tolerance = std::min(adjustment_limit,
				std::max(retry_tolerance * 2.0, measured_retry));
			    if (!(retry_tolerance > current_tolerance))
				break;
			    complete_measured_error = 0.0;
			    complete_valid = refined_fragment_polyline(*current,
				*seed_samples, retry_tolerance, &complete_pullback,
				&complete_failure, true, &complete_measured_error);
			    if (!complete_valid && step->Verbose())
				std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
				    << ((*current)->edge ?
					(*current)->edge->m_edge_user.i : -1)
				    << " complete-edge retry " << retry + 1
				    << " at tolerance " << retry_tolerance
				    << " rejected after measuring "
				    << complete_measured_error << ": "
				    << complete_failure << std::endl;
			    if (retry_tolerance >= adjustment_limit)
				break;
			}
			if (complete_valid && complete_measured_error >
				current_tolerance) {
			    const double measured_tolerance = std::min(
				adjustment_limit, complete_measured_error *
				    kMeasuredToleranceSafetyFactor);
			    if (measured_tolerance >= complete_measured_error) {
				(*current)->tolerance = std::max(
				    (*current)->tolerance, measured_tolerance);
				(*current)->flatness = std::max(
				    (*current)->flatness, measured_tolerance);
				(*current)->tolerance_adjusted = true;
				(*current)->declared_tolerance =
				    LocalUnits::tolerance;
			    }
			}
		    }
		}
		if (complete_valid) {
		    const ON_PolylineCurve *polyline =
			ON_PolylineCurve::Cast(complete_pullback);
		    if (polyline && polyline->m_pline.Count() >= 2) {
			ON_2dPointArray *complete_samples = new ON_2dPointArray();
			complete_samples->Reserve(polyline->m_pline.Count());
			for (int point = 0; point < polyline->m_pline.Count(); ++point)
			    complete_samples->Append(ON_2dPoint(
				polyline->m_pline[point].x,
				polyline->m_pline[point].y));
			while (!(*current)->segments->empty()) {
			    delete (*current)->segments->front();
			    (*current)->segments->pop_front();
			}
			(*current)->segments->push_back(complete_samples);
			std::map<PBCData *, ON_Curve *>::iterator old_exact =
			    exact_pullbacks.find(*current);
			if (old_exact != exact_pullbacks.end()) {
			    delete old_exact->second;
			    exact_pullbacks.erase(old_exact);
			}
			exact_pullbacks[*current] = complete_pullback;
			rejoined_periodic_pullbacks.insert(*current);
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "reconstructed a fragmented periodic pcurve from the complete "
			    "exact STEP edge");
			continue;
		    }
		    delete complete_pullback;
		    complete_failure = "complete pullback was not a valid polyline";
		}
		if (step && step->Verbose() && !complete_failure.empty())
		    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			<< ((*current)->edge ? (*current)->edge->m_edge_user.i : -1)
			<< " complete-edge pullback reconstruction rejected: "
			<< complete_failure << std::endl;
	    }
	    if (!singular_split && (*current)->segments->size() > 1)
		split_periodic_pullbacks.insert(*current);
	    for (size_t split = 0;
		    split_at_native_seam && split < seam_splits; ++split)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "split a periodic pcurve at a proven surface singularity");
	}
    /* A real seam split may add reciprocal fragments inside one STEP edge,
     * but it must not open a join between two neighboring STEP edges which
     * was already exact.  Restore the unchanged-cardinality edge proposals
     * as one group when independent native-image choices do that; edges which
     * acquired explicit seam fragments retain their topology split. */
    if (!native_split_exact_joins.empty()) {
	bool native_proposal_valid = true;
	for (size_t current = 0; native_proposal_valid &&
		current < native_split_pullbacks.size(); ++current) {
	    if (!native_split_exact_joins[current])
		continue;
	    const size_t previous = current ? current - 1 :
		native_split_pullbacks.size() - 1;
	    const PBCData *previous_data =
		native_split_pullbacks[previous];
	    const PBCData *current_data =
		native_split_pullbacks[current];
	    if (!previous_data || !current_data ||
		    !previous_data->segments || !current_data->segments ||
		    previous_data->segments->empty() ||
		    current_data->segments->empty()) {
		native_proposal_valid = false;
		break;
	    }
	    const ON_2dPointArray *previous_segment =
		previous_data->segments->back();
	    const ON_2dPointArray *current_segment =
		current_data->segments->front();
	    native_proposal_valid = face_local_join_is_exact(
		(*previous_segment)[previous_segment->Count() - 1],
		(*current_segment)[0]);
	}
	if (!native_proposal_valid) {
	    std::map<PBCData *, std::vector<ON_2dPointArray> >
		pre_restore_segments;
	    for (std::map<PBCData *, std::vector<ON_2dPointArray> >::
		    const_iterator saved = native_split_snapshot.begin();
		    saved != native_split_snapshot.end(); ++saved) {
		if (!saved->first || !saved->first->segments ||
			!saved->first->curve || !saved->first->surf ||
			saved->first->segments->size() != saved->second.size())
		    continue;
		/* An exact UV join is not enough: OpenNURBS may extrapolate a
		 * closed profile outside its declared domain, so the pre-remap
		 * branch can be topologically closed while missing the immutable
		 * 3-D STEP edge.  Restore only a complete edge whose every saved
		 * sample still lies on that source edge. */
		const CurveDistanceEvaluator source_distance(
		    saved->first->curve);
		const double locus_tolerance = std::max(
		    LocalUnits::tolerance, saved->first->tolerance);
		bool source_valid = source_distance.IsValid();
		for (std::vector<ON_2dPointArray>::const_iterator segment =
			saved->second.begin(); source_valid &&
			segment != saved->second.end(); ++segment) {
		    for (int point = 0; source_valid &&
			    point < segment->Count(); ++point) {
			const ON_3dPoint lift = saved->first->surf->PointAt(
			    (*segment)[point].x, (*segment)[point].y);
			source_valid = lift.IsValid() &&
			    source_distance.DistanceTo(lift,
				locus_tolerance) <= locus_tolerance;
		    }
		}
		if (!source_valid)
		    continue;
		std::vector<ON_2dPointArray> &current_saved =
		    pre_restore_segments[saved->first];
		std::list<ON_2dPointArray *>::iterator destination =
		    saved->first->segments->begin();
		for (std::vector<ON_2dPointArray>::const_iterator source =
			saved->second.begin(); source != saved->second.end();
			++source, ++destination) {
		    if (*destination) {
			current_saved.push_back(**destination);
			**destination = *source;
		    }
		}
	    }
	    bool restored_loop_valid = !pre_restore_segments.empty();
	    for (size_t current = 0; restored_loop_valid &&
		    current < native_split_pullbacks.size(); ++current) {
		if (!native_split_exact_joins[current])
		    continue;
		const size_t previous = current ? current - 1 :
		    native_split_pullbacks.size() - 1;
		const PBCData *previous_data =
		    native_split_pullbacks[previous];
		const PBCData *current_data =
		    native_split_pullbacks[current];
		const ON_2dPointArray *previous_segment =
		    previous_data && previous_data->segments &&
			!previous_data->segments->empty() ?
			previous_data->segments->back() : NULL;
		const ON_2dPointArray *current_segment =
		    current_data && current_data->segments &&
			!current_data->segments->empty() ?
			current_data->segments->front() : NULL;
		restored_loop_valid = previous_segment && current_segment &&
		    face_local_join_is_exact(
			(*previous_segment)[previous_segment->Count() - 1],
			(*current_segment)[0]);
	    }
	    if (!restored_loop_valid) {
		for (std::map<PBCData *, std::vector<ON_2dPointArray> >::
			const_iterator saved = pre_restore_segments.begin();
			saved != pre_restore_segments.end(); ++saved) {
		    std::list<ON_2dPointArray *>::iterator destination =
			saved->first->segments->begin();
		    for (std::vector<ON_2dPointArray>::const_iterator source =
			    saved->second.begin(); source != saved->second.end();
			    ++source, ++destination)
			if (*destination)
			    **destination = *source;
		}
	    } else {
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "rolled back native-domain edge remaps which opened an exact cyclic join");
		if (step->Verbose())
		    std::cerr << "EDGE_LOOP #" << id
			<< ": rolled back source-locus-validated native-domain edge "
			   "remaps because they opened an exact cyclic UV join"
			<< std::endl;
	    }
	}
    }
    }
    log_fragmented_pullbacks("after native seam split", curve_pullback_samples,
	false);
    log_pullback_endpoint_errors("after native seam split",
	curve_pullback_samples);
    log_adjusted_endpoints("after endpoint repair", curve_pullback_samples);

    if (step && !step->ImportOptions().exact &&
	step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	for (std::list<PBCData *>::iterator current =
		curve_pullback_samples.begin(); current !=
		curve_pullback_samples.end(); ++current) {
	    if (!*current || !(*current)->segments ||
		    (*current)->segments->size() < 2)
		continue;
	    const double fragment_tolerance = std::max(LocalUnits::tolerance,
		(*current)->tolerance);
	    const size_t removed = remove_subtolerance_outer_split_fragments(
		*current, brep, fragment_tolerance);
	    for (size_t repair = 0; repair < removed; ++repair)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "removed a densely validated sub-tolerance outer split fragment");
	    if ((*current)->segments->size() == 1)
		split_periodic_pullbacks.erase(*current);
	}
    }

    /* A single STEP edge can require multiple continuous pcurve fragments
     * when it crosses a surface singularity.  Reserve all possible subedges
     * before creating any of them; ON_SimpleArray growth would otherwise
     * invalidate the PBCData edge pointers retained during this loop. */
    int possible_subedges = 0;
    std::map<PBCData *, int> source_edge_indices;
    std::map<PBCData *, bool> singular_topology_splits;
    std::map<PBCData *, bool> required_topology_splits;
    std::map<PBCData *, std::vector<double> > fragment_boundary_parameters;
    for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	    current != curve_pullback_samples.end(); ++current) {
	if (!*current || !(*current)->edge || !(*current)->segments)
	    continue;
	source_edge_indices[*current] = (*current)->edge->m_edge_index;
	const bool singular_split = pullback_requires_singular_topology_split(
	    *current, std::max(LocalUnits::tolerance, (*current)->tolerance));
	singular_topology_splits[*current] = singular_split;
	const bool required_split = singular_split ||
	    split_periodic_pullbacks.find(*current) !=
	    split_periodic_pullbacks.end();
	required_topology_splits[*current] = required_split;
	if (required_split && (*current)->segments->size() > 1) {
	    std::vector<double> &boundaries = fragment_boundary_parameters[*current];
	    boundaries.assign((*current)->segments->size() - 1, ON_UNSET_VALUE);
	    std::list<ON_2dPointArray *>::const_iterator left =
		(*current)->segments->begin();
	    std::list<ON_2dPointArray *>::const_iterator right = left;
	    ++right;
	    size_t boundary = 0;
	    for (; right != (*current)->segments->end();
		    ++left, ++right, ++boundary) {
		if (*left && *right)
		    (void)step_fragment_boundary_parameter(*current, **left,
			**right, std::max(LocalUnits::tolerance,
			    (*current)->tolerance), &boundaries[boundary]);
	    }
	}
	if (required_split && step && step->Verbose()) {
	    const CurveDistanceEvaluator source_distance((*current)->curve);
	    size_t fragment_index = 0;
	    for (std::list<ON_2dPointArray *>::const_iterator fragment =
		    (*current)->segments->begin(); fragment !=
		    (*current)->segments->end(); ++fragment, ++fragment_index) {
		if (!*fragment || (*fragment)->Count() == 0)
		    continue;
		const ON_2dPoint start = (**fragment)[0];
		const ON_2dPoint end = (**fragment)[(*fragment)->Count() - 1];
		const ON_3dPoint start_lift = (*current)->surf->PointAt(start.x,
		    start.y);
		const ON_3dPoint end_lift = (*current)->surf->PointAt(end.x,
		    end.y);
		double start_parameter = 0.0;
		double end_parameter = 0.0;
		double start_distance = DBL_MAX;
		double end_distance = DBL_MAX;
		(void)source_distance.ClosestParameter(start_lift,
		    &start_parameter, &start_distance);
		(void)source_distance.ClosestParameter(end_lift,
		    &end_parameter, &end_distance);
		std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
		    << ((*current)->edge ? (*current)->edge->m_edge_user.i : -1)
		    << " required topology fragment " << fragment_index
		    << " uv=" << start.x << ':' << start.y << "->"
		    << end.x << ':' << end.y << " source="
		    << start_parameter << ':' << end_parameter << " error="
		    << start_distance << ':' << end_distance << std::endl;
	    }
	}
	if (required_split)
	    possible_subedges += static_cast<int>((*current)->segments->size());
    }
    const bool surface_has_pole = surface &&
	(surface->IsSingular(0) || surface->IsSingular(1) ||
	 surface->IsSingular(2) || surface->IsSingular(3));
    const int possible_pole_cuts = surface_has_pole ?
	static_cast<int>(curve_pullback_samples.size()) : 0;
    if (possible_subedges > 0 || possible_pole_cuts > 0) {
	brep->m_E.Reserve(brep->m_E.Count() + possible_subedges +
	    possible_pole_cuts);
	for (std::map<PBCData *, int>::const_iterator source =
		source_edge_indices.begin(); source != source_edge_indices.end(); ++source) {
	    if (source->second >= 0 && source->second < brep->m_E.Count())
		source->first->edge = &brep->m_E[source->second];
	}
    }
#ifdef _DEBUG_TESTING_
    //TODO: remove debugging
    if (_debug_print_) {
	std::cerr << "Face " << face->m_face_index << " id " << id << std::endl;
	print_pullback_data("After check_pullback_data", curve_pullback_samples, false);
    }
#endif
    list<PBCData *>::iterator cs = curve_pullback_samples.begin();
    list<PBCData *>::iterator next_cs;
    bool trim_construction_failed = false;

    cs = curve_pullback_samples.begin();
    while (cs != curve_pullback_samples.end()) {
	if (brlcad::PullbackWorkCancelled()) {
	    trim_construction_failed = true;
	    break;
	}
	next_cs = cs;
	next_cs++;
	if (next_cs == curve_pullback_samples.end()) {
	    next_cs = curve_pullback_samples.begin();
	}
	data = (*cs);
	const bool split_at_singularity = singular_topology_splits[data];
	const bool split_at_topology_discontinuity =
	    required_topology_splits[data];
	list<ON_2dPointArray *>::iterator si;
	si = data->segments->begin();
	size_t fragment_index = 0;
	PBCData *ndata = (*next_cs);
	list<ON_2dPointArray *>::iterator nsi;
	nsi = ndata->segments->begin();
	ON_2dPointArray *nsamples = NULL;

	while (si != data->segments->end()) {
	    if (brlcad::PullbackWorkCancelled()) {
		trim_construction_failed = true;
		break;
	    }
	    nsi = si;
	    nsi++;
	    if (nsi == data->segments->end()) {
		PBCData *nsidata = (*next_cs);
		nsi = nsidata->segments->begin();
	    }
	    ON_2dPointArray *samples = (*si);
	    nsamples = (*nsi);

	    int trimCurve = brep->m_C2.Count();
	    //TODO: remove debugging code
	    if ((false) && (trimCurve == 68)) {
		std::cerr << "Debug:LoadONTrimmingCurves for Path:" << trimCurve << std::endl;
	    }
	    ON_Curve *c2d = NULL;
	    bool used_polyline_fallback = false;
	    /* Periodic recovery may deliberately sample a collapsed UV branch at
	     * 1024 bounded intervals.  Those samples carry useful projection proof,
	     * but a chain of numerically identical points is still one effective
	     * pcurve point and ON_PolylineCurve correctly rejects it.  Compact only
	     * consecutive zero-distance samples here; no nonzero UV locus is changed,
	     * and an effectively one-point chain then reaches the exact endpoint/
	     * adjacent-trim recovery below instead of being mislabeled as a failed
	     * 1025-point curve fit. */
	    if (samples && samples->Count() > 1) {
		ON_2dPointArray compacted;
		compacted.Reserve(samples->Count());
		for (int sample = 0; sample < samples->Count(); ++sample) {
		    if (compacted.Count() > 0 &&
			    (*samples)[sample].DistanceTo(
				compacted[compacted.Count() - 1]) <=
			    ON_ZERO_TOLERANCE)
			continue;
		    compacted.Append((*samples)[sample]);
		}
		if (compacted.Count() < samples->Count()) {
		    /* A full-period closed edge can have pointwise-equivalent UV
		     * samples even though its exact pcurve spans a complete surface
		     * period.  Give the exact isoparametric reconstruction its original
		     * dense proof before classifying the chain as truly collapsed. */
		    if (compacted.Count() < 2 && data && data->edge &&
			data->edge->m_vi[0] == data->edge->m_vi[1] && step &&
			step->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe) {
			std::string iso_line_failure;
			c2d = closed_edge_iso_line(data, *samples,
			    maximum_authorized_curve_surface_tolerance(data,
				item_scale), &iso_line_failure);
			if (c2d)
			    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
				"regenerated closed-edge boundary pcurve within its authorized tolerance");
			else if (step->Verbose() && !iso_line_failure.empty())
			    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
				<< data->edge->m_edge_user.i
				<< " pre-compaction isoparametric reconstruction rejected: "
				<< iso_line_failure << std::endl;
		    }
		    if (!c2d)
			*samples = compacted;
		}
	    }
	    /* Compaction may reduce a full-period analytic seam to one effective
	     * UV point because its two endpoints are equivalent on the surface.
	     * Prefer the already constructed exact pcurve when its complete locus
	     * still lifts to the authoritative STEP edge.  This proof is stronger
	     * than comparing its Euclidean UV endpoints with the collapsed sample,
	     * which would incorrectly reject opposite copies of a periodic seam. */
	    if (samples && samples->Count() < 2 && data && data->curve &&
		data->surf && data->segments->size() == 1) {
		std::map<PBCData *, ON_Curve *>::iterator exact =
		    exact_pullbacks.find(data);
		ON_Curve *candidate = exact != exact_pullbacks.end() ?
		    exact->second : NULL;
		bool valid_exact = candidate && candidate->Dimension() == 2 &&
		    candidate->IsValid();
		const ON_Interval candidate_domain = valid_exact ?
		    candidate->Domain() : ON_Interval::EmptyInterval;
		const ON_Interval edge_domain = data->curve->Domain();
		const double exact_tolerance = std::max(LocalUnits::tolerance,
		    std::max(data->tolerance,
			data->edge ? data->edge->m_tolerance : 0.0));
		double maximum_exact_error = 0.0;
		int rejected_exact_sample = -1;
		for (int sample = 0; valid_exact &&
			sample <= kDenseLiftValidationSegments; ++sample) {
		    if (brlcad::PullbackWorkCancelled()) {
			valid_exact = false;
			break;
		    }
		    const double fraction = static_cast<double>(sample) /
			kDenseLiftValidationSegments;
		    const ON_3dPoint uv = candidate->PointAt(
			candidate_domain.ParameterAt(fraction));
		    const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
		    const ON_3dPoint target = data->curve->PointAt(
			edge_domain.ParameterAt(data->order_reversed ?
			    1.0 - fraction : fraction));
		    const double error = lifted.IsValid() && target.IsValid() ?
			lifted.DistanceTo(target) : DBL_MAX;
		    maximum_exact_error = std::max(maximum_exact_error, error);
		    valid_exact = uv.IsValid() && lifted.IsValid() &&
			target.IsValid() && error <= exact_tolerance;
		    if (!valid_exact) rejected_exact_sample = sample;
		}
		if (valid_exact) {
		    c2d = candidate;
		    exact->second = NULL;
		    if (step)
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "restored an exact periodic pcurve after sample compaction");
		} else if (step && step->Verbose()) {
		    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			<< (data->edge ? data->edge->m_edge_user.i : -1)
			<< " compacted exact pcurve "
			<< (candidate ? "rejected" : "was unavailable")
			<< " at sample " << rejected_exact_sample
			<< " with maximum lift error " << maximum_exact_error
			<< " and tolerance " << exact_tolerance
			<< " rejoined=" << (rejoined_periodic_pullbacks.find(data) !=
			    rejoined_periodic_pullbacks.end() ? 1 : 0) << std::endl;
		}
	    }
	    /* A short exact boundary can collapse to one pullback sample when its
	     * length is below the asserted model uncertainty.  Do not silently
	     * omit that topology edge.  Leave c2d unset so the bounded adjacent-
	     * endpoint reconstruction below can prove and restore the pcurve. */
	    if (samples && samples->Count() >= 2) {
	    std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.find(data);
	    if (exact != exact_pullbacks.end() && data->segments->size() == 1 &&
		exact->second && rejoined_periodic_pullbacks.find(data) ==
		    rejoined_periodic_pullbacks.end()) {
		/* Periodic fragment merging and native-domain normalization change
		 * the UV branch represented by the samples.  A curve cached before
		 * that operation is stale even when rounding makes its two endpoints
		 * coincide with the new chain; rejoined proposals are regenerated and
		 * interval-validated below. */
		const ON_Interval exact_domain = exact->second->Domain();
	const ON_3dPoint exact_start = exact->second->PointAt(exact_domain.Min());
	const ON_3dPoint exact_end = exact->second->PointAt(exact_domain.Max());
	if (ON_2dPoint(exact_start.x, exact_start.y).DistanceTo((*samples)[0]) <=
			ON_ZERO_TOLERANCE &&
		    ON_2dPoint(exact_end.x, exact_end.y).DistanceTo(
			(*samples)[samples->Count() - 1]) <= ON_ZERO_TOLERANCE) {
		    c2d = exact->second;
		    exact->second = NULL;
		}
	    }
	    bool regenerated = false;
	    if (!c2d && step && step->ImportOptions().repair ==
		    brlcad::step::RepairMode::Safe &&
		    rejoined_periodic_pullbacks.find(data) !=
			rejoined_periodic_pullbacks.end()) {
		std::string refinement_failure;
		double measured_refinement_error = 0.0;
		(void)refined_fragment_polyline(data, *samples,
		    std::max(LocalUnits::tolerance, data->tolerance), &c2d,
		    &refinement_failure, true, &measured_refinement_error);
		/* Fragment rejoining can expose a source edge/surface discrepancy at
		 * a chord midpoint which the adaptive source samples did not previously
		 * visit.  In non-exact safe mode, retry this one immutable pcurve
		 * proposal within the same feature- and item-scale ceiling used by the
		 * initial materialization audit.  The proposal records the largest
		 * vertex and interval error it actually measured; only that local value,
		 * not the search ceiling, is installed on the affected edge and trim. */
		if (!c2d && !step->ImportOptions().exact) {
		    bool endpoint_anchored_extension = false;
		    const double adjustment_limit = std::max(
			maximum_verified_curve_surface_tolerance(data->curve,
			    data->surf, LocalUnits::tolerance, item_scale,
			    data->context.get(),
			    &endpoint_anchored_extension),
			data->maximum_recovery_tolerance);
		    const double ordinary_adjustment_limit =
			maximum_authorized_edge_tolerance(data, item_scale);
		    if (adjustment_limit > std::max(LocalUnits::tolerance,
			    data->tolerance)) {
			ON_Curve *adjusted = NULL;
			std::string adjustment_failure;
			double adjusted_error = 0.0;
			(void)refined_fragment_polyline(data, *samples,
			    adjustment_limit, &adjusted, &adjustment_failure, true,
			    &adjusted_error);
			const double evidenced_tolerance = adjusted_error *
			    kMeasuredToleranceSafetyFactor;
			if (adjusted && adjusted_error > 0.0 &&
				evidenced_tolerance <= adjustment_limit) {
			    c2d = adjusted;
			    measured_refinement_error = adjusted_error;
			    data->tolerance = std::max(data->tolerance,
				evidenced_tolerance);
			    data->flatness = std::max(data->flatness,
				data->tolerance);
			    data->tolerance_adjusted =
				data->tolerance > LocalUnits::tolerance;
			    data->declared_tolerance = LocalUnits::tolerance;
			    if (data->edge) {
				ON_BrepEdge *adjusted_edge =
				    const_cast<ON_BrepEdge *>(data->edge);
				adjusted_edge->m_tolerance = std::max(
				    adjusted_edge->m_tolerance, data->tolerance);
			    }
			    std::ostringstream warning;
			    warning << "source edge/surface geometry exceeded the declared "
				"tolerance during complete pcurve interval validation; "
				"used measured local tolerance " << data->tolerance
				<< " for STEP edge #"
				<< (data->edge ? data->edge->m_edge_user.i : -1);
			    step->RecordDiagnostic(
				brlcad::step::DiagnosticSeverity::Warning, id,
				"EDGE_LOOP", "edge_list", warning.str());
			    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
				"recorded complete pcurve interval mismatch on the affected edge");
			    if (endpoint_anchored_extension &&
				    evidenced_tolerance >
					ordinary_adjustment_limit)
				step->RecordRepair(id, "EDGE_LOOP", "edge_list",
				    "used an endpoint-anchored measured tolerance for interior source curve/surface drift");
			} else {
			    delete adjusted;
			    if (!adjustment_failure.empty())
				refinement_failure = adjustment_failure;
			}
		    }
		}
		if (c2d) {
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"regenerated a rejoined periodic pcurve from the complete exact "
			"STEP edge");
		} else if (step->Verbose() && !refinement_failure.empty()) {
		    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			<< (data->edge ? data->edge->m_edge_user.i : -1)
			<< " rejoined-pullback refinement rejected: "
			<< refinement_failure << std::endl;
		}
	    }
	    if (!c2d && !split_at_topology_discontinuity && step &&
		    step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
		std::string iso_line_failure;
		c2d = closed_edge_iso_line(data, *samples,
		    maximum_authorized_curve_surface_tolerance(data, item_scale),
		    &iso_line_failure);
		if (!c2d && step->Verbose() && !iso_line_failure.empty())
		    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			<< (data->edge ? data->edge->m_edge_user.i : -1)
			<< " closed-edge isoparametric reconstruction rejected: "
			<< iso_line_failure << std::endl;
		if (c2d) {
		    /* A closed 3-D edge can have coincident pullback endpoints even
		     * though its exact Euclidean UV representation spans one complete
		     * surface period.  closed_edge_iso_line has densely proved the full
		     * curve locus, so restore those authoritative endpoints in the sample
		     * chain as well.  Rejecting the curve merely because the seam resolver
		     * selected the same periodic image at both ends erases the entire
		     * toroidal boundary. */
		    const ON_Interval reconstructed_domain = c2d->Domain();
		    for (int sample = 0; sample < samples->Count(); ++sample) {
			const ON_3dPoint restored = c2d->PointAt(
			    reconstructed_domain.ParameterAt(
				static_cast<double>(sample) / (samples->Count() - 1)));
			(*samples)[sample].Set(restored.x, restored.y);
		    }
		}
		regenerated = c2d != NULL;
	    }
	    if (regenerated) {
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "regenerated closed-edge boundary pcurve within its authorized tolerance");
	    } else if (!c2d) {
		const bool closed_edge = data->edge &&
		    data->edge->m_vi[0] == data->edge->m_vi[1];
		if (split_at_topology_discontinuity) {
		    /* A native seam or singularity fragment represents a strict
		     * subinterval of the STEP edge.  Reconstruct that interval from
		     * densely projected exact edge samples before considering the
		     * ordinary whole-edge fallback, even when the source itself required
		     * a measured tolerance adjustment. */
		    std::string refinement_failure;
		    (void)refined_fragment_polyline(data, *samples,
			std::max(LocalUnits::tolerance, data->tolerance), &c2d,
			&refinement_failure, false, NULL, true);
		    if (!c2d && step && step->Verbose() &&
			    !refinement_failure.empty())
			std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			    << (data->edge ? data->edge->m_edge_user.i : -1)
			    << " fragment refinement rejected: "
			    << refinement_failure << std::endl;
		} else if (data->tolerance_adjusted) {
		    /* A smooth interpolant can overshoot between closest-point samples
		     * by far more than the measured source mismatch.  The BREP's 3-D
		     * edge remains the original exact curve; use the bounded UV sample
		     * path solely as its trim association and prove it densely below. */
		    ON_3dPointArray points;
		    points.Reserve(samples->Count());
		    for (int sample = 0; sample < samples->Count(); ++sample)
			points.Append(ON_3dPoint((*samples)[sample].x,
			    (*samples)[sample].y, 0.0));
		    ON_PolylineCurve *polyline = new ON_PolylineCurve(points);
		    if (polyline->ChangeDimension(2) && polyline->IsValid()) {
			c2d = polyline;
			used_polyline_fallback = true;
		    } else {
			delete polyline;
		    }
		} else if (closed_edge) {
		    /* The local cubic fitter extrapolates open-curve endpoint
		     * tangents.  On a closed 3-D edge whose UV endpoints lie on
		     * different sides of a periodic domain, that fit can overshoot
		     * the already validated pullback samples and reverse one endpoint
		     * tangent.  Preserve the adaptive, lift-bounded UV path directly. */
		    ON_3dPointArray points;
		    points.Reserve(samples->Count());
		    for (int sample = 0; sample < samples->Count(); ++sample)
			points.Append(ON_3dPoint((*samples)[sample].x,
			    (*samples)[sample].y, 0.0));
		    ON_PolylineCurve *polyline = new ON_PolylineCurve(points);
		    if (polyline->ChangeDimension(2) && polyline->IsValid())
			c2d = polyline;
		    else
			delete polyline;
		} else {
		    ON_2dPointArray interpolation_samples(*samples);
		    c2d = interpolateCurve(interpolation_samples);
		}
		if (!c2d) {
		    ON_3dPointArray points;
		    points.Reserve(samples->Count());
		    for (int sample = 0; sample < samples->Count(); ++sample) {
			const ON_3dPoint point((*samples)[sample].x,
			    (*samples)[sample].y, 0.0);
			if (points.Count() > 0 && point.DistanceTo(
				points[points.Count() - 1]) <= ON_ZERO_TOLERANCE) {
			    points[points.Count() - 1] = point;
			    continue;
			}
			points.Append(point);
		    }
		    ON_PolylineCurve *polyline = points.Count() >= 2 ?
			new ON_PolylineCurve(points) : NULL;
		    if (polyline && polyline->ChangeDimension(2) && polyline->IsValid()) {
			c2d = polyline;
			used_polyline_fallback = true;
		    } else {
			delete polyline;
		    }
		}
		/* A same-vertex closed STEP edge can be sampled only at its two
		 * equivalent endpoints and one interior witness.  Those samples prove
		 * the edge/surface association but may not themselves define a valid
		 * OpenNURBS polyline.  Reproject the complete,
		 * authoritative edge domain at the dense validation resolution rather
		 * than omitting the face.  The helper preserves the directed edge
		 * interval, follows a continuous periodic branch, and checks every
		 * generated point and UV chord against the immutable 3-D edge before
		 * returning a curve. */
		if (!c2d && closed_edge && !split_at_topology_discontinuity &&
			step && !step->ImportOptions().exact &&
			step->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe) {
		    std::string dense_failure;
		    (void)refined_fragment_polyline(data, *samples,
			maximum_authorized_curve_surface_tolerance(data,
			    item_scale), &c2d, &dense_failure, true);
		    if (c2d) {
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "densely reconstructed a complete closed pcurve from its exact STEP edge");
		    } else if (step->Verbose() && !dense_failure.empty()) {
			std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			    << (data->edge ? data->edge->m_edge_user.i : -1)
			    << " complete closed-edge reconstruction rejected: "
			    << dense_failure << std::endl;
		    }
		}
	    }
	    if (used_polyline_fallback && step)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "preserved validated pullback samples after curve fitting failed");
	    }
	    if (step && step->Verbose() && split_at_topology_discontinuity &&
		    samples && samples->Count() > 0) {
		const ON_3dPoint curve_start = c2d ? c2d->PointAtStart() :
		    ON_3dPoint::UnsetPoint;
		const ON_3dPoint curve_end = c2d ? c2d->PointAtEnd() :
		    ON_3dPoint::UnsetPoint;
		std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
		    << (data->edge ? data->edge->m_edge_user.i : -1)
		    << " topology fragment samples=" << samples->Count()
		    << " " << (*samples)[0].x << ':' << (*samples)[0].y
		    << "->" << (*samples)[samples->Count() - 1].x << ':'
		    << (*samples)[samples->Count() - 1].y
		    << " singular=" << (split_at_singularity ? 1 : 0)
		    << " rejoined=" << (rejoined_periodic_pullbacks.find(data) !=
			rejoined_periodic_pullbacks.end() ? 1 : 0)
		    << " curve=" << curve_start.x << ':' << curve_start.y
		    << "->" << curve_end.x << ':' << curve_end.y << std::endl;
	    }
	    if (!c2d && step && step->Verbose() && samples && samples->Count() >= 2) {
		ON_2dPoint minimum((*samples)[0]);
		ON_2dPoint maximum((*samples)[0]);
		double walk_length = 0.0;
		for (int sample = 1; sample < samples->Count(); ++sample) {
		    minimum.x = std::min(minimum.x, (*samples)[sample].x);
		    minimum.y = std::min(minimum.y, (*samples)[sample].y);
		    maximum.x = std::max(maximum.x, (*samples)[sample].x);
		    maximum.y = std::max(maximum.y, (*samples)[sample].y);
		    walk_length += (*samples)[sample - 1].DistanceTo((*samples)[sample]);
		}
		std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
		    << (data->edge ? data->edge->m_edge_user.i : -1)
		    << " validated UV path did not form a valid curve; samples="
		    << samples->Count() << " start=" << (*samples)[0].x << ':'
		    << (*samples)[0].y << " end="
		    << (*samples)[samples->Count() - 1].x << ':'
		    << (*samples)[samples->Count() - 1].y << " span="
		    << maximum.x - minimum.x << ':' << maximum.y - minimum.y
		    << " walk=" << walk_length << std::endl;
	    }
	    if (!c2d && samples && samples->Count() < 2 && step &&
		step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
		data->edge && data->curve) {
		/* The second STEP use of a collapsed seam is the same exact edge in
		 * reverse.  Reuse the first validated pcurve instead of asking a
		 * closest-point solver to distinguish endpoints whose separation is
		 * smaller than the model uncertainty.  A second fragment of the current
		 * PBCData is not another topology use and must never enter this path. */
		bool have_prior_edge_use = false;
		for (std::list<PBCData *>::const_iterator prior =
			curve_pullback_samples.begin(); prior != cs; ++prior) {
		    if (*prior && (*prior)->edge && data->edge &&
			    (*prior)->edge->m_edge_index == data->edge->m_edge_index) {
			have_prior_edge_use = true;
			break;
		    }
		}
		if (have_prior_edge_use) {
		for (int lti = 0; lti < loop->TrimCount() && !c2d; ++lti) {
		    const ON_BrepTrim *paired_trim = loop->Trim(lti);
		    if (!paired_trim || paired_trim->m_ei != data->edge->m_edge_index ||
			!paired_trim->TrimCurveOf())
			continue;
		    ON_Curve *candidate = paired_trim->DuplicateCurve();
		    if (!candidate)
			continue;
		    if (paired_trim->m_bRev3d != data->order_reversed &&
			!candidate->Reverse()) {
			delete candidate;
			continue;
		    }
		    const ON_Interval candidate_domain = candidate->Domain();
		    const ON_Interval edge_domain = data->curve->Domain();
		    bool candidate_valid = candidate->ChangeDimension(2) &&
			candidate->IsValid();
		    for (int sample = 0; candidate_valid &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			if (brlcad::PullbackWorkCancelled()) {
			    candidate_valid = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) /
			    kDenseLiftValidationSegments;
			const ON_3dPoint uv = candidate->PointAt(
			    candidate_domain.ParameterAt(fraction));
			const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
			const ON_3dPoint target = data->curve->PointAt(
			    edge_domain.ParameterAt(data->order_reversed ?
				1.0 - fraction : fraction));
			if (!lifted.IsValid() || !target.IsValid() ||
				lifted.DistanceTo(target) > LocalUnits::tolerance)
			    candidate_valid = false;
		    }
		    if (!candidate_valid) {
			delete candidate;
			continue;
		    }
		    c2d = candidate;
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"reused an exact paired collapsed seam pcurve");
		}
		}
	    }
	    if (!c2d && step && step->ImportOptions().repair ==
		    brlcad::step::RepairMode::Safe && nsamples &&
		    nsamples->Count() > 0 &&
		    data->curve && data->surf) {
		/* A collapsed edge may be the first member of its STEP loop.  In
		 * that case there is no previously emitted trim to seed the start,
		 * but its two declared vertices still provide authoritative 3-D
		 * endpoints for the bounded closest-point reconstruction below. */
		const ON_BrepTrim *previous_trim = loop->TrimCount() > 0 ?
		    loop->Trim(loop->TrimCount() - 1) : NULL;
		ON_3dPoint start = previous_trim ? previous_trim->PointAtEnd() :
		    ON_3dPoint::UnsetPoint;
		if (!previous_trim && samples && samples->Count() > 0)
		    start.Set((*samples)[0].x, (*samples)[0].y, 0.0);
		const ON_3dPoint adjacent_start = start;
		ON_3dPoint end((*nsamples)[0].x, (*nsamples)[0].y, 0.0);
		const ON_3dPoint adjacent_end = end;
		const ON_Interval edge_domain = data->curve->Domain();
		/* Non-exact safe repair may already have densely measured a small,
		 * scale-bounded source edge/surface mismatch for this edge.  Reuse that
		 * proven tolerance here; falling back to the file uncertainty would make
		 * the one-sample recovery reject the same geometry a second time. */
		const double collapsed_acceptance_tolerance = std::max(
		    LocalUnits::tolerance, std::max(data->tolerance,
			data->edge ? data->edge->m_tolerance : 0.0));
		const bool collapsed_measured_repair_allowed =
		    !step->ImportOptions().exact &&
		    step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
		const double collapsed_repair_limit =
		    collapsed_measured_repair_allowed ?
		    maximum_authorized_edge_tolerance(data, item_scale) :
		    collapsed_acceptance_tolerance;
		if (!data->context)
		    data->context = std::make_shared<brlcad::PullbackContext>();
		const double pullback_tolerance = short_curve_pullback_resolution(
		    data->curve, LocalUnits::tolerance);
		ON_3dPoint target_start = data->curve->PointAt(edge_domain[
		    data->order_reversed ? 1 : 0]);
		ON_3dPoint target_end = data->curve->PointAt(edge_domain[
		    data->order_reversed ? 0 : 1]);
		/* Edge geometry can retain a wider proxy domain than a sub-tolerance
		 * topological use.  Its declared STEP vertices are authoritative for
		 * recovery of the lost pullback endpoint on every surface, not only a
		 * periodic one. */
		const int start_vertex = data->edge->m_vi[data->order_reversed ? 1 : 0];
		const int end_vertex = data->edge->m_vi[data->order_reversed ? 0 : 1];
		if (start_vertex >= 0 &&
			start_vertex < brep->m_V.Count())
		    target_start = brep->m_V[start_vertex].point;
		if (end_vertex >= 0 &&
			end_vertex < brep->m_V.Count())
		    target_end = brep->m_V[end_vertex].point;
		ON_2dPoint pulled_start, pulled_end;
		ON_3dPoint pulled_lift;
		double pulled_distance = DBL_MAX;
		/* Preserve an adjacent endpoint when its lift already represents the
		 * declared STEP vertex within model uncertainty.  This keeps the loop
		 * exactly closed in parameter space.  A periodic adjacent edge may begin
		 * at an arbitrary parameter on its 3-D circle, so solve the declared
		 * vertex when that bounded adjacency proof fails. */
		const ON_3dPoint adjacent_lift = data->surf->PointAt(
		    adjacent_start.x, adjacent_start.y);
		const bool adjacent_start_valid = previous_trim &&
		    adjacent_lift.IsValid() &&
		    adjacent_lift.DistanceTo(target_start) <=
			collapsed_acceptance_tolerance;
		ON_3dPoint seeded_start = start;
		if (!adjacent_start_valid && refine_surface_point_seeded(data->surf,
			target_start, pullback_tolerance, seeded_start,
			&pulled_distance)) {
		    start = seeded_start;
		} else if (!adjacent_start_valid &&
			data->context->SurfaceClosestPoint(data->surf, target_start,
			pulled_start, pulled_lift, pulled_distance, 0,
			pullback_tolerance, pullback_tolerance) &&
			pulled_distance <= pullback_tolerance) {
		    start.Set(pulled_start.x, pulled_start.y, 0.0);
		    for (int direction = 0; direction < 2; ++direction) {
			if (!data->surf->IsClosed(direction))
			    continue;
			const double period = data->surf->Domain(direction).Length();
			if (period > ON_ZERO_TOLERANCE)
			    start[direction] += round((adjacent_start[direction] -
				start[direction]) / period) * period;
		    }
		}
		ON_3dPoint seeded_end = start;
		if (refine_surface_point_seeded(data->surf, target_end,
			pullback_tolerance, seeded_end, &pulled_distance)) {
		    end = seeded_end;
		} else if (data->context->SurfaceClosestPoint(data->surf, target_end,
			pulled_end, pulled_lift, pulled_distance, 0,
			pullback_tolerance, pullback_tolerance) &&
			pulled_distance <= pullback_tolerance) {
		    end.Set(pulled_end.x, pulled_end.y, 0.0);
		}

		/* Try each closed parameter direction as the constant seam coordinate.
		 * The one-sample pullback has lost its varying endpoint, so recover that
		 * endpoint from the exact 3-D edge and accept a candidate only after a
		 * dense lift comparison with the complete edge. */
		int boundary_direction = -1;
		ON_LineCurve *boundary = NULL;
		bool exact_boundary = false;
		double maximum_boundary_error = 0.0;
		double rejected_boundary_fraction = 0.0;
		const auto validate_boundary = [data, &edge_domain, &target_start,
			&target_end](ON_LineCurve *candidate,
			double acceptance_tolerance, double *maximum_error,
			double *rejected_fraction) {
		    *maximum_error = 0.0;
		    *rejected_fraction = 0.0;
		    bool candidate_valid = candidate && candidate->ChangeDimension(2) &&
			candidate->IsValid();
		    const ON_Interval candidate_domain = candidate_valid ?
			candidate->Domain() : ON_Interval::EmptyInterval;
		    if (candidate_valid) {
			const ON_3dPoint start_uv = candidate->PointAt(candidate_domain.Min());
			const ON_3dPoint end_uv = candidate->PointAt(candidate_domain.Max());
			const ON_3dPoint start_lift = data->surf->PointAt(
			    start_uv.x, start_uv.y);
			const ON_3dPoint end_lift = data->surf->PointAt(end_uv.x, end_uv.y);
			/* pullback_tolerance is a deliberately tighter solver resolution
			 * used to distinguish the endpoints of a sub-tolerance feature.
			 * Acceptance uses either the source uncertainty or a previously
			 * recorded, densely verified safe-repair tolerance. */
			const double endpoint_tolerance = acceptance_tolerance;
			const double start_error = start_lift.IsValid() ?
			    start_lift.DistanceTo(target_start) : DBL_MAX;
			const double end_error = end_lift.IsValid() ?
			    end_lift.DistanceTo(target_end) : DBL_MAX;
			candidate_valid = start_lift.IsValid() && end_lift.IsValid() &&
			    start_error <= endpoint_tolerance &&
			    end_error <= endpoint_tolerance;
			*maximum_error = std::max(start_error, end_error);
		    }
		    for (int sample = 0; candidate_valid &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			if (brlcad::PullbackWorkCancelled()) {
			    candidate_valid = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) /
			    kDenseLiftValidationSegments;
			const ON_3dPoint uv = candidate->PointAt(
			    candidate_domain.ParameterAt(fraction));
			const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
			const double edge_fraction = data->order_reversed ?
			    1.0 - fraction : fraction;
			const ON_3dPoint target = data->curve->PointAt(
			    edge_domain.ParameterAt(edge_fraction));
			const double error = lifted.IsValid() && target.IsValid() ?
			    lifted.DistanceTo(target) : DBL_MAX;
			*maximum_error = std::max(*maximum_error, error);
			if (error > acceptance_tolerance) {
			    *rejected_fraction = fraction;
			    candidate_valid = false;
			}
		    }
		    return candidate_valid;
		};
		const auto validate_boundary_with_safe_adjustment =
		    [this, data, &validate_boundary,
			collapsed_acceptance_tolerance, collapsed_repair_limit](
			ON_LineCurve *candidate, double *maximum_error,
			double *rejected_fraction) {
		    if (validate_boundary(candidate, collapsed_acceptance_tolerance,
			    maximum_error, rejected_fraction))
			return true;
		    if (!(collapsed_repair_limit > collapsed_acceptance_tolerance) ||
			    !validate_boundary(candidate, collapsed_repair_limit,
				maximum_error, rejected_fraction))
			return false;
		    const double adjusted = std::max(collapsed_acceptance_tolerance,
			*maximum_error * kMeasuredToleranceSafetyFactor);
		    if (!(adjusted <= collapsed_repair_limit))
			return false;
		    data->tolerance = std::max(data->tolerance, adjusted);
		    data->flatness = std::max(data->flatness, adjusted);
		    data->tolerance_adjusted = true;
		    step->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Warning, id,
			"EDGE_LOOP", "edge_list",
			"source edge/surface separation exceeded the declared "
			"tolerance; used a densely measured tolerance for collapsed "
			"pcurve recovery");
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"adjusted one collapsed trim tolerance to measured source geometry");
		    return true;
		};
		/* First preserve both already-joined loop endpoints when their straight
		 * UV association densely follows the complete short STEP edge.  This is
		 * especially important for the last edge of a cyclic loop: replacing its
		 * end would otherwise require rebuilding the first trim after that trim
		 * has already been emitted.  The 3-D edge remains unchanged, and the
		 * declared model tolerance still bounds every accepted lift sample. */
		{
		    ON_LineCurve *candidate = new ON_LineCurve(adjacent_start,
			adjacent_end);
		    double candidate_maximum_error = 0.0;
		    double candidate_rejected_fraction = 0.0;
		    if (validate_boundary_with_safe_adjustment(candidate,
			    &candidate_maximum_error,
			    &candidate_rejected_fraction)) {
			boundary = candidate;
			start = adjacent_start;
			end = adjacent_end;
			exact_boundary = true;
			maximum_boundary_error = candidate_maximum_error;
		    } else {
			maximum_boundary_error = candidate_maximum_error;
			rejected_boundary_fraction = candidate_rejected_fraction;
			delete candidate;
		    }
		}
		/* Next try the direct endpoint association on every surface.  A
		 * sub-tolerance edge on a periodic surface is not necessarily a seam;
		 * it may vary in the closed direction while remaining an ordinary
		 * isoparametric trim.  Dense lift validation below is authoritative. */
		if (!exact_boundary) {
		    ON_LineCurve *candidate = new ON_LineCurve(start, end);
		    double candidate_maximum_error = 0.0;
		    double candidate_rejected_fraction = 0.0;
		    if (validate_boundary_with_safe_adjustment(candidate,
			    &candidate_maximum_error,
			    &candidate_rejected_fraction)) {
			boundary = candidate;
			exact_boundary = true;
			maximum_boundary_error = candidate_maximum_error;
		    } else {
			maximum_boundary_error = candidate_maximum_error;
			rejected_boundary_fraction = candidate_rejected_fraction;
			delete candidate;
		    }
		}
		for (int fixed_direction = 0; fixed_direction < 2 && !exact_boundary;
		     ++fixed_direction) {
		    if (!data->surf->IsClosed(fixed_direction))
			continue;
		    ON_3dPoint candidate_end = end;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!data->surf->IsClosed(direction))
			    continue;
			const double period = data->surf->Domain(direction).Length();
			if (period > ON_ZERO_TOLERANCE)
			    candidate_end[direction] += round((start[direction] -
				candidate_end[direction]) / period) * period;
		    }
		    candidate_end[fixed_direction] = start[fixed_direction];
		    ON_LineCurve *candidate = new ON_LineCurve(start, candidate_end);
		    double candidate_maximum_error = 0.0;
		    double candidate_rejected_fraction = 0.0;
		    const bool candidate_valid =
			validate_boundary_with_safe_adjustment(candidate,
			&candidate_maximum_error, &candidate_rejected_fraction);
		    if (candidate_valid) {
			boundary_direction = fixed_direction;
			boundary = candidate;
			end = candidate_end;
			exact_boundary = true;
			maximum_boundary_error = candidate_maximum_error;
		    } else {
			maximum_boundary_error = std::max(maximum_boundary_error,
			    candidate_maximum_error);
			rejected_boundary_fraction = candidate_rejected_fraction;
			delete candidate;
		    }
		}
		if (exact_boundary) {
		    const ON_2dPoint original_next_start = (*nsamples)[0];
		    const bool preserves_next_start = end.DistanceTo(
			original_next_start) <= ON_ZERO_TOLERANCE;
		    if (!preserves_next_start)
			(*nsamples)[0].Set(end.x, end.y);
		    bool next_curve_valid = preserves_next_start ||
			next_cs != curve_pullback_samples.begin();
		    if (!preserves_next_start &&
			next_cs == curve_pullback_samples.begin()) {
			const bool can_rebuild_next = initial_trim_count == 0 &&
			    loop->TrimCount() > 0 &&
			    ndata && ndata->curve && ndata->surf;
			ON_2dPointArray next_interpolation(*nsamples);
			ON_Curve *next_curve = can_rebuild_next ?
			    interpolateCurve(next_interpolation) : NULL;
			const auto make_next_polyline = [nsamples]() -> ON_Curve * {
			    ON_3dPointArray points;
			    for (int point = 0; point < nsamples->Count(); ++point) {
				const ON_3dPoint candidate((*nsamples)[point].x,
				    (*nsamples)[point].y, 0.0);
				if (points.Count() == 0 || candidate.DistanceTo(
					points[points.Count() - 1]) > ON_ZERO_TOLERANCE)
				    points.Append(candidate);
			    }
			    ON_PolylineCurve *polyline = points.Count() >= 2 ?
				new ON_PolylineCurve(points) : NULL;
			    if (polyline && polyline->ChangeDimension(2) && polyline->IsValid())
				return polyline;
			    delete polyline;
			    return NULL;
			};
			const auto validates_next = [ndata](ON_Curve *candidate) {
			    if (!candidate || !candidate->ChangeDimension(2) ||
				    !candidate->IsValid())
				return false;
			    const ON_Interval candidate_domain = candidate->Domain();
			    const ON_Interval next_edge_domain = ndata->curve->Domain();
			    for (int sample = 0;
				    sample <= kDenseLiftValidationSegments; ++sample) {
				if (brlcad::PullbackWorkCancelled()) return false;
				const double fraction = static_cast<double>(sample) /
				    kDenseLiftValidationSegments;
				const ON_3dPoint uv = candidate->PointAt(
				    candidate_domain.ParameterAt(fraction));
				const ON_3dPoint lifted = ndata->surf->PointAt(uv.x, uv.y);
				const ON_3dPoint target = ndata->curve->PointAt(
				    next_edge_domain.ParameterAt(ndata->order_reversed ?
					1.0 - fraction : fraction));
				const double next_tolerance = std::max(
				    LocalUnits::tolerance, std::max(ndata->tolerance,
					ndata->edge ? ndata->edge->m_tolerance : 0.0));
				if (!lifted.IsValid() || lifted.DistanceTo(target) >
					next_tolerance)
				    return false;
			    }
			    return true;
			};
			next_curve_valid = can_rebuild_next && validates_next(next_curve);
			if (!next_curve_valid && can_rebuild_next) {
			    delete next_curve;
			    next_curve = make_next_polyline();
			    next_curve_valid = validates_next(next_curve);
			}
			if (next_curve_valid) {
			    ON_BrepTrim *first_trim = loop->Trim(0);
			    const int next_c2 = brep->AddTrimCurve(next_curve);
			    if (!first_trim || next_c2 < 0 ||
				    !brep->SetTrimCurve(*first_trim, next_c2)) {
				if (next_c2 < 0)
				    delete next_curve;
				next_curve_valid = false;
			    } else {
				brep->SetTrimIsoFlags(*first_trim);
			    }
			} else {
			    delete next_curve;
			}
		    }
		    if (next_curve_valid) {
			c2d = boundary;
			const bool periodic_surface = data->surf->IsClosed(0) ||
			    data->surf->IsClosed(1);
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    periodic_surface ?
			    "regenerated a collapsed seam from exact adjacent endpoints" :
			    "regenerated a collapsed boundary pcurve within model tolerance");
		    } else {
			if (step->Verbose())
			    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
				<< (data->edge ? data->edge->m_edge_user.i : -1)
				<< " collapsed boundary " << start.x << ':' << start.y
				<< "->" << end.x << ':' << end.y
				<< " rejected because cyclic next STEP edge #"
				<< (ndata && ndata->edge ?
				    ndata->edge->m_edge_user.i : -1)
				<< " could not be rebuilt from adjusted start "
				<< original_next_start.x << ':'
				<< original_next_start.y << "->" << end.x << ':'
				<< end.y << std::endl;
			(*nsamples)[0] = original_next_start;
			delete boundary;
		    }
		} else {
		    if (step->Verbose())
			std::cerr << "EDGE_LOOP #" << id
			    << ": collapsed pcurve proof rejected for STEP edge "
			    << (data->edge ? data->edge->m_edge_user.i : -1)
			    << " boundary direction " << boundary_direction
			    << " uv " << start.x << ':' << start.y << " -> "
			    << end.x << ':' << end.y << " at normalized "
			    << rejected_boundary_fraction << " max error "
			    << maximum_boundary_error << " tolerance "
			    << collapsed_acceptance_tolerance << " target length "
			    << target_start.DistanceTo(target_end) << " endpoint errors "
			    << data->surf->PointAt(start.x, start.y).DistanceTo(target_start)
			    << '/' << data->surf->PointAt(end.x, end.y).DistanceTo(target_end)
			    << " surface closed "
			    << (data->surf->IsClosed(0) ? '1' : '0')
			    << (data->surf->IsClosed(1) ? '1' : '0') << " domains "
			    << data->surf->Domain(0).Min() << ':'
			    << data->surf->Domain(0).Max() << ','
			    << data->surf->Domain(1).Min() << ':'
			    << data->surf->Domain(1).Max() << std::endl;
		    delete boundary;
		}
	    }
	    /* Absent/collapsed-pcurve recovery above may construct an exact curve
	     * from adjacent endpoints even when the seam splitter supplied zero or
	     * one samples.  Reconstitute those endpoint samples for the subsequent
	     * loop-closure and singular-bridge checks. */
	    if (c2d && samples && samples->Count() < 2) {
		const ON_Interval recovered_domain = c2d->Domain();
		const ON_3dPoint recovered_start = c2d->PointAt(recovered_domain.Min());
		const ON_3dPoint recovered_end = c2d->PointAt(recovered_domain.Max());
		samples->Empty();
		samples->Append(ON_2dPoint(recovered_start.x, recovered_start.y));
		samples->Append(ON_2dPoint(recovered_end.x, recovered_end.y));
	    }
	    if (c2d && rejoined_periodic_pullbacks.find(data) !=
		    rejoined_periodic_pullbacks.end()) {
		/* The merge above proves its internal endpoint snap, and this final
		 * parameterization-independent locus check proves the curve fitter did
		 * not overshoot between the retained exact samples. */
		const CurveDistanceEvaluator source_distance(data->curve);
		const double acceptance_tolerance = std::max(LocalUnits::tolerance,
		    data->tolerance);
		double maximum_lift_error = 0.0;
		const auto validate_rejoined_locus = [&](ON_Curve *candidate) {
		    maximum_lift_error = 0.0;
		    const ON_Interval pcurve_domain = candidate ?
			candidate->Domain() : ON_Interval::EmptyInterval;
		    bool evaluable = candidate && source_distance.IsValid() &&
			pcurve_domain.IsIncreasing();
		    bool within_tolerance = evaluable;
		    for (int sample = 0; evaluable &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			if (brlcad::PullbackWorkCancelled()) {
			    evaluable = false;
			    data->failure_reason = PullbackFailureReason::Cancelled;
			    break;
			}
			const double fraction = static_cast<double>(sample) /
			    kDenseLiftValidationSegments;
			const ON_3dPoint uv = candidate->PointAt(
			    pcurve_domain.ParameterAt(fraction));
			const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
			const double error = uv.IsValid() && lifted.IsValid() ?
			    source_distance.DistanceTo(lifted,
				acceptance_tolerance) : DBL_MAX;
			maximum_lift_error = std::max(maximum_lift_error, error);
			if (error > acceptance_tolerance)
			    within_tolerance = false;
		    }
		    return evaluable && within_tolerance;
		};
		bool lift_valid = validate_rejoined_locus(c2d);
		double best_lift_error = maximum_lift_error;
		if (!lift_valid && data->failure_reason !=
			PullbackFailureReason::Cancelled && samples &&
			samples->Count() >= 2) {
		    ON_Curve *refined = NULL;
		    std::string refinement_failure;
		    (void)refined_fragment_polyline(data, *samples,
			acceptance_tolerance, &refined, &refinement_failure, true);
		    const bool refined_valid = refined &&
			validate_rejoined_locus(refined);
		    const double refined_lift_error = maximum_lift_error;
		    if (refined_valid) {
			delete c2d;
			c2d = refined;
			lift_valid = true;
			best_lift_error = refined_lift_error;
			if (step)
			    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
				"refined a rejoined periodic pullback to preserve its exact edge locus");
		    } else {
			if (refined && refined_lift_error < best_lift_error) {
			    delete c2d;
			    c2d = refined;
			    best_lift_error = refined_lift_error;
			} else {
			    delete refined;
			}
			if (step && step->Verbose() && !refinement_failure.empty())
			    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
				<< (data->edge ? data->edge->m_edge_user.i : -1)
				<< " rejoined-pullback refinement rejected: "
				<< refinement_failure << std::endl;
		    }
		}
		if (!lift_valid && data->failure_reason !=
			PullbackFailureReason::Cancelled && samples &&
			samples->Count() >= 2) {
		    ON_3dPointArray points;
		    points.Reserve(samples->Count());
		    for (int sample = 0; sample < samples->Count(); ++sample) {
			const ON_3dPoint point((*samples)[sample].x,
			    (*samples)[sample].y, 0.0);
			if (points.Count() == 0 || point.DistanceTo(
				points[points.Count() - 1]) > ON_ZERO_TOLERANCE)
			    points.Append(point);
		    }
		    ON_PolylineCurve *polyline = points.Count() >= 2 ?
			new ON_PolylineCurve(points) : NULL;
		    const bool polyline_valid = polyline &&
			polyline->ChangeDimension(2) && polyline->IsValid() &&
			validate_rejoined_locus(polyline);
		    const double polyline_lift_error = maximum_lift_error;
		    if (polyline_valid) {
			delete c2d;
			c2d = polyline;
			lift_valid = true;
			best_lift_error = polyline_lift_error;
			if (step)
			    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
				"preserved a rejoined periodic pullback after curve fitting overshot its exact edge");
		    } else {
			if (polyline && polyline_lift_error < best_lift_error) {
			    delete c2d;
			    c2d = polyline;
			    best_lift_error = polyline_lift_error;
			} else {
			    delete polyline;
			}
		    }
		}
		maximum_lift_error = best_lift_error;
		/* If the source edge is consistently farther from the surface than the
		 * declared uncertainty, retain the least-error validated UV locus only
		 * after a complete dense measurement proves a small, scale-bounded local
		 * tolerance.  A curve-fitting overshoot is never used as that evidence:
		 * the best measured candidate above wins before this decision. */
		if (!lift_valid && c2d && step && !step->ImportOptions().exact &&
			step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
			data->failure_reason != PullbackFailureReason::Cancelled &&
			maximum_lift_error < DBL_MAX) {
		    const double adjustment_limit =
			maximum_authorized_curve_surface_tolerance(data,
			    item_scale);
		    const double adjusted_tolerance = maximum_lift_error *
			kMeasuredToleranceSafetyFactor;
		    if (adjusted_tolerance > acceptance_tolerance &&
			    adjusted_tolerance <= adjustment_limit) {
			data->tolerance = adjusted_tolerance;
			data->flatness = std::max(data->flatness,
			    adjusted_tolerance);
			data->tolerance_adjusted = true;
			if (data->edge) {
			    ON_BrepEdge *adjusted_edge =
				const_cast<ON_BrepEdge *>(data->edge);
			    adjusted_edge->m_tolerance = std::max(
				adjusted_edge->m_tolerance, adjusted_tolerance);
			}
			lift_valid = true;
			std::ostringstream warning;
			warning << "source edge/surface separation exceeded the declared "
			    "tolerance; used densely measured tolerance "
			    << adjusted_tolerance << " for rejoined STEP edge #"
			    << (data->edge ? data->edge->m_edge_user.i : -1);
			step->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Warning, id,
			    "EDGE_LOOP", "edge_list", warning.str());
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "adjusted one rejoined trim tolerance to measured source geometry");
		    }
		}
		/* A rejoined curve whose source edge/surface mismatch was already
		 * established may still exceed that first measured tolerance at a
		 * narrow dense sample.  Do not discard it before the generic adjusted-
		 * locus pass below can measure the complete candidate and attempt its
		 * bounded adaptive refinement.  Unadjusted edges retain the immediate
		 * failure: curve-fitting error alone is not authority to loosen a trim. */
		if (!lift_valid && !data->tolerance_adjusted) {
		    delete c2d;
		    c2d = NULL;
		    if (data->failure_reason != PullbackFailureReason::Cancelled)
			data->failure_reason =
			    PullbackFailureReason::SurfaceDistanceExceeded;
		    data->maximum_projection_distance = maximum_lift_error;
		}
	    }
	    if (c2d && data->tolerance_adjusted) {
		double maximum_lift_error = 0.0;
		double maximum_error_fraction = 0.0;
		ON_3dPoint maximum_error_uv = ON_3dPoint::UnsetPoint;
		const double adjustment_limit =
		    maximum_authorized_curve_surface_tolerance(data,
			item_scale);
		const auto validate_locus = [&](ON_Curve *candidate) {
		    maximum_lift_error = 0.0;
		    maximum_error_fraction = 0.0;
		    maximum_error_uv = ON_3dPoint::UnsetPoint;
		    if (!candidate) return false;
		    const ON_Interval pcurve_domain = candidate->Domain();
		    CurveDistanceEvaluator edge_distance(data->curve);
		    bool valid = pcurve_domain.IsIncreasing() && edge_distance.IsValid();
		    for (int sample = 0; valid &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			if (brlcad::PullbackWorkCancelled()) {
			    valid = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) /
			    kDenseLiftValidationSegments;
			const ON_3dPoint uv = candidate->PointAt(
			    pcurve_domain.ParameterAt(fraction));
			const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
			/* A UV curve has its own parameterization.  Validate its 3-D
			 * geometric locus against the closest exact source-curve point. */
			const double error = lifted.IsValid() ?
			    edge_distance.DistanceTo(lifted, data->tolerance) : DBL_MAX;
			if (error > maximum_lift_error) {
			    maximum_lift_error = error;
			    maximum_error_fraction = fraction;
			    maximum_error_uv = uv;
			}
		    }
		    if (valid && maximum_lift_error > data->tolerance) {
			const double dense_tolerance = maximum_lift_error *
			    kMeasuredToleranceSafetyFactor;
			if (dense_tolerance <= adjustment_limit)
			    data->tolerance = dense_tolerance;
			else
			    valid = false;
		    }
		    return valid;
		};

		bool lift_valid = validate_locus(c2d);
	if (!lift_valid && !brlcad::PullbackWorkCancelled() &&
		used_polyline_fallback && samples && samples->Count() >= 2) {
		    ON_Curve *refined = NULL;
		    double measured_tolerance = 0.0;
		    if (refine_adjusted_pullback_polyline(data, *samples,
			    maximum_error_uv, adjustment_limit, &refined,
			    &measured_tolerance)) {
			delete c2d;
			c2d = refined;
			data->tolerance = std::max(data->tolerance,
			    measured_tolerance);
			lift_valid = validate_locus(c2d);
			if (lift_valid && step)
			    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
				"adaptively refined a measured-tolerance pcurve after dense locus validation");
		}
	}
	/* The existing pullback samples are adaptive in source-curve parameter,
	 * while ON_PolylineCurve evaluates each UV segment with a linear parameter.
	 * On a tightly curved periodic surface, a narrow lift-error peak can
	 * therefore occur away from a segment midpoint even when both samples and
	 * the midpoint refinement above are valid.  Reproject the complete source
	 * interval at the dense validation resolution before rejecting the edge.
	 * Keep the established UV endpoints so the already-proven loop closure is
	 * unchanged, and retain the same measured source-mismatch limit. */
	if (!lift_valid && !brlcad::PullbackWorkCancelled() && samples &&
		samples->Count() >= 2 && data->segments->size() == 1 &&
		!split_at_topology_discontinuity) {
	    ON_Curve *refined = NULL;
	    std::string refinement_failure;
	    if (refined_fragment_polyline(data, *samples, adjustment_limit,
		    &refined, &refinement_failure, true)) {
		delete c2d;
		c2d = refined;
		lift_valid = validate_locus(c2d);
		if (lift_valid && step)
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"densely reprojected an adjusted pcurve after curve-locus validation");
	    } else {
		delete refined;
		if (step && step->Verbose() && !refinement_failure.empty())
		    std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			<< (data->edge ? data->edge->m_edge_user.i : -1)
			<< " dense adjusted-pullback refinement rejected: "
			<< refinement_failure << std::endl;
	    }
	}
	/* Item-level topology repair can regenerate a provisional periodic trim
	 * directly from its exact 3-D STEP edge once the complete face/loop graph is
	 * available.  Exact mode necessarily carries such trims to that stage.
	 * Safe mode's additional adjusted-locus check must not reject the same
	 * repairable topology prematurely.  Defer only a narrowly bounded periodic
	 * failure (at most one declared tolerance beyond the scale-bounded repair
	 * ceiling); final OpenNURBS and exact-edge validation remain mandatory before
	 * the item can be written. */
	if (!lift_valid && !brlcad::PullbackWorkCancelled() && c2d &&
		(data->surf->IsClosed(0) || data->surf->IsClosed(1)) &&
		data->rejected_projection_samples == 0 &&
		maximum_lift_error <= adjustment_limit +
		    data->declared_tolerance) {
	    lift_valid = true;
	    if (step)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "deferred a bounded periodic pcurve to exact item-level topology repair");
	}
	if (!lift_valid) {
		    const bool cancelled = brlcad::PullbackWorkCancelled();
		    if (step && step->Verbose()) {
			std::cerr << "EDGE_LOOP #" << id << ": adjusted pcurve for STEP edge #"
			    << (data->edge ? data->edge->m_edge_user.i : -1)
			    << " failed dense locus validation at " << maximum_error_fraction
			    << " uv=" << maximum_error_uv.x << ':' << maximum_error_uv.y
			    << " error=" << maximum_lift_error << " limit="
			    << adjustment_limit << " segments=" << data->segments->size()
			    << " samples=" << (samples ? samples->Count() : 0);
			if (samples && samples->Count() >= 2) {
			const int last = samples->Count() - 1;
			std::cerr << " sample-uvs=" << (*samples)[0].x << ':'
			    << (*samples)[0].y << ',' << (*samples)[1].x << ':'
			    << (*samples)[1].y;
			if (last > 1)
			    std::cerr << "," << (*samples)[last - 1].x << ':'
				<< (*samples)[last - 1].y;
			std::cerr << ',' << (*samples)[last].x << ':'
			    << (*samples)[last].y;
			}
			std::cerr << std::endl;
		    }
		    delete c2d;
		    c2d = NULL;
		    data->failure_reason = cancelled ? PullbackFailureReason::Cancelled :
			PullbackFailureReason::SurfaceDistanceExceeded;
		    if (!cancelled)
			data->maximum_projection_distance = maximum_lift_error;
		} else if (step) {
		    const int edge_id = data->edge ? data->edge->m_edge_user.i : -1;
		    std::ostringstream warning;
		    warning << "source edge geometry separation exceeds declared tolerance "
			<< data->declared_tolerance << "; using densely verified edge tolerance "
			<< data->tolerance << " for STEP edge #" << edge_id;
		    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
			id, "EDGE_LOOP", "edge_list", warning.str());
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"adjusted one OpenNURBS edge tolerance to measured source geometry");
		}
	    }
	    if (!c2d) {
		if (step) {
		    const int edge_id = data->edge ? data->edge->m_edge_user.i : -1;
		    std::ostringstream reason;
		    if (data->failure_reason == PullbackFailureReason::Cancelled) {
			reason << "the enclosing item's work budget expired while "
			    << "validating STEP edge #" << edge_id;
		    } else if (data->failure_reason == PullbackFailureReason::SurfaceDistanceExceeded) {
			if (data->rejected_projection_samples == 0) {
			    reason << "constructed pcurve lift deviated from STEP edge #"
				<< edge_id << " by "
				<< data->maximum_projection_distance
				<< " after dense curve-locus validation";
			} else {
			    reason << "source curve/surface separation "
				<< data->maximum_projection_distance
				<< " exceeds model tolerance " << LocalUnits::tolerance
				<< " for STEP edge #" << edge_id << " ("
				<< data->rejected_projection_samples << '/'
				<< data->projection_samples
				<< " projection samples rejected)";
			}
		    } else {
			reason << "could not construct an exact trim for STEP edge #"
			    << edge_id << " from "
			    << (samples ? samples->Count() : 0)
			    << " validated pullback samples ("
			    << data->failed_projection_samples
			    << " closest-point solver failures)";
		    }
		    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
			"EDGE_LOOP", "edge_list", reason.str());
		}
		if (step && step->Verbose())
		    std::cerr << "EDGE_LOOP #" << id
			<< ": could not construct trim for STEP edge "
			<< (data->edge ? data->edge->m_edge_user.i : -1)
			<< " from " << samples->Count() << " pullback samples"
			<< std::endl;
		trim_construction_failed = true;
		break;
	    }
	    trimCurve = brep->m_C2.Count();
	    brep->m_C2.Append(c2d);

	    ON_BrepEdge *trim_edge = const_cast<ON_BrepEdge *>(data->edge);
	    bool trim_reversed = data->order_reversed;
	    std::string split_failure;
	    list<ON_2dPointArray *>::iterator following_fragment = si;
	    ++following_fragment;
	    const bool first_fragment = si == data->segments->begin();
	    const bool last_fragment =
		following_fragment == data->segments->end();
	    double split_tolerance = data->tolerance;
	    const bool allow_split_tolerance_adjustment = step &&
		!step->ImportOptions().exact &&
		step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
	    const double split_tolerance_limit =
		maximum_authorized_edge_tolerance(data, item_scale);
	    const std::vector<double> &boundary_parameters =
		fragment_boundary_parameters[data];
	    const double start_parameter_hint = fragment_index > 0 &&
		fragment_index - 1 < boundary_parameters.size() ?
		boundary_parameters[fragment_index - 1] : ON_UNSET_VALUE;
	    const double end_parameter_hint = fragment_index <
		boundary_parameters.size() ? boundary_parameters[fragment_index] :
		ON_UNSET_VALUE;
	    const bool topology_split_created = split_at_topology_discontinuity &&
		split_pullback_segment_edge(brep, data, c2d, *samples,
		    &split_tolerance, allow_split_tolerance_adjustment,
		    split_tolerance_limit, first_fragment, last_fragment,
		    start_parameter_hint, end_parameter_hint, &trim_edge,
		    &trim_reversed, &split_failure);
	    if (split_at_topology_discontinuity && !topology_split_created) {
		if (step)
		    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
			id, "EDGE_LOOP", "edge_list",
			"could not split STEP edge #" +
			std::to_string(data->edge ? data->edge->m_edge_user.i : -1) +
			" into exact continuous pcurve fragments" +
			(split_failure.empty() ? std::string() :
			    ": " + split_failure));
		trim_construction_failed = true;
		break;
	    }
	    if (topology_split_created && split_tolerance > data->tolerance) {
		data->tolerance = split_tolerance;
		if (step) {
		    const int edge_id = data->edge ? data->edge->m_edge_user.i : -1;
		    std::ostringstream warning;
		    warning << "source edge geometry separation exceeds declared "
			"tolerance " << data->declared_tolerance
			<< "; using densely verified edge tolerance "
			<< data->tolerance << " for STEP edge #" << edge_id;
		    step->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Warning, id,
			"EDGE_LOOP", "edge_list", warning.str());
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"adjusted one OpenNURBS edge tolerance to measured source geometry");
		}
	    }
	    if (topology_split_created && step)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    split_at_singularity ?
		    "split an exact STEP edge at a surface parameter singularity" :
		    "split an exact STEP edge at a native periodic parameter discontinuity");
	    trim_edge->m_tolerance = std::max(trim_edge->m_tolerance, data->tolerance);
	    ON_BrepTrim &trim = brep->NewTrim(*trim_edge, trim_reversed,
		(ON_BrepLoop &) * loop, trimCurve);
	    trim.m_tolerance[0] = data->tolerance;
	    trim.m_tolerance[1] = data->tolerance;
	    ON_Interval PD = trim.ProxyCurveDomain();
	    trim.m_iso = surface->IsIsoparametric(*c2d, &PD);

	    // check for bridging trim, trims along singularities
	    // are implicitly expected
	    ON_2dPoint end_current, start_next;
	    /* Regeneration can move a pcurve endpoint by a few numerical parameter
	     * units while preserving its exact 3-D lift.  A following singular trim
	     * must start at the endpoint of the curve actually emitted to the BREP,
	     * not at the older adaptive sample, or OpenNURBS sees a disconnected 2-D
	     * trim chain even though both points lift to the same surface pole. */
	    const ON_3dPoint emitted_end = c2d->PointAtEnd();
	    end_current = ON_2dPoint(emitted_end.x, emitted_end.y);
	    /* The following seam fragment may be the absent pcurve that will be
	     * regenerated on its own iteration.  Defer this bridge decision until
	     * it has exact endpoints instead of indexing an empty sample array. */
	    start_next = (nsamples && nsamples->Count() > 0) ?
		(*nsamples)[0] : end_current;

	    if (end_current.DistanceTo(start_next) > LocalUnits::tolerance) {
		// endpoints don't connect
		int is;
		const ON_Surface *surf = data->surf;
		const bool selected_pole_cut = data->periodic_pole_cut_after &&
		    ndata && ndata->periodic_pole_cut_before;
		if (selected_pole_cut && step_insert_periodic_pole_cut(brep, *loop, surf, trim,
			end_current, start_next, LocalUnits::tolerance)) {
		    if (step)
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "inserted an exact paired seam cut to a surface pole");
		} else if ((is = check_pullback_singularity_bridge(surf, end_current, start_next)) >= 0) {
		    // insert trim
		    // insert singular trim along
		    // 0 = south, 1 = east, 2 = north, 3 = west
		    ON_Surface::ISO iso = ON_Surface::N_iso;
		    switch (is) {
			case 0:
			    //south
			    iso = ON_Surface::S_iso;
			    break;
			case 1:
			    //east
			    iso = ON_Surface::E_iso;
			    break;
			case 2:
			    //north
			    iso = ON_Surface::N_iso;
			    break;
			case 3:
			    //west
			    iso = ON_Surface::W_iso;
		    }

		    /* IsAtSingularity() uses a parameter-space proximity test.  Its
		     * two input endpoints can therefore be near the same pole without
		     * lying exactly on the collapsed surface side.  A singular
		     * OpenNURBS trim, however, must be an exact W/E/S/N isoparametric
		     * boundary.  Project only the fixed coordinate onto that side and
		     * accept the connector after both supplied endpoints and the full
		     * projected locus densely lift to the shared STEP vertex. */
		    const int fixed_direction = (is == 1 || is == 3) ? 0 : 1;
		    const int side = (is == 1 || is == 2) ? 1 : 0;
		    const ON_Interval fixed_domain = surf->Domain(fixed_direction);
		    ON_2dPoint singular_start = end_current;
		    ON_2dPoint singular_end = start_next;
		    singular_start[fixed_direction] = fixed_domain[side];
		    singular_end[fixed_direction] = fixed_domain[side];
		    std::unique_ptr<ON_LineCurve> sing_c2d(
			new ON_LineCurve(singular_start, singular_end));
		    const ON_Interval singular_domain = sing_c2d->Domain();
		    bool exact_singular = fixed_domain.IsIncreasing() &&
			sing_c2d->ChangeDimension(2) && sing_c2d->IsValid() &&
			surf->IsIsoparametric(*sing_c2d, &singular_domain) == iso;
		    const int vi = trim.m_vi[1];
		    double singular_tolerance = std::max(LocalUnits::tolerance,
			data ? data->tolerance : 0.0);
		    if (ndata)
			singular_tolerance = std::max(singular_tolerance,
			    ndata->tolerance);
		    singular_tolerance = std::max(singular_tolerance,
			std::max(trim.m_tolerance[0], trim.m_tolerance[1]));
		    if (trim.Edge())
			singular_tolerance = std::max(singular_tolerance,
			    trim.Edge()->m_tolerance);
		    exact_singular = exact_singular && vi >= 0 &&
			vi < brep->m_V.Count();
		    const ON_3dPoint vertex = exact_singular ? brep->m_V[vi].point :
			ON_3dPoint::UnsetPoint;
		    const ON_3dPoint supplied_lift[2] = {
			surf->PointAt(end_current.x, end_current.y),
			surf->PointAt(start_next.x, start_next.y)
		    };
		    exact_singular = exact_singular && supplied_lift[0].IsValid() &&
			supplied_lift[1].IsValid() &&
			supplied_lift[0].DistanceTo(vertex) <= singular_tolerance &&
			supplied_lift[1].DistanceTo(vertex) <= singular_tolerance;
		    for (int sample = 0; exact_singular &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			const ON_3dPoint uv = sing_c2d->PointAt(
			    singular_domain.ParameterAt(static_cast<double>(sample) /
				kDenseLiftValidationSegments));
			const ON_3dPoint lift = surf->PointAt(uv.x, uv.y);
			exact_singular = lift.IsValid() &&
			    lift.DistanceTo(vertex) <= singular_tolerance;
		    }
		    if (exact_singular) {
			trimCurve = brep->AddTrimCurve(sing_c2d.release());
			if (trimCurve >= 0) {
			    ON_BrepTrim &sing_trim = brep->NewSingularTrim(
				brep->m_V[vi], (ON_BrepLoop &) * loop, iso,
				trimCurve);
			    sing_trim.m_tolerance[0] = singular_tolerance;
			    sing_trim.m_tolerance[1] = singular_tolerance;
			    sing_trim.m_iso = iso;
			}
		    } else if (step && step->Verbose()) {
			std::cerr << "EDGE_LOOP #" << id
			    << ": declined a near-pole singular connector because its "
			       "exact boundary projection did not lift to the shared "
			       "STEP vertex within tolerance " << singular_tolerance
			    << std::endl;
		    }
		    //trim.Reverse();
		} /*else if ((is = check_pullback_seam_bridge(surf, end_current, start_next)) >= 0) {
		  // insert trim
		  // insert singular trim along
		  // 0 = south, 1 = east, 2 = north, 3 = west
		  ON_Surface::ISO iso;
		  switch (is) {
		  case 0:
		  //south
		  iso = ON_Surface::S_iso;
		  break;
		  case 1:
		  //east
		  iso = ON_Surface::E_iso;
		  break;
		  case 2:
		  //north
		  iso = ON_Surface::N_iso;
		  break;
		  case 3:
		  //west
		  iso = ON_Surface::W_iso;
		  }

		  ON_Curve* c2d = new ON_LineCurve(end_current, start_next);
		  trimCurve = brep->m_C2.Count();
		  brep->m_C2.Append(c2d);

		  int vi;
		  int vo;
		  if (data->order_reversed) {
		  vi = data->edge->m_vi[0];
		  vo = data->edge->m_vi[1];
		  } else {
		  vi = data->edge->m_vi[1];
		  vo = data->edge->m_vi[0];
		  }
		  #ifdef TREATASBOUNDARY
		  ON_BrepEdge& e = (ON_BrepEdge&)*data->edge;
		  ON_BrepTrim& trim = brep->NewTrim(e, false, (ON_BrepLoop&) *loop, trimCurve);

		  trim.m_type = ON_BrepTrim::boundary;
		  trim.m_tolerance[0] = LocalUnits::tolerance;
		  trim.m_tolerance[1] = LocalUnits::tolerance;
		  #else
		  ON_BrepTrim& trim = brep->NewSingularTrim(brep->m_V[vi], (ON_BrepLoop&) *loop, iso, trimCurve);

		  trim.m_tolerance[0] = LocalUnits::tolerance;
		  trim.m_tolerance[1] = LocalUnits::tolerance;
		  ON_Interval PD = trim.ProxyCurveDomain();
		  trim.m_iso = surf->IsIsoparametric(*brep->m_C2[trimCurve], &PD);
		  trim.m_iso = iso;
		  #endif
		  trim.IsValid(&tl);
		  }*/
	    }
	    si++;
	    ++fragment_index;
	}
    if (trim_construction_failed)
	    break;
	cs++;
    }

    if (step && step->Verbose() && loop->TrimCount() > 1) {
	for (int trim_offset = 0; trim_offset < loop->TrimCount(); ++trim_offset) {
	    const ON_BrepTrim *current_trim = loop->Trim(trim_offset);
	    const ON_BrepTrim *next_trim = loop->Trim((trim_offset + 1) % loop->TrimCount());
	    if (!current_trim || !next_trim)
		continue;
	    const ON_3dPoint current_end = current_trim->PointAtEnd();
	    const ON_3dPoint next_start = next_trim->PointAtStart();
	    if (current_end.DistanceTo(next_start) <= ON_ZERO_TOLERANCE)
		continue;
	    std::cerr << "EDGE_LOOP #" << id << ": trim " << current_trim->m_trim_index
		<< " (edge " << current_trim->m_ei << ", type "
		<< static_cast<int>(current_trim->m_type) << ", iso "
		<< static_cast<int>(current_trim->m_iso) << ", STEP "
		<< (current_trim->Edge() ? current_trim->Edge()->m_edge_user.i : 0)
		<< ") ends at ("
		<< current_end.x << ',' << current_end.y << ") but trim "
		<< next_trim->m_trim_index << " (edge " << next_trim->m_ei
		<< ", type " << static_cast<int>(next_trim->m_type) << ", iso "
		<< static_cast<int>(next_trim->m_iso) << ", STEP "
		<< (next_trim->Edge() ? next_trim->Edge()->m_edge_user.i : 0)
		<< ") starts at (" << next_start.x << ',' << next_start.y << ')';
	    const ON_Surface *loop_surface = loop->Face() ?
		loop->Face()->SurfaceOf() : NULL;
	    if (loop_surface)
		std::cerr << " surface domains " << loop_surface->Domain(0).Min()
		    << ':' << loop_surface->Domain(0).Max() << ','
		    << loop_surface->Domain(1).Min() << ':'
		    << loop_surface->Domain(1).Max() << " closed "
		    << (loop_surface->IsClosed(0) ? '1' : '0')
		    << (loop_surface->IsClosed(1) ? '1' : '0');
	    std::cerr << std::endl;
	}
    }

    record_pullback_context_statistics(step, curve_pullback_samples);
    while (!curve_pullback_samples.empty()) {
	data = curve_pullback_samples.front();
	while (!data->segments->empty()) {
	    delete data->segments->front();
	    data->segments->pop_front();
	}
	delete data->segments;
	delete data;
	curve_pullback_samples.pop_front();
    }
    for (std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.begin();
	 exact != exact_pullbacks.end(); ++exact)
	delete exact->second;

    if (loop->TrimCount() == initial_trim_count && step && step->Verbose())
	std::cerr << "EDGE_LOOP #" << id << ": trim construction produced no edge uses" << std::endl;

    return !trim_construction_failed;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
