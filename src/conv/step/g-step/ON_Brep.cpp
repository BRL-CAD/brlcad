/*                     O N _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
/** @file ON_Brep.cpp
 *
 * File for writing out an ON_Brep structure into the STEPcode containers
 *
 */


// Make entity arrays for each of the m_V, m_S, etc arrays and create
// step instances of them, starting with the basic ones.
//
// The array indices in the ON_Brep will correspond to the step entity
// array locations that hold the step version of each entity.
//
// then, need to map the ON_Brep hierarchy to the corresponding STEP
// hierarchy
//
// brep -> advanced_brep_shape_representation
//         manifold_solid_brep
// faces-> closed_shell
//         advanced_face
//
// surface-> bspline_surface_with_knots
//           cartesian_point
//
// outer 3d trim loop ->  face_outer_bound    ->    SdaiFace_outer_bound -> SdaiFace_bound
//                        edge_loop           ->    SdaiEdge_loop
//                        oriented_edge       ->    SdaiOriented_edge
//                        edge_curve          ->    SdaiEdge_curve
//                        bspline_curve_with_knots
//                        vertex_point
//                        cartesian_point
//
// 2d points -> point_on_surface
// 1d points to bound curve -> point_on_curve
//
// 2d trimming curves -> pcurve using point_on_surface? almost doesn't
// look as if there is a good AP203 way to represent 2d trimming
// curves...
//
//
// Note that STEPentity is the same thing as
// SDAI_Application_instance... see src/clstepcore/sdai.h line 220
//
#include "AP_Common.h"
#include "ON_Brep.h"
#include "STEPGeneratedAPI.h"
#include "Shape_Definition_Representation.h"
#include "Default_Geometric_Context.h"
#include "brep/boolean.h"

#include <algorithm>
#include <memory>
#include <queue>
#include <set>

namespace {

struct BrepShellPlan {
    bool solid = false;
    int outer_component = -1;
    std::vector<std::vector<int> > components;
    std::vector<bool> component_closed;
};

std::vector<std::vector<int> >
brep_face_components(const ON_Brep &brep)
{
    std::vector<std::set<int> > adjacency(brep.m_F.Count());
    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep.m_E[edge_index];
	std::vector<int> faces;
	for (int trim_index = 0; trim_index < edge.m_ti.Count(); ++trim_index) {
	    const int index = edge.m_ti[trim_index];
	    if (index < 0 || index >= brep.m_T.Count()) continue;
	    const int face_index = brep.m_T[index].FaceIndexOf();
	    if (face_index >= 0 && face_index < brep.m_F.Count() &&
		    std::find(faces.begin(), faces.end(), face_index) == faces.end())
		faces.push_back(face_index);
	}
	for (size_t first = 0; first < faces.size(); ++first) {
	    for (size_t second = first + 1; second < faces.size(); ++second) {
		adjacency[faces[first]].insert(faces[second]);
		adjacency[faces[second]].insert(faces[first]);
	    }
	}
    }

    std::vector<bool> visited(brep.m_F.Count(), false);
    std::vector<std::vector<int> > components;
    for (int seed = 0; seed < brep.m_F.Count(); ++seed) {
	if (visited[seed]) continue;
	components.push_back(std::vector<int>());
	std::queue<int> pending;
	pending.push(seed);
	visited[seed] = true;
	while (!pending.empty()) {
	    const int face = pending.front();
	    pending.pop();
	    components.back().push_back(face);
	    for (std::set<int>::const_iterator neighbor = adjacency[face].begin();
		    neighbor != adjacency[face].end(); ++neighbor) {
		if (visited[*neighbor]) continue;
		visited[*neighbor] = true;
		pending.push(*neighbor);
	    }
	}
    }
    return components;
}

