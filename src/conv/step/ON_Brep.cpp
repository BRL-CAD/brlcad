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


// Make entity arrays for each of the m_V, m_S, etc arrays and create step instances of them,
// starting with the basic ones.
//
// The array indices in the ON_Brep will correspond to the step entity array locations that
// hold the step version of each entity.
//
// then, need to map the ON_Brep hierarchy to the corresponding STEP hierarchy
//
// brep -> advanced_brep_shape_representation
//         manifold_solid_brep
// faces-> closed_shell
//         advanced_face
//
// surface-> bspline_surface_with_knots
//           cartesian_point
//
// outer 3d trim loop ->  face_outer_bound
//                        edge_loop
//                        oriented_edge
//                        edge_curve
//                        bspline_curve_with_knots
//                        vertex_point
//                        cartesian_point
//
// 2d points -> point_on_surface
// 1d points to bound curve -> point_on_curve
//
// 2d trimming curves -> pcurve using point_on_surface? almost doesn't look as if there is a good AP203 way to represent 2d trimming curves...
//

STEPattribute * getAttribute(STEPentity *ent, const char *name)
{
	STEPattribute *attr, *attr_result;
	ent->ResetAttributes();
	while ((attr = ent->NextAttribute()) != NULL) {
		std::string attrname = attr->Name();
		if (attrname.compare(name) == 0) {
			attr_result = attr;
			break;
		}
	}
	ent->ResetAttributes();
	return attr_result;
}

bool ON_BRep_to_STEP(ON_Brep *brep)
{
	STEPentity ** vertex_cartesian_pt_array = new STEPentity*[brep->m_V.Count()];
	STEPentity ** vertex_pt_array = new STEPentity*[brep->m_V.Count()];
	STEPentity ** bspline_curve_array = new STEPentity*[brep->m_E.Count()];
	STEPentity ** edge_curve_array = new STEPentity*[brep->m_E.Count()];
	STEPentity ** oriented_edge_array = new STEPentity*[2*brep->m_E.Count()];
	STEPentity ** edge_loop_array = new STEPentity*[brep->m_L.Count()];
	STEPentity ** outer_bounds_array = new STEPentity*[brep->m_F.Count()];
	STEPentity ** surface_array = new STEPentity*[brep->m_S.Count()];
	STEPentity ** faces_array = new STEPentity*[brep->m_F.Count()];
	STEPentity *closed_shell = new STEPentity;
	STEPentity *manifold_solid_brep = new STEPentity;
	STEPentity *advanced_brep = new STEPentity;

        // Set up vertices and associated cartesian points
	for (int i = 0; i < brep->m_V.Count(); ++i) {
                // Cartesian points (actual 3D geometry)
		vertex_cartesian_pt_array[i] = registry->ObjCreate("CARTESIAN_POINT");
		STEPentity *ent = vertex_cartesian_pt_array[i];
		STEPattribute *coords = getAttribute(ent, "coordinates");
		RealAggregate_ptr coord_vals = coords->coordinates_();
                RealNode *xnode = new RealNode();
                xnode->value = brep->m_V[i].x;
		coord_vals.AddNode(xnode);
                RealNode *ynode = new RealNode();
                ynode->value = brep->m_V[i].y;
		coord_vals.AddNode(ynode);
                RealNode *znode = new RealNode();
                znode->value = brep->m_V[i].z;
		coord_vals.AddNode(znode);
                // Vertex points (topological, references actual 3D geometry)
		vertex_pt_array[i] = registry->ObjCreate("VERTEX_POINT");
	}
}
