/*                C D T _ V A L I D A T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2024 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt_validate.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

/* TODO:
 *
 * Need a process for how to modify edges and faces, and in what order.  Need to
 * come up with heuristics for applying different "fixing" strategies:
 *
 * - remove problematic surface point from face tessellation
 * - insert new point(s) at midpoints of edges of problem face
 * - insert new point at the center of the problem face
 * - split edge involved with problem face and retessellate both faces on that edge
 *
 * Different strategies may be appropriate depending on the characteristics of the
 * "bad" triangle - for example, a triangle with all three brep normals nearly perpendicular
 * to the triangle normal and having only one non-edge point we should probably try to
 * handle by removing the surface point.  On the other hand, if the angle is not as
 * perpendicular and the surface area is significant, we might want to add a new point
 * on the longest edge of the triangle (which might be a shared edge with another face.)
 *
 * Also, consider whether to retessellate whole face or try to assemble "local" set of
 * faces involved, build bounding polygon and do new local tessellation.  Latter
 * is a lot more work but might avoid re-introducing new problems elsewhere in a
 * mesh during a full re-CDT.
 *
 * Need to be able to introduce new edge points in specific subranges of an edge.
 *
 */

#include "common.h"
#include "./cdt.h"

void
trimesh_error_report(struct ON_Brep_CDT_State *s_cdt, int valid_fcnt, int UNUSED(valid_vcnt), int *valid_faces, fastf_t *valid_vertices, struct bg_trimesh_solid_errors *se)
{
    if (se->degenerate.count > 0) {
	std::set<int> problem_pnts;
	std::set<int>::iterator pp_it;
	bu_log("%d degenerate faces\n", se->degenerate.count);
	for (int i = 0; i < se->degenerate.count; i++) {
	    int face = se->degenerate.faces[i];
	    bu_log("dface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
		    valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
		    valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
		    valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
	    problem_pnts.insert(valid_faces[face*3]);
	    problem_pnts.insert(valid_faces[face*3+1]);
	    problem_pnts.insert(valid_faces[face*3+2]);
	}
	for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
	    int pind = (*pp_it);
	    ON_3dPoint *p = (*s_cdt->bot_pnt_to_on_pnt)[pind];
	    if (!p) {
		bu_log("unmapped point??? %d\n", pind);
	    } else {
		struct cdt_audit_info *paudit = (*s_cdt->pnt_audit_info)[p];
		if (!paudit) {
		    bu_log("point with no audit info??? %d\n", pind);
		} else {
		    bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		}
	    }
	}
    }
    if (se->excess.count > 0) {
	bu_log("extra edges???\n");
    }
    if (se->unmatched.count > 0) {
	std::set<int> problem_pnts;
	std::set<int>::iterator pp_it;

	bu_log("%d unmatched edges\n", se->unmatched.count);
	for (int i = 0; i < se->unmatched.count; i++) {
	    int v1 = se->unmatched.edges[i*2];
	    int v2 = se->unmatched.edges[i*2+1];
	    bu_log("%d->%d: %f %f %f->%f %f %f\n", v1, v2,
		    valid_vertices[v1*3], valid_vertices[v1*3+1], valid_vertices[v1*3+2],
		    valid_vertices[v2*3], valid_vertices[v2*3+1], valid_vertices[v2*3+2]
		  );
	    for (int j = 0; j < valid_fcnt; j++) {
		std::set<std::pair<int,int>> fedges;
		fedges.insert(std::pair<int,int>(valid_faces[j*3],valid_faces[j*3+1]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+1],valid_faces[j*3+2]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+2],valid_faces[j*3]));
		int has_edge = (fedges.find(std::pair<int,int>(v1,v2)) != fedges.end()) ? 1 : 0;
		if (has_edge) {
		    int face = j;
		    bu_log("eface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
			    valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
			    valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
			    valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
		    problem_pnts.insert(valid_faces[j*3]);
		    problem_pnts.insert(valid_faces[j*3+1]);
		    problem_pnts.insert(valid_faces[j*3+2]);
		}
	    }
	}
	for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
	    int pind = (*pp_it);
	    ON_3dPoint *p = (*s_cdt->bot_pnt_to_on_pnt)[pind];
	    if (!p) {
		bu_log("unmapped point??? %d\n", pind);
	    } else {
		struct cdt_audit_info *paudit = (*s_cdt->pnt_audit_info)[p];
		if (!paudit) {
		    bu_log("point with no audit info??? %d\n", pind);
		} else {
		    bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		}
	    }
	}
    }
    if (se->misoriented.count > 0) {
	std::set<int> problem_pnts;
	std::set<int>::iterator pp_it;

	bu_log("%d misoriented edges\n", se->misoriented.count);
	for (int i = 0; i < se->misoriented.count; i++) {
	    int v1 = se->misoriented.edges[i*2];
	    int v2 = se->misoriented.edges[i*2+1];
	    bu_log("%d->%d: %f %f %f->%f %f %f\n", v1, v2,
		    valid_vertices[v1*3], valid_vertices[v1*3+1], valid_vertices[v1*3+2],
		    valid_vertices[v2*3], valid_vertices[v2*3+1], valid_vertices[v2*3+2]
		  );
	    for (int j = 0; j < valid_fcnt; j++) {
		std::set<std::pair<int,int>> fedges;
		fedges.insert(std::pair<int,int>(valid_faces[j*3],valid_faces[j*3+1]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+1],valid_faces[j*3+2]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+2],valid_faces[j*3]));
		int has_edge = (fedges.find(std::pair<int,int>(v1,v2)) != fedges.end()) ? 1 : 0;
		if (has_edge) {
		    int face = j;
		    bu_log("eface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
			    valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
			    valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
			    valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
		    problem_pnts.insert(valid_faces[j*3]);
		    problem_pnts.insert(valid_faces[j*3+1]);
		    problem_pnts.insert(valid_faces[j*3+2]);
		}
	    }
	}
	for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
	    int pind = (*pp_it);
	    ON_3dPoint *p = (*s_cdt->bot_pnt_to_on_pnt)[pind];
	    if (!p) {
		bu_log("unmapped point??? %d\n", pind);
	    } else {
		struct cdt_audit_info *paudit = (*s_cdt->pnt_audit_info)[p];
		if (!paudit) {
		    bu_log("point with no audit info??? %d\n", pind);
		} else {
		    bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		}
	    }
	}
    }

}

/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