bool
build_shell_plan(const ON_Brep &brep, BrepShellPlan &plan,
    std::string &diagnostic)
{
    ON_TextLog validation_log;
    if (!brep.IsValid(&validation_log)) {
	diagnostic = "OpenNURBS BRep is invalid and cannot be exported safely";
	return false;
    }
    plan.solid = brep.IsSolid();
    plan.components = brep_face_components(brep);
    if (plan.components.empty()) {
	diagnostic = "BRep has no connected face component";
	return false;
    }

    std::vector<std::unique_ptr<ON_Brep> > component_breps;
    component_breps.reserve(plan.components.size());
    plan.component_closed.reserve(plan.components.size());
    for (size_t component = 0; component < plan.components.size(); ++component) {
	ON_Brep *copy = brep.DuplicateFaces(
	    static_cast<int>(plan.components[component].size()),
	    plan.components[component].data(), false);
	if (!copy || !copy->IsValid()) {
	    delete copy;
	    diagnostic = "a connected BRep shell could not be isolated safely";
	    return false;
	}
	plan.component_closed.push_back(copy->IsSolid());
	component_breps.push_back(std::unique_ptr<ON_Brep>(copy));
    }
    if (!plan.solid) return true;
    if (plan.components.size() == 1) {
	plan.outer_component = 0;
	return true;
    }

    /* A STEP BREP_WITH_VOIDS has exactly one outer shell.  Prove that choice
     * geometrically instead of treating every disconnected solid component
     * as a cavity.  This also rejects a single BRL-CAD BRep containing two
     * independent volumes, for which one MANIFOLD_SOLID_BREP would be false. */
    std::vector<int> outer_candidates;
    for (size_t candidate = 0; candidate < component_breps.size(); ++candidate) {
	bool contains_all = plan.component_closed[candidate];
	const ON_BoundingBox candidate_bounds = component_breps[candidate]->BoundingBox();
	for (size_t other = 0; other < component_breps.size() && contains_all; ++other) {
	    if (other == candidate) continue;
	    if (!plan.component_closed[other] ||
		    component_breps[other]->m_V.Count() == 0) {
		contains_all = false;
		break;
	    }
	    if (!candidate_bounds.Includes(component_breps[other]->BoundingBox(), true) ||
		    !ON_BrepPointInside(component_breps[other]->m_V[0].Point(),
			component_breps[candidate].get()))
		contains_all = false;
	}
	if (contains_all) outer_candidates.push_back(static_cast<int>(candidate));
    }
    if (outer_candidates.size() != 1) {
	diagnostic = "multiple closed BRep components are not one provable outer shell with voids";
	return false;
    }
    plan.outer_component = outer_candidates.front();
    return true;
}

} // namespace

void
ON_3dPoint_to_Cartesian_point(ON_3dPoint *inpnt, STEPentity *step_pnt) {
    XYZ_to_Cartesian_point(inpnt->x, inpnt->y, inpnt->z, step_pnt);
}

void
ON_3dVector_to_Direction(ON_3dVector *invect, STEPentity *step_direction) {
    invect->Unitize();
    XYZ_to_Direction(invect->x, invect->y, invect->z, step_direction);
}

/* STEP needs explicit edges corresponding to what in OpenNURBS are
 * the UV space trimming curves
 */
void
Add_Edge(ON_BrepTrim *trim, STEPentity *edge_loop, ON_Brep_Info_AP203 *info)
{
    ON_BrepEdge *edge = trim->Edge();
    int i = -1;

    // Some trims don't have an associated edge - allow for that
    if (edge) {
	int ec_index = edge->EdgeCurveIndexOf();
	if (ec_index < (int)info->edge_curves.size()) {
	    STEPentity *new_oriented_edge = info->registry->ObjCreate("ORIENTED_EDGE");
	    brlcad::step::SetString(new_oriented_edge, "name", "");
	    brlcad::step::SetEntity(new_oriented_edge, "edge_element",
		info->edge_curves.at(ec_index));
	    brlcad::step::SetBoolean(new_oriented_edge, "orientation",
		trim->m_bRev3d ? BFalse : BTrue);
	    info->oriented_edges.push_back(new_oriented_edge);
	    i = (int)info->oriented_edges.size() - 1;
	    brlcad::step::AddEntity(edge_loop, "edge_list",
		info->oriented_edges.at(i));
	}
    }
}

/* Rather than have the organization of elements in the step file be
 * dictated by the order in which they are built up from ON_Brep,
 * define a function that iterates over the structures to populate the
 * list in a way that puts the more complex/high-level structures at
 * the beginning of the file.
 */
