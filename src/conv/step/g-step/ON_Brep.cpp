/*                     O N _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include "common.h"

#include <sstream>
#include <map>

#include "G_STEP_internal.h"
#include "ON_Brep.h"

void
ON_3dPoint_to_Cartesian_point(ON_3dPoint *inpnt, SdaiCartesian_point *step_pnt) {
    XYZ_to_Cartesian_point(inpnt->x, inpnt->y, inpnt->z, step_pnt);
}

void
ON_3dVector_to_Direction(ON_3dVector *invect, SdaiDirection *step_direction) {
    invect->Unitize();
    XYZ_to_Direction(invect->x, invect->y, invect->z, step_direction);
}

/* STEP needs explicit edges corresponding to what in OpenNURBS are
 * the UV space trimming curves
 */
void
Add_Edge(ON_BrepTrim *trim, SdaiPath *e_loop_path, ON_Brep_Info_AP203 *info)
{
    ON_BrepEdge *edge = trim->Edge();
    int i = -1;

    // Some trims don't have an associated edge - allow for that
    if (edge) {
	STEPentity *new_oriented_edge = info->registry->ObjCreate("ORIENTED_EDGE");
	SdaiOriented_edge *oriented_edge = (SdaiOriented_edge *)new_oriented_edge;
	SdaiEdge_curve *e_curve = (SdaiEdge_curve *)info->edge_curves.at(edge->EdgeCurveIndexOf());

	oriented_edge->name_("''");
	oriented_edge->edge_element_((SdaiEdge *)e_curve);

	if (trim->m_bRev3d) {
	    oriented_edge->edge_start_(((SdaiVertex *)info->vertex_pnts.at(edge->Vertex(1)->m_vertex_index)));
	    oriented_edge->edge_end_(((SdaiVertex *)info->vertex_pnts.at(edge->Vertex(0)->m_vertex_index)));
	} else {
	    oriented_edge->edge_start_(((SdaiVertex *)info->vertex_pnts.at(edge->Vertex(0)->m_vertex_index)));
	    oriented_edge->edge_end_(((SdaiVertex *)info->vertex_pnts.at(edge->Vertex(1)->m_vertex_index)));
	}

	// add the edge
	oriented_edge->orientation_((Boolean)!trim->m_bRev3d);
	info->oriented_edges.push_back(new_oriented_edge);
	i = info->oriented_edges.size() - 1;
	e_loop_path->edge_list_()->AddNode(new EntityNode((SDAI_Application_instance *)(info->oriented_edges.at(i))));
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
    std::map<int, std::pair<STEPentity *, STEPentity *> >::iterator c_it;
    std::map<int, STEPentity * >::iterator mpt_it;

    /* Topology */

    // High level structures
    info->instance_list->Append((STEPentity *)(info->advanced_brep), completeSE);
    info->instance_list->Append((STEPentity *)(info->manifold_solid_brep), completeSE);
    info->instance_list->Append((STEPentity *)(info->closed_shell), completeSE);

    // Faces
    for(v_it = info->faces.begin(); v_it != info->faces.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // inner_bounds
    for(v_it = info->inner_bounds.begin(); v_it != info->inner_bounds.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // outer_bounds
    for(v_it = info->outer_bounds.begin(); v_it != info->outer_bounds.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // edge_loops
    for(v_it = info->edge_loops.begin(); v_it != info->edge_loops.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // oriented_edges
    for(v_it = info->oriented_edges.begin(); v_it != info->oriented_edges.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // edge_curves
    for(v_it = info->edge_curves.begin(); v_it != info->edge_curves.end(); ++v_it) {
	if (*v_it) info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // vertex_pnts
    for(v_it = info->vertex_pnts.begin(); v_it != info->vertex_pnts.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    /* Geometry */

    // surfaces
    for(v_it = info->surfaces.begin(); v_it != info->surfaces.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // three_dimensional_curves
    for(v_it = info->three_dimensional_curves.begin(); v_it != info->three_dimensional_curves.end(); ++v_it) {
	if (*v_it) info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // directions
    for(v_it = info->directions.begin(); v_it != info->directions.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // vectors
    for(v_it = info->vectors.begin(); v_it != info->vectors.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // cartesian_pnts
    for(v_it = info->cartesian_pnts.begin(); v_it != info->cartesian_pnts.end(); ++v_it) {
	info->instance_list->Append((STEPentity *)(*v_it), completeSE);
    }

    // Now that we know the ids for all the points, we can finalize the surface definitions
    ON_NurbsSurfaceCV_Finalize_GenericAggregates(info);
}

void
ON_BRep_to_STEP(struct directory *dp, struct rt_db_internal *intern, Registry *registry, InstMgr *instance_list, STEPentity **brep_shape, STEPentity **brep_product)
{
    RT_CK_DB_INTERNAL(intern);
    struct rt_brep_internal *bi = (struct rt_brep_internal*)(intern->idb_ptr);
    RT_BREP_TEST_MAGIC(bi);
    ON_Brep *brep = bi->brep;
    //ON_wString wstr;
    //ON_TextLog dump(wstr);
    //brep->Dump(dump);
    //ON_String ss = wstr;
    //bu_log("Brep:\n %s\n", ss.Array());
    ON_Brep_Info_AP203 *info = new ON_Brep_Info_AP203();
    info->registry = registry;
    info->instance_list = instance_list;
    info->split_closed = 0; /* For now, don't try splitting things - need some libbrep functionality before that can work */

    info->cartesian_pnts.assign(brep->m_V.Count(), (STEPentity *)0);
    info->vertex_pnts.assign(brep->m_V.Count(), (STEPentity *)0);
    info->three_dimensional_curves.assign(brep->m_C3.Count(), (STEPentity *)0);
    info->edge_curves.assign(brep->m_E.Count(), (STEPentity *)0);
    info->edge_loops.assign(brep->m_L.Count(), (STEPentity *)0);
    info->surfaces.assign(brep->m_S.Count(), (STEPentity *)0);
    info->faces.assign(brep->m_F.Count(), (STEPentity *)0);

    /* The BRep needs a context - TODO: this can probably be used once for the whole step file... */
    STEPcomplex *context = Add_Default_Geometric_Context(info->registry, info->instance_list);

    // Set up vertices and associated cartesian points
    for (int i = 0; i < brep->m_V.Count(); ++i) {
	// Cartesian points (actual 3D geometry)
	info->cartesian_pnts.at(i) = info->registry->ObjCreate("CARTESIAN_POINT");
	((SdaiCartesian_point *)info->cartesian_pnts.at(i))->name_("''");
	ON_3dPoint v_pnt = brep->m_V[i].Point();
	ON_3dPoint_to_Cartesian_point(&(v_pnt), (SdaiCartesian_point *)info->cartesian_pnts.at(i));

	// Vertex points (topological, references actual 3D geometry)
	info->vertex_pnts.at(i) = info->registry->ObjCreate("VERTEX_POINT");
	((SdaiVertex_point *)info->vertex_pnts.at(i))->name_("''");
	((SdaiVertex_point *)info->vertex_pnts.at(i))->vertex_geometry_((const SdaiPoint_ptr)info->cartesian_pnts.at(i));
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

	    SdaiLine *curr_line = (SdaiLine *)info->three_dimensional_curves.at(i);
	    curr_line->pnt_((SdaiCartesian_point *)info->registry->ObjCreate("CARTESIAN_POINT"));
	    ON_3dPoint_to_Cartesian_point(&(m_line->from), curr_line->pnt_());
	    curr_line->dir_((SdaiVector *)info->registry->ObjCreate("VECTOR"));
	    SdaiVector *curr_dir = curr_line->dir_();
	    curr_dir->orientation_((SdaiDirection *)info->registry->ObjCreate("DIRECTION"));
	    ON_3dVector on_dir = m_line->Direction();
	    ON_3dVector_to_Direction(&(on_dir), curr_line->dir_()->orientation_());
	    curr_line->dir_()->magnitude_(m_line->Length());
	    curr_line->pnt_()->name_("''");
	    curr_dir->orientation_()->name_("''");
	    curr_line->dir_()->name_("''");
	    curr_line->name_("''");

	    info->cartesian_pnts.push_back((STEPentity *)curr_line->pnt_());
	    info->directions.push_back((STEPentity *)curr_dir->orientation_());
	    info->vectors.push_back((STEPentity *)curr_line->dir_());
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

	SdaiEdge_curve *e_curve = (SdaiEdge_curve *)info->edge_curves.at(i);
	e_curve->name_("''");
	e_curve->edge_geometry_(((SdaiCurve *)info->three_dimensional_curves.at(edge->EdgeCurveIndexOf())));
	e_curve->same_sense_(BTrue);
	e_curve->edge_start_(((SdaiVertex *)info->vertex_pnts.at(edge->Vertex(0)->m_vertex_index)));
	e_curve->edge_end_(((SdaiVertex *)info->vertex_pnts.at(edge->Vertex(1)->m_vertex_index)));
    }

    // loop topology.  STEP defines loops with 3D edge curves, but
    // OpenNURBS describes ON_BrepLoops with 2d trim curves.  So for a
    // given loop, we need to iterate over the trims, for each trim
    // get the index of its corresponding edge, and add that edge to
    // the _edge_list for the loop.
    for (int i = 0; i < brep->m_L.Count(); ++i) {
	ON_BrepLoop *loop= &(brep->m_L[i]);
	info->edge_loops.at(i) = info->registry->ObjCreate("EDGE_LOOP");
	((SdaiEdge_loop *)info->edge_loops.at(i))->name_("''");

	// Why doesn't SdaiEdge_loop's edge_list_() function give use
	// the edge_list from the SdaiPath??  Initialized to NULL and
	// crashes - what good is it?  Have to get at the internal
	// SdaiPath directly to build something that STEPwrite will
	// output.
	SdaiPath *e_loop_path = (SdaiPath *)info->edge_loops.at(i)->GetNextMiEntity();
	for (int l = 0; l < loop->TrimCount(); ++l) {
	    Add_Edge(loop->Trim(l), e_loop_path, info);
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
		SdaiB_spline_surface_with_knots *curr_surface = (SdaiB_spline_surface_with_knots *)info->surfaces.at(i);
		curr_surface->surface_form_(B_spline_surface_form__plane_surf);
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
	    if(rev_surface->IsPlanar()) {
	    }
	    if(rev_surface->IsSphere()) {
	    }
	    if(rev_surface->IsCylinder()) {
	    }
	    if(rev_surface->IsCone()) {
	    }
	    if(rev_surface->IsTorus()) {
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

    // faces
    for (int i = 0; i < brep->m_F.Count(); ++i) {
	ON_BrepFace* face = &(brep->m_F[i]);
	info->faces.at(i) = info->registry->ObjCreate("ADVANCED_FACE");
	SdaiAdvanced_face *step_face = (SdaiAdvanced_face *)info->faces.at(i);
	step_face->name_("''");
	step_face->face_geometry_((SdaiSurface *)info->surfaces.at(face->SurfaceIndexOf()));
	// TODO - is m_bRev the same thing as same_sense?
	step_face->same_sense_((const Boolean)!(face->m_bRev));

	EntityAggregate *bounds = step_face->bounds_();

	for (int j = 0; j < face->LoopCount(); ++j) {
	    ON_BrepLoop *curr_loop = face->Loop(j);
	    if (curr_loop == face->OuterLoop()) {
		SdaiFace_outer_bound *outer_bound = (SdaiFace_outer_bound *)info->registry->ObjCreate("FACE_OUTER_BOUND");
		outer_bound->name_("''");
		outer_bound->bound_((SdaiLoop *)info->edge_loops.at(curr_loop->m_loop_index));
		outer_bound->orientation_((const Boolean)!(face->m_bRev));
		bounds->AddNode(new EntityNode((SDAI_Application_instance *)outer_bound));
		info->outer_bounds.push_back((STEPentity *)outer_bound);
	    } else {
		SdaiFace_bound *inner_bound = (SdaiFace_bound *)info->registry->ObjCreate("FACE_BOUND");
		inner_bound->name_("''");
		inner_bound->bound_((SdaiLoop *)info->edge_loops.at(curr_loop->m_loop_index));
		inner_bound->orientation_((const Boolean)!(face->m_bRev));
		bounds->AddNode(new EntityNode((SDAI_Application_instance *)inner_bound));
		info->inner_bounds.push_back((STEPentity *)inner_bound);
	    }
	}
    }

    // Closed shell that assembles the faces
    info->closed_shell = (SdaiClosed_shell *)info->registry->ObjCreate("CLOSED_SHELL");
    info->closed_shell->name_("''");

    EntityAggregate *shell_faces = info->closed_shell->cfs_faces_();
    for (int i = 0; i < brep->m_F.Count(); ++i) {
	shell_faces->AddNode(new EntityNode((SDAI_Application_instance *)info->faces.at(i)));
    }

    // Solid manifold BRep
    info->manifold_solid_brep = (SdaiManifold_solid_brep *)info->registry->ObjCreate("MANIFOLD_SOLID_BREP");
    info->manifold_solid_brep->outer_(info->closed_shell);
    info->manifold_solid_brep->name_("''");

    // Advanced BRep shape representation - this is the object step-g will look for
    info->advanced_brep= (SdaiAdvanced_brep_shape_representation *)info->registry->ObjCreate("ADVANCED_BREP_SHAPE_REPRESENTATION");
    std::ostringstream ss;
    ss << "'" << dp->d_namep << "'";
    std::string str = ss.str();
    info->advanced_brep->name_(str.c_str());
    EntityAggregate *items = info->advanced_brep->items_();
    items->AddNode(new EntityNode((SDAI_Application_instance *)info->manifold_solid_brep));
    info->advanced_brep->context_of_items_((SdaiRepresentation_context *) context);

    (*brep_product) = Add_Shape_Definition_Representation(info->registry, info->instance_list, info->advanced_brep);
    (*brep_shape) = info->advanced_brep;

    Populate_Instance_List(info);

    delete info;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
