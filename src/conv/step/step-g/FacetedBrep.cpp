/*               F A C E T E D B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "FacetedBrep.h"
#include "ClosedShell.h"
#include "Face.h"
#include "FaceBound.h"
#include "FaceOuterBound.h"
#include "PolyLoop.h"
#include "CartesianPoint.h"
#include "LocalUnits.h"
#include "OpenNurbsInterfaces.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

#define CLASSNAME "FacetedBrep"
#define ENTITYNAME "Faceted_Brep"

string FacetedBrep::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)FacetedBrep::Create);

FacetedBrep::FacetedBrep()
{
}

FacetedBrep::FacetedBrep(STEPWrapper *sw, int step_id) : ManifoldSolidBrep(sw, step_id)
{
}

FacetedBrep::~FacetedBrep()
{
}

bool
FacetedBrep::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    if (!ManifoldSolidBrep::Load(sw, sse)) {
	sw->entity_status[sse->STEPfile_id] = STEP_LOAD_ERROR;
	return false;
    }
    sw->entity_status[sse->STEPfile_id] = STEP_LOADED;
    return true;
}

bool
FacetedBrep::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !outer)
	return false;

    const LIST_OF_FACES &faces = outer->Faces();
    bool have_polygon_loop = false;
    bool have_other_loop = false;
    for (LIST_OF_FACES::const_iterator source_face = faces.begin();
	    source_face != faces.end(); ++source_face) {
	if (!*source_face) return false;
	const LIST_OF_FACE_BOUNDS &source_bounds = (*source_face)->Bounds();
	for (LIST_OF_FACE_BOUNDS::const_iterator source_bound =
		source_bounds.begin(); source_bound != source_bounds.end();
		++source_bound) {
	    if (*source_bound && dynamic_cast<PolyLoop *>((*source_bound)->GetBound()))
		have_polygon_loop = true;
	    else
		have_other_loop = true;
	}
    }
    /* AP214 faceted models may use ordinary EDGE_LOOP topology, which the
     * current validated manifold path already handles.  The specialized path
     * below is solely for classic AP203 POLY_LOOP topology. */
    if (!have_polygon_loop)
	return ManifoldSolidBrep::LoadONBrep(brep);
    if (have_other_loop)
	return false;

    std::map<std::pair<int, int>, int> synthetic_edges;
    int next_synthetic_edge = 1;
    for (LIST_OF_FACES::const_iterator source_face = faces.begin();
	    source_face != faces.end(); ++source_face) {
	if (!*source_face)
	    return false;
	const LIST_OF_FACE_BOUNDS &bounds = (*source_face)->Bounds();
	if (bounds.empty())
	    return false;

	struct PolygonBoundary {
	    FaceBound *bound = NULL;
	    std::vector<ON_3dPoint> points;
	    std::vector<int> source_ids;
	    bool outer = false;
	};
	std::vector<PolygonBoundary> polygons;
	polygons.reserve(bounds.size());
	size_t outer_index = 0;
	bool have_outer = false;
	for (LIST_OF_FACE_BOUNDS::const_iterator source_bound = bounds.begin();
		 source_bound != bounds.end(); ++source_bound) {
	    PolyLoop *loop = *source_bound ?
		dynamic_cast<PolyLoop *>((*source_bound)->GetBound()) : NULL;
	    if (!loop)
		return false;
	    PolygonBoundary polygon;
	    polygon.bound = *source_bound;
	    polygon.outer = dynamic_cast<FaceOuterBound *>(*source_bound) != NULL;
	    const LIST_OF_POINTS &source_points = loop->Polygon();
	    for (LIST_OF_POINTS::const_iterator source_point = source_points.begin();
		    source_point != source_points.end(); ++source_point) {
		if (!*source_point)
		    return false;
		polygon.points.push_back(ON_3dPoint(
		    (*source_point)->X() * LocalUnits::length,
		    (*source_point)->Y() * LocalUnits::length,
		    (*source_point)->Z() * LocalUnits::length));
		polygon.source_ids.push_back((*source_point)->STEPid());
	    }
	    if (polygon.points.size() > 3 &&
		    polygon.points.front().DistanceTo(polygon.points.back()) <=
			LocalUnits::tolerance)
	    {
		polygon.points.pop_back();
		polygon.source_ids.pop_back();
	    }
	    if (polygon.points.size() < 3)
		return false;
	    if (!polygon.bound->Oriented()) {
		std::reverse(polygon.points.begin(), polygon.points.end());
		std::reverse(polygon.source_ids.begin(), polygon.source_ids.end());
	    }
	    if (polygon.outer && !have_outer) {
		outer_index = polygons.size();
		have_outer = true;
	    }
	    polygons.push_back(polygon);
	}
	if (!have_outer)
	    polygons.front().outer = true;

	const std::vector<ON_3dPoint> &outer_points =
	    polygons[outer_index].points;
	size_t second = 1;
	while (second < outer_points.size() &&
		outer_points[second].DistanceTo(outer_points[0]) <=
		    LocalUnits::tolerance)
	    ++second;
	size_t third = second + 1;
	for (; third < outer_points.size(); ++third) {
	    const ON_3dVector first_axis = outer_points[second] - outer_points[0];
	    const ON_3dVector second_axis = outer_points[third] - outer_points[0];
	    if (ON_CrossProduct(first_axis, second_axis).Length() >
		    LocalUnits::tolerance * LocalUnits::tolerance)
		break;
	}
	if (second >= outer_points.size() || third >= outer_points.size())
	    return false;
	ON_Plane plane(outer_points[0], outer_points[second], outer_points[third]);
	if (!plane.IsValid())
	    return false;
	double umin = DBL_MAX, umax = -DBL_MAX;
	double vmin = DBL_MAX, vmax = -DBL_MAX;
	for (std::vector<PolygonBoundary>::const_iterator polygon = polygons.begin();
		polygon != polygons.end(); ++polygon) {
	    for (std::vector<ON_3dPoint>::const_iterator point =
		    polygon->points.begin(); point != polygon->points.end(); ++point) {
		if (std::fabs(plane.DistanceTo(*point)) >
			std::max(LocalUnits::tolerance, 1.0e-9))
		    return false;
		double u = 0.0, v = 0.0;
		if (!plane.ClosestPointTo(*point, &u, &v))
		    return false;
		umin = std::min(umin, u);
		umax = std::max(umax, u);
		vmin = std::min(vmin, v);
		vmax = std::max(vmax, v);
	    }
	}
	const double padding = std::max(LocalUnits::tolerance, 1.0e-9);
	ON_PlaneSurface *surface = new ON_PlaneSurface(plane);
	surface->SetDomain(0, umin - padding, umax + padding);
	surface->SetDomain(1, vmin - padding, vmax + padding);
	surface->SetExtents(0, surface->Domain(0));
	surface->SetExtents(1, surface->Domain(1));
	ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));
	face.m_face_user.i = (*source_face)->STEPid();

	std::vector<size_t> order;
	order.push_back(outer_index);
	for (size_t i = 0; i < polygons.size(); ++i)
	    if (i != outer_index) order.push_back(i);
	for (std::vector<size_t>::const_iterator index = order.begin();
		index != order.end(); ++index) {
	    PolygonBoundary &polygon = polygons[*index];
	    ON_SimpleArray<ON_Curve *> curves;
	    for (size_t i = 0; i < polygon.points.size(); ++i)
		curves.Append(new ON_LineCurve(polygon.points[i],
		    polygon.points[(i + 1) % polygon.points.size()]));
	    const bool created = brep->NewPlanarFaceLoop(face.m_face_index,
		polygon.outer ? ON_BrepLoop::outer : ON_BrepLoop::inner,
		curves, true);
	    for (int i = 0; i < curves.Count(); ++i)
		delete curves[i];
	    if (!created)
		return false;
	    ON_BrepLoop *created_loop = brep->m_L.Last();
	    if (!created_loop)
		return false;
	    created_loop->m_loop_user.i = polygon.bound->STEPid();
	    for (int use = 0; use < created_loop->TrimCount(); ++use) {
		ON_BrepTrim *trim = created_loop->Trim(use);
		ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (!edge)
		    return false;
		int endpoint_ids[2] = {0, 0};
		for (int endpoint = 0; endpoint < 2; ++endpoint) {
		    if (edge->m_vi[endpoint] < 0 ||
			    edge->m_vi[endpoint] >= brep->m_V.Count())
			return false;
		    ON_BrepVertex &vertex = brep->m_V[edge->m_vi[endpoint]];
		    for (size_t point = 0; point < polygon.points.size(); ++point) {
			if (vertex.point.DistanceTo(polygon.points[point]) <=
				LocalUnits::tolerance) {
			    endpoint_ids[endpoint] = polygon.source_ids[point];
			    break;
			}
		    }
		    if (endpoint_ids[endpoint] <= 0)
			return false;
		    vertex.m_vertex_user.i = endpoint_ids[endpoint];
		}
		std::pair<int, int> edge_key(std::min(endpoint_ids[0], endpoint_ids[1]),
		    std::max(endpoint_ids[0], endpoint_ids[1]));
		std::map<std::pair<int, int>, int>::iterator source_edge =
		    synthetic_edges.find(edge_key);
		if (source_edge == synthetic_edges.end())
		    source_edge = synthetic_edges.insert(std::make_pair(edge_key,
			    next_synthetic_edge++)).first;
		edge->m_edge_user.i = source_edge->second;
	    }
	}
	brep->SetTrimIsoFlags(face);
    }

    std::string stitch_failure;
    if (!step_stitch_face_breps(brep, LocalUnits::tolerance, &stitch_failure)) {
	if (step)
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"FACETED_BREP", "outer", "could not stitch polygon faces: " +
		stitch_failure);
	return false;
    }
    return brep->m_F.Count() == static_cast<int>(faces.size());
}

void
FacetedBrep::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":(" << STEPid() << ")" << std::endl;
    ManifoldSolidBrep::Print(level + 1);
}

STEPEntity *
FacetedBrep::GetInstance(STEPWrapper *sw, int id)
{
    return new FacetedBrep(sw, id);
}

STEPEntity *
FacetedBrep::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}