void
Populate_Instance_List(ON_Brep_Info_AP203 *info)
{
    std::vector<STEPentity *>::iterator v_it;

    /* Topology */

    // High level structures
    info->instance_list->Append((STEPentity *)(info->advanced_brep), completeSE);
    info->instance_list->Append((STEPentity *)(info->manifold_solid_brep), completeSE);
    for (v_it = info->oriented_shells.begin();
	    v_it != info->oriented_shells.end(); ++v_it)
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    for (v_it = info->shells.begin(); v_it != info->shells.end(); ++v_it)
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);

    // Faces
    for (v_it = info->faces.begin(); v_it != info->faces.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // inner_bounds
    for (v_it = info->inner_bounds.begin(); v_it != info->inner_bounds.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // outer_bounds
    for (v_it = info->outer_bounds.begin(); v_it != info->outer_bounds.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // edge_loops
    for (v_it = info->edge_loops.begin(); v_it != info->edge_loops.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // oriented_edges
    for (v_it = info->oriented_edges.begin(); v_it != info->oriented_edges.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // edge_curves
    for (v_it = info->edge_curves.begin(); v_it != info->edge_curves.end(); ++v_it) {
	if (*v_it) info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // vertex_pnts
    for (v_it = info->vertex_pnts.begin(); v_it != info->vertex_pnts.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    /* Geometry */

    // surfaces
    for (v_it = info->surfaces.begin(); v_it != info->surfaces.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // three_dimensional_curves
    for (v_it = info->three_dimensional_curves.begin(); v_it != info->three_dimensional_curves.end(); ++v_it) {
	if (*v_it) info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // directions
    for (v_it = info->directions.begin(); v_it != info->directions.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // vectors
    for (v_it = info->vectors.begin(); v_it != info->vectors.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // cartesian_pnts
    for (v_it = info->cartesian_pnts.begin(); v_it != info->cartesian_pnts.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // Now that we know the ids for all the points, we can finalize the surface definitions
    ON_NurbsSurfaceCV_Finalize_GenericAggregates(info);
}

bool
ON_BRep_to_STEP(struct directory *dp, ON_Brep *brep, AP203_Contents *sc,
    STEPentity **brep_shape, STEPentity **brep_product, STEPentity **brep_manifold,
    bool create_product, std::string *diagnostic)
{
    //ON_wString wstr;
    //ON_TextLog dump(wstr);
    //brep->Dump(dump);
    //ON_String ssw = wstr;
    //bu_log("Brep:\n %s\n", ssw.Array());
    if (brep_shape) *brep_shape = NULL;
    if (brep_product) *brep_product = NULL;
    if (brep_manifold) *brep_manifold = NULL;
    if (!brep || !brep->m_F.Count()) {
	if (diagnostic) *diagnostic = "BRep has no exportable faces";
	return false;
    }

    BrepShellPlan shell_plan;
    std::string shell_diagnostic;
    if (!build_shell_plan(*brep, shell_plan, shell_diagnostic)) {
	if (diagnostic) *diagnostic = shell_diagnostic;
	return false;
    }

    ON_Brep_Info_AP203 *info = new ON_Brep_Info_AP203();
    info->registry = sc->registry;
    info->instance_list = sc->instance_list;
    info->split_closed = 0; /* For now, don't try splitting things - need some libbrep functionality before that can work */

    info->cartesian_pnts.assign(brep->m_V.Count(), (STEPentity *)0);
    info->vertex_pnts.assign(brep->m_V.Count(), (STEPentity *)0);
    info->three_dimensional_curves.assign(brep->m_C3.Count(), (STEPentity *)0);
    info->edge_curves.assign(brep->m_E.Count(), (STEPentity *)0);
    info->edge_loops.assign(brep->m_L.Count(), (STEPentity *)0);
    info->surfaces.assign(brep->m_S.Count(), (STEPentity *)0);
    info->faces.assign(brep->m_F.Count(), (STEPentity *)0);
    info->closed_shell = NULL;
    info->manifold_solid_brep = NULL;
    info->advanced_brep = NULL;

    STEPentity *context = sc->default_context;

    // Set up vertices and associated cartesian points
    for (int i = 0; i < brep->m_V.Count(); ++i) {
	// Cartesian points (actual 3D geometry)
	info->cartesian_pnts.at(i) = info->registry->ObjCreate("CARTESIAN_POINT");
	brlcad::step::SetString(info->cartesian_pnts.at(i), "name", "");
	ON_3dPoint v_pnt = brep->m_V[i].Point();
	ON_3dPoint_to_Cartesian_point(&(v_pnt), info->cartesian_pnts.at(i));

	// Vertex points (topological, references actual 3D geometry)
	info->vertex_pnts.at(i) = info->registry->ObjCreate("VERTEX_POINT");
	brlcad::step::SetString(info->vertex_pnts.at(i), "name", "");
	brlcad::step::SetEntity(info->vertex_pnts.at(i), "vertex_geometry",
	    info->cartesian_pnts.at(i));
    }

    // 3D curves
    for (int i = 0; i < brep->m_C3.Count(); ++i) {
	bool curve_converted = false;
	ON_Curve* curve = brep->m_C3[i];

	/* Supported curve types */
	ON_ArcCurve *a_curve = ON_ArcCurve::Cast(curve);
	ON_LineCurve *l_curve = ON_LineCurve::Cast(curve);
	ON_NurbsCurve *n_curve = ON_NurbsCurve::Cast(curve);
	ON_PolyCurve *p_curve = ON_PolyCurve::Cast(curve);

	if (a_curve && !curve_converted) {
	    ON_NurbsCurve arc_nurb;
	    a_curve->GetNurbForm(arc_nurb);
	    curve_converted = ON_NurbsCurve_to_STEP(&arc_nurb, info, i);
	}

	if (l_curve && !curve_converted) {
	    ON_Line *m_line = &(l_curve->m_line);

	    /* In STEP, a line consists of a cartesian point and a 3D
	     * vector.  Since it does not appear that OpenNURBS data
	     * structures reference m_V points for these constructs,
	     * create our own
	     */

	    info->three_dimensional_curves.at(i) = info->registry->ObjCreate("LINE");

	    STEPentity *curr_line = info->three_dimensional_curves.at(i);
	    STEPentity *point = info->registry->ObjCreate("CARTESIAN_POINT");
	    STEPentity *curr_dir = info->registry->ObjCreate("VECTOR");
	    STEPentity *orientation = info->registry->ObjCreate("DIRECTION");
	    brlcad::step::SetEntity(curr_line, "pnt", point);
	    ON_3dPoint_to_Cartesian_point(&(m_line->from), point);
	    brlcad::step::SetEntity(curr_line, "dir", curr_dir);
	    brlcad::step::SetEntity(curr_dir, "orientation", orientation);
	    ON_3dVector on_dir = m_line->Direction();
	    ON_3dVector_to_Direction(&(on_dir), orientation);
	    brlcad::step::SetReal(curr_dir, "magnitude", m_line->Length());
	    brlcad::step::SetString(point, "name", "");
	    brlcad::step::SetString(orientation, "name", "");
	    brlcad::step::SetString(curr_dir, "name", "");
	    brlcad::step::SetString(curr_line, "name", "");

	    info->cartesian_pnts.push_back(point);
	    info->directions.push_back(orientation);
	    info->vectors.push_back(curr_dir);
	    curve_converted = true;
	}

	if (p_curve && !curve_converted) {
	    ON_NurbsCurve poly_nurb;
	    p_curve->GetNurbForm(poly_nurb);
	    curve_converted = ON_NurbsCurve_to_STEP(&poly_nurb, info, i);
	}

	if (n_curve && !curve_converted) {
	    curve_converted = ON_NurbsCurve_to_STEP(n_curve, info, i);
	}

	/* Whatever this is, if it's not a supported type and it does
	 * have a NURBS form, use that
	 */
	if (!curve_converted) {
	    ON_NurbsCurve nurb_form;
	    curve->GetNurbForm(nurb_form);
	    curve_converted = ON_NurbsCurve_to_STEP(&nurb_form, info, i);
	    if (!curve_converted) std::cout << "Curve not converted! " << i << "\n";
	}

    }

    // edge topology - ON_BrepEdge -> edge curve
    for (int i = 0; i < brep->m_E.Count(); ++i) {
	ON_BrepEdge *edge = &(brep->m_E[i]);
	info->edge_curves.at(i) = info->registry->ObjCreate("EDGE_CURVE");

	STEPentity *e_curve = info->edge_curves.at(i);
	brlcad::step::SetString(e_curve, "name", "");
	brlcad::step::SetEntity(e_curve, "edge_geometry",
	    info->three_dimensional_curves.at(edge->EdgeCurveIndexOf()));
	/* ON_BrepEdge is a proxy over m_C3.  Its topological start/end can run
	 * opposite the stored curve, particularly after STEP import.  STEP's
	 * same_sense records precisely that relationship. */
	brlcad::step::SetBoolean(e_curve, "same_sense",
	    edge->ProxyCurveIsReversed() ? BFalse : BTrue);
	brlcad::step::SetEntity(e_curve, "edge_start",
	    info->vertex_pnts.at(edge->Vertex(0)->m_vertex_index));
	brlcad::step::SetEntity(e_curve, "edge_end",
	    info->vertex_pnts.at(edge->Vertex(1)->m_vertex_index));
    }

    // loop topology.  STEP defines loops with 3D edge curves, but
    // OpenNURBS describes ON_BrepLoops with 2d trim curves.  So for a
    // given loop, we need to iterate over the trims, for each trim
    // get the index of its corresponding edge, and add that edge to
    // the _edge_list for the loop.
    for (int i = 0; i < brep->m_L.Count(); ++i) {
	ON_BrepLoop *loop= &(brep->m_L[i]);
	info->edge_loops.at(i) = info->registry->ObjCreate("EDGE_LOOP");
	brlcad::step::SetString(info->edge_loops.at(i), "name", "");
	for (int l = 0; l < loop->TrimCount(); ++l) {
	    Add_Edge(loop->Trim(l), info->edge_loops.at(i), info);
	}
    }

    // surfaces - TODO - need to handle cylindrical, conical,
    // toroidal, etc. types that are enumerated
    for (int i = 0; i < brep->m_S.Count(); ++i) {
	bool surface_converted = false;
	ON_Surface* surface = brep->m_S[i];
	/* Supported surface types */
	ON_OffsetSurface *o_surface = ON_OffsetSurface::Cast(surface);
	ON_PlaneSurface *p_surface = ON_PlaneSurface::Cast(surface);
	ON_ClippingPlaneSurface *pc_surface = ON_ClippingPlaneSurface::Cast(surface);
	ON_NurbsSurface *n_surface = ON_NurbsSurface::Cast(surface);
	ON_RevSurface *rev_surface = ON_RevSurface::Cast(surface);
	ON_SumSurface *sum_surface = ON_SumSurface::Cast(surface);
	ON_SurfaceProxy *surface_proxy = ON_SurfaceProxy::Cast(surface);

	if (o_surface && !surface_converted) {
	    ON_NurbsSurface o_nurb;
	    o_surface->GetNurbForm(o_nurb);
	    surface_converted = ON_NurbsSurface_to_STEP(&o_nurb, info, i);
	}

	if (p_surface && !surface_converted) {
	    ON_NurbsSurface p_nurb;
	    p_surface->GetNurbForm(p_nurb);
	    surface_converted = ON_NurbsSurface_to_STEP(&p_nurb, info, i);
	    if (!p_nurb.IsRational()) {
		brlcad::step::SetEnum(info->surfaces.at(i), "surface_form", "PLANE_SURF");
	    }
	}

	if (pc_surface && !surface_converted) {
	    std::cout << "Have CuttingPlaneSurface\n";
	}

	if (n_surface && !surface_converted) {
	    surface_converted = ON_NurbsSurface_to_STEP(n_surface, info, i);
	}

	if (rev_surface && !surface_converted) {
	    ON_NurbsSurface rev_nurb;
	    rev_surface->GetNurbForm(rev_nurb);
	    surface_converted = ON_NurbsSurface_to_STEP(&rev_nurb, info, i);
	    /* Surfaces of revolution can satisfy several of the surface types
	     * offered by STEP - check*/
	    /* TODO - need to also expose the surface type via a map, since rational
	     * surface composites do not offer a hook that is compatible with the C++
	     * surface_form_ method.*/
	    /*
	    if (rev_surface->IsPlanar()) {
	    }
	    if (rev_surface->IsSphere()) {
	    }
	    if (rev_surface->IsCylinder()) {
	    }
	    if (rev_surface->IsCone()) {
	    }
	    if (rev_surface->IsTorus()) {
	    }*/
	    /* If none of the other conditions are satisfied, by definition we still have
	     * a surface of revolution...*/
	}

	if (sum_surface && !surface_converted) {
	    ON_NurbsSurface sum_nurb;
	    sum_surface->GetNurbForm(sum_nurb);
	    surface_converted = ON_NurbsSurface_to_STEP(&sum_nurb, info, i);
	}

	if (surface_proxy && !surface_converted) {
	    std::cout << "Have SurfaceProxy\n";
	}

	if (!surface_converted) {
	    ON_NurbsSurface nurb_surf_form;
	    surface->GetNurbForm(nurb_surf_form);
	    surface_converted = ON_NurbsSurface_to_STEP(&nurb_surf_form, info, i);
	    if (!surface_converted) std::cout << "Failed to convert surface!\n";
	}


    }

    /* A BRL-CAD BRep stores a cavity shell with its faces already directed
     * toward the material.  STEP's BREP_WITH_VOIDS instead gives the base
     * CLOSED_SHELL a solid-shell orientation and reverses that shell through
     * ORIENTED_CLOSED_SHELL.  Invert only the base ADVANCED_FACE senses here;
     * the false-oriented wrapper then reconstructs the source face direction
     * exactly rather than reversing the cavity twice. */
    std::vector<bool> reverse_void_base_face(brep->m_F.Count(), false);
    if (shell_plan.solid && shell_plan.components.size() > 1) {
	for (size_t component = 0; component < shell_plan.components.size(); ++component) {
	    if (static_cast<int>(component) == shell_plan.outer_component) continue;
	    for (std::vector<int>::const_iterator face =
		    shell_plan.components[component].begin();
		    face != shell_plan.components[component].end(); ++face)
		reverse_void_base_face[*face] = true;
	}
    }

    // faces
    for (int i = 0; i < brep->m_F.Count(); ++i) {
	ON_BrepFace* face = &(brep->m_F[i]);
	info->faces.at(i) = info->registry->ObjCreate("ADVANCED_FACE");
	STEPentity *step_face = info->faces.at(i);
	brlcad::step::SetString(step_face, "name", "");
	if (!brlcad::step::SetEntity(step_face, "face_geometry",
		info->surfaces.at(face->SurfaceIndexOf()))) {
	    bu_log("Unable to assign advanced face geometry for %s\n", dp->d_namep);
	    if (diagnostic) *diagnostic = "could not assign STEP advanced-face geometry";
	    delete info;
	    return false;
	}
	bool same_sense = !face->m_bRev;
	if (reverse_void_base_face[i]) same_sense = !same_sense;
	brlcad::step::SetBoolean(step_face, "same_sense",
	    same_sense ? BTrue : BFalse);

	for (int j = 0; j < face->LoopCount(); ++j) {
	    ON_BrepLoop *curr_loop = face->Loop(j);
	    if (curr_loop == face->OuterLoop()) {
		STEPentity *outer_bound = info->registry->ObjCreate("FACE_OUTER_BOUND");
		brlcad::step::SetString(outer_bound, "name", "");
		brlcad::step::SetEntity(outer_bound, "bound",
		    info->edge_loops.at(curr_loop->m_loop_index));
		brlcad::step::SetBoolean(outer_bound, "orientation",
		    face->m_bRev ? BFalse : BTrue);
		brlcad::step::AddEntity(step_face, "bounds", outer_bound);
		info->outer_bounds.push_back(outer_bound);
	    } else {
		STEPentity *inner_bound = info->registry->ObjCreate("FACE_BOUND");
		brlcad::step::SetString(inner_bound, "name", "");
		brlcad::step::SetEntity(inner_bound, "bound",
		    info->edge_loops.at(curr_loop->m_loop_index));
		brlcad::step::SetBoolean(inner_bound, "orientation",
		    face->m_bRev ? BFalse : BTrue);
		brlcad::step::AddEntity(step_face, "bounds", inner_bound);
		info->inner_bounds.push_back(inner_bound);
	    }
	}
    }

    /* Preserve the source topology class.  Open BReps are surface models;
     * disconnected shells in a solid are emitted as one outer shell plus
     * explicitly oriented voids only after build_shell_plan proves nesting. */
    for (size_t component = 0; component < shell_plan.components.size(); ++component) {
	const char *shell_type = shell_plan.solid ||
	    shell_plan.component_closed[component] ? "CLOSED_SHELL" : "OPEN_SHELL";
	STEPentity *shell = info->registry->ObjCreate(shell_type);
	if (!shell) {
	    if (diagnostic) *diagnostic = "schema cannot represent the required BRep shell";
	    delete info;
	    return false;
	}
	brlcad::step::SetString(shell, "name", "");
	for (std::vector<int>::const_iterator face =
		shell_plan.components[component].begin();
		face != shell_plan.components[component].end(); ++face) {
	    if (!brlcad::step::AddEntity(shell, "cfs_faces", info->faces.at(*face))) {
		if (diagnostic) *diagnostic = "could not assign a face to its STEP shell";
		delete info;
		return false;
	    }
	}
	info->shells.push_back(shell);
    }

    const char *representation_type = NULL;
    if (shell_plan.solid) {
	info->closed_shell = info->shells[shell_plan.outer_component];
	if (info->shells.size() == 1) {
	    info->manifold_solid_brep =
		info->registry->ObjCreate("MANIFOLD_SOLID_BREP");
	} else {
	    info->manifold_solid_brep = info->registry->ObjCreate("BREP_WITH_VOIDS");
	}
	if (!info->manifold_solid_brep ||
		!brlcad::step::SetEntity(info->manifold_solid_brep, "outer",
		    info->closed_shell)) {
	    if (diagnostic) *diagnostic = "could not construct a STEP manifold BRep";
	    delete info;
	    return false;
	}
	brlcad::step::SetString(info->manifold_solid_brep, "name", "");
	for (size_t component = 0; component < info->shells.size(); ++component) {
	    if (static_cast<int>(component) == shell_plan.outer_component) continue;
	    STEPentity *oriented = info->registry->ObjCreate("ORIENTED_CLOSED_SHELL");
	    if (!oriented ||
		    !brlcad::step::SetEntity(oriented, "closed_shell_element",
			info->shells[component]) ||
		    !brlcad::step::AddEntity(info->manifold_solid_brep, "voids",
			oriented)) {
		if (diagnostic) *diagnostic = "could not construct a STEP oriented void shell";
		delete info;
		return false;
	    }
	    brlcad::step::SetString(oriented, "name", "");
	    brlcad::step::SetBoolean(oriented, "orientation", BFalse);
	    info->oriented_shells.push_back(oriented);
	}
	representation_type = "ADVANCED_BREP_SHAPE_REPRESENTATION";
    } else {
	info->manifold_solid_brep =
	    info->registry->ObjCreate("SHELL_BASED_SURFACE_MODEL");
	if (!info->manifold_solid_brep) {
	    if (diagnostic) *diagnostic = "schema cannot represent a shell-based surface model";
	    delete info;
	    return false;
	}
	brlcad::step::SetString(info->manifold_solid_brep, "name", "");
	for (std::vector<STEPentity *>::const_iterator shell = info->shells.begin();
		shell != info->shells.end(); ++shell) {
	    if (!brlcad::step::AddEntity(info->manifold_solid_brep,
		    "sbsm_boundary", *shell)) {
		if (diagnostic) *diagnostic = "could not assign a surface-model boundary";
		delete info;
		return false;
	    }
	}
	representation_type = "MANIFOLD_SURFACE_SHAPE_REPRESENTATION";
    }

    info->advanced_brep = info->registry->ObjCreate(representation_type);
    std::ostringstream ss;
    ss << "'" << dp->d_namep << "'";
    std::string str = ss.str();
    brlcad::step::SetString(info->advanced_brep, "name", str.c_str());
    if (!brlcad::step::AddEntity(info->advanced_brep, "items",
	    info->manifold_solid_brep)) {
	bu_log("Unable to add BRep representation item for %s\n", dp->d_namep);
	if (diagnostic) *diagnostic = "could not add geometry to its STEP shape representation";
	delete info;
	return false;
    }
    brlcad::step::SetEntity(info->advanced_brep, "context_of_items", context);

    (*brep_product) = create_product ?
	Add_Shape_Definition_Representation(dp, sc, info->advanced_brep) : NULL;
    (*brep_shape) = info->advanced_brep;
    (*brep_manifold) = info->manifold_solid_brep;

    Populate_Instance_List(info);

    delete info;
    return (!create_product || *brep_product) && *brep_shape && *brep_manifold;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
