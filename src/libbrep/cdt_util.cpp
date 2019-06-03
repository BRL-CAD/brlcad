/*                        C D T _ U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @file cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include "./cdt.h"

/***************************************************
 * debugging routines
 ***************************************************/

void
plot_polyline(std::vector<p2t::Point *> *pnts, const char *filename)
{
    FILE *plot = fopen(filename, "w");
    int r = 255; int g = 0; int b = 0;
    point_t pc;
    pl_color(plot, r, g, b); 
    for (size_t i = 0; i < pnts->size(); i++) {
	p2t::Point *pt = (*pnts)[i];
	VSET(pc, pt->x, pt->y, 0);
	if (i == 0) {
	    pdv_3move(plot, pc);
	}
	pdv_3cont(plot, pc);
    }
    fclose(plot);
}


void
plot_tri(p2t::Triangle *t, const char *filename)
{
    FILE *plot = fopen(filename, "w");
    int r = 0; int g = 255; int b = 0;
    point_t pc;
    pl_color(plot, r, g, b); 
    for (size_t i = 0; i < 3; i++) {
	p2t::Point *pt = t->GetPoint(i);
	VSET(pc, pt->x, pt->y, 0);
	if (i == 0) {
	    pdv_3move(plot, pc);
	}
	pdv_3cont(plot, pc);
    }
    fclose(plot);
}

struct cdt_audit_info *
cdt_ainfo(int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d)
{
    struct cdt_audit_info *a = new struct cdt_audit_info;
    a->face_index = fid;
    a->vert_index = vid;
    a->trim_index = tid;
    a->edge_index = eid;
    a->surf_uv = ON_2dPoint(x2d, y2d);
    return a;
}


static Edge
mk_edge(ON_3dPoint *pt_A, ON_3dPoint *pt_B)
{
    if (pt_A <= pt_B) {
	return std::make_pair(pt_A, pt_B);
    } else {
	return std::make_pair(pt_B, pt_A);
    }
}

void
add_tri_edges(EdgeToTri *e2f, p2t::Triangle *t,
    std::map<p2t::Point *, ON_3dPoint *> *pointmap)
{
    ON_3dPoint *pt_A, *pt_B, *pt_C;

    p2t::Point *p2_A = t->GetPoint(0);
    p2t::Point *p2_B = t->GetPoint(1);
    p2t::Point *p2_C = t->GetPoint(2);
    pt_A = (*pointmap)[p2_A];
    pt_B = (*pointmap)[p2_B];
    pt_C = (*pointmap)[p2_C];
    (*e2f)[(mk_edge(pt_A, pt_B))].insert(t);
    (*e2f)[(mk_edge(pt_B, pt_C))].insert(t);
    (*e2f)[(mk_edge(pt_C, pt_A))].insert(t);
}


void
CDT_Add3DPnt(struct ON_Brep_CDT_State *s, ON_3dPoint *p, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d)
{
    s->w3dpnts->push_back(p);
    (*s->pnt_audit_info)[p] = cdt_ainfo(fid, vid, tid, eid, x2d, y2d);
}

// Digest tessellation tolerances...
void
CDT_Tol_Set(struct brep_cdt_tol *cdt, double dist, fastf_t md, double t_abs, double t_rel, double t_norm, double t_dist)
{
    fastf_t min_dist, max_dist, within_dist, cos_within_ang;

    max_dist = md;

    if (t_abs < t_dist + ON_ZERO_TOLERANCE) {
	min_dist = t_dist;
    } else {
	min_dist = t_abs;
    }

    double rel = 0.0;
    if (t_rel > 0.0 + ON_ZERO_TOLERANCE) {
	rel = t_rel * dist;
	// TODO - think about what we want maximum distance to mean and how it
	// relates to other tessellation settings...
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else if (t_abs > 0.0 + ON_ZERO_TOLERANCE) {
	within_dist = min_dist;
    } else {
	within_dist = 0.01 * dist; // default to 1% of dist
    }

    if (t_norm > 0.0 + ON_ZERO_TOLERANCE) {
	cos_within_ang = cos(t_norm);
    } else {
	cos_within_ang = cos(ON_PI / 2.0);
    }

    cdt->min_dist = min_dist;
    cdt->max_dist = max_dist;
    cdt->within_dist = within_dist;
    cdt->cos_within_ang = cos_within_ang;
}

struct ON_Brep_CDT_Face_State *
ON_Brep_CDT_Face_Create(struct ON_Brep_CDT_State *s_cdt, int ind)
{
    struct ON_Brep_CDT_Face_State *fcdt = new struct ON_Brep_CDT_Face_State;

    fcdt->s_cdt = s_cdt;
    fcdt->ind = ind;

    fcdt->w3dpnts = new std::vector<ON_3dPoint *>;
    fcdt->w3dnorms = new std::vector<ON_3dPoint *>;

    fcdt->vert_face_norms = new std::map<int, std::set<ON_3dPoint *>>;
    fcdt->face_loop_points = NULL;
    fcdt->p2t_to_trimpt = new std::map<p2t::Point *, BrepTrimPoint *>;
    fcdt->p2t_trim_ind = new std::map<p2t::Point *, int>;

    fcdt->strim_pnts = new std::map<int,ON_3dPoint *>;
    fcdt->strim_norms = new std::map<int,ON_3dPoint *>;

    fcdt->on_surf_points = new ON_2dPointArray;

    fcdt->on2_to_on3_map = new std::map<ON_2dPoint *, ON_3dPoint *>;
    fcdt->on2_to_p2t_map = new std::map<ON_2dPoint *, p2t::Point *>;
    fcdt->p2t_to_on2_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->p2t_to_on3_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->p2t_to_on3_norm_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->on3_to_tri_map = new std::map<ON_3dPoint *, std::set<p2t::Point *>>;

    fcdt->cdt = NULL;
    fcdt->p2t_extra_faces = new std::vector<p2t::Triangle *>;
    fcdt->degen_faces = new std::set<p2t::Triangle *>;

    fcdt->degen_pnts = new std::set<p2t::Point *>;

    return fcdt;
}

/* Clears old triangulation data */
void
ON_Brep_CDT_Face_Reset(struct ON_Brep_CDT_Face_State *fcdt)
{
    fcdt->on2_to_p2t_map->clear();
    fcdt->p2t_to_on2_map->clear();
    fcdt->p2t_to_on3_map->clear();
    fcdt->p2t_to_on3_norm_map->clear();
    fcdt->on3_to_tri_map->clear();
    fcdt->p2t_to_trimpt->clear();
    fcdt->p2t_trim_ind->clear();

    if (fcdt->cdt) {
	delete fcdt->cdt;
	fcdt->cdt = NULL;
    }

    std::vector<p2t::Triangle *>::iterator trit;
    for (trit = fcdt->p2t_extra_faces->begin(); trit != fcdt->p2t_extra_faces->end(); trit++) {
	p2t::Triangle *t = *trit;
	delete t;
    }

    fcdt->p2t_extra_faces->clear();
    fcdt->degen_faces->clear();

}

void
ON_Brep_CDT_Face_Destroy(struct ON_Brep_CDT_Face_State *fcdt)
{
    for (size_t i = 0; i < fcdt->w3dpnts->size(); i++) {
	delete (*(fcdt->w3dpnts))[i];
    }
    for (size_t i = 0; i < fcdt->w3dnorms->size(); i++) {
	delete (*(fcdt->w3dnorms))[i];
    }

    std::vector<p2t::Triangle *>::iterator trit;
    for (trit = fcdt->p2t_extra_faces->begin(); trit != fcdt->p2t_extra_faces->end(); trit++) {
	p2t::Triangle *t = *trit;
	delete t;
    }

    delete fcdt->w3dpnts;
    delete fcdt->w3dnorms;
    delete fcdt->vert_face_norms;
    if (fcdt->face_loop_points) {
	delete fcdt->face_loop_points;
    }
    delete fcdt->p2t_to_trimpt;
    delete fcdt->strim_pnts;
    delete fcdt->strim_norms;
    delete fcdt->on_surf_points;
    delete fcdt->on2_to_on3_map;
    delete fcdt->on2_to_p2t_map;
    delete fcdt->p2t_to_on2_map;
    delete fcdt->p2t_to_on3_map;
    delete fcdt->p2t_to_on3_norm_map;
    delete fcdt->on3_to_tri_map;
    delete fcdt->p2t_extra_faces;
    delete fcdt->degen_faces;
    delete fcdt->degen_pnts;

    delete fcdt;
}


struct ON_Brep_CDT_State *
ON_Brep_CDT_Create(void *bv)
{
    struct ON_Brep_CDT_State *cdt = new struct ON_Brep_CDT_State;
 
    /* Set status to "never evaluated" */
    cdt->status = BREP_CDT_UNTESSELLATED;

    ON_Brep *brep = (ON_Brep *)bv;
    cdt->orig_brep = brep;
    cdt->brep = NULL;

    /* Set sane default tolerances.  May want to do
     * something better (perhaps brep dimension based...) */
    cdt->abs = BREP_CDT_DEFAULT_TOL_ABS;
    cdt->rel = BREP_CDT_DEFAULT_TOL_REL ;
    cdt->norm = BREP_CDT_DEFAULT_TOL_NORM ;
    cdt->dist = BREP_CDT_DEFAULT_TOL_DIST ;


    cdt->w3dpnts = new std::vector<ON_3dPoint *>;
    cdt->w3dnorms = new std::vector<ON_3dPoint *>;

    cdt->vert_pnts = new std::map<int, ON_3dPoint *>;
    cdt->vert_avg_norms = new std::map<int, ON_3dPoint *>;
    cdt->vert_to_on = new std::map<int, ON_3dPoint *>;

    cdt->edge_pnts = new std::set<ON_3dPoint *>;
    cdt->min_edge_seg_len = new std::map<int, double>;
    cdt->max_edge_seg_len = new std::map<int, double>;
    cdt->on_brep_edge_pnts = new std::map<ON_3dPoint *, std::set<BrepTrimPoint *>>;

    cdt->pnt_audit_info = new std::map<ON_3dPoint *, struct cdt_audit_info *>;

    cdt->faces = new std::map<int, struct ON_Brep_CDT_Face_State *>;

    return cdt;
}


void
ON_Brep_CDT_Destroy(struct ON_Brep_CDT_State *s_cdt)
{
    for (size_t i = 0; i < s_cdt->w3dpnts->size(); i++) {
	delete (*(s_cdt->w3dpnts))[i];
    }
    for (size_t i = 0; i < s_cdt->w3dnorms->size(); i++) {
	delete (*(s_cdt->w3dnorms))[i];
    }

    if (s_cdt->faces) {
	std::map<int, struct ON_Brep_CDT_Face_State *>::iterator f_it;
	for (f_it = s_cdt->faces->begin(); f_it != s_cdt->faces->end(); f_it++) {
	    struct ON_Brep_CDT_Face_State *f = f_it->second;
	    delete f;
	}
    }

    if (s_cdt->brep) {
	delete s_cdt->brep;
    }

    delete s_cdt->vert_pnts;
    delete s_cdt->vert_avg_norms;
    delete s_cdt->vert_to_on;

    delete s_cdt->edge_pnts;
    delete s_cdt->min_edge_seg_len;
    delete s_cdt->max_edge_seg_len;
    delete s_cdt->on_brep_edge_pnts;

    delete s_cdt->pnt_audit_info;
    delete s_cdt->faces;

    delete s_cdt;
}

int
ON_Brep_CDT_Status(struct ON_Brep_CDT_State *s_cdt)
{
    return s_cdt->status;
}


void *
ON_Brep_CDT_Brep(struct ON_Brep_CDT_State *s_cdt)
{
    return (void *)s_cdt->brep;
}

void
ON_Brep_CDT_Tol_Set(struct ON_Brep_CDT_State *s, const struct ON_Brep_CDT_Tols *t)
{
    if (!s) {
	return;
    }

    if (!t) {
	/* reset to defaults */
	s->abs = BREP_CDT_DEFAULT_TOL_ABS;
	s->rel = BREP_CDT_DEFAULT_TOL_REL ;
	s->norm = BREP_CDT_DEFAULT_TOL_NORM ;
	s->dist = BREP_CDT_DEFAULT_TOL_DIST ;
    }

    s->abs = t->abs;
    s->rel = t->rel;
    s->norm = t->norm;
    s->dist = t->dist;
}

void
ON_Brep_CDT_Tol_Get(struct ON_Brep_CDT_Tols *t, const struct ON_Brep_CDT_State *s)
{
    if (!t) {
	return;
    }

    if (!s) {
	/* set to defaults */
	t->abs = BREP_CDT_DEFAULT_TOL_ABS;
	t->rel = BREP_CDT_DEFAULT_TOL_REL ;
	t->norm = BREP_CDT_DEFAULT_TOL_NORM ;
	t->dist = BREP_CDT_DEFAULT_TOL_DIST ;
    }

    t->abs = s->abs;
    t->rel = s->rel;
    t->norm = s->norm;
    t->dist = s->dist;
}

static int
ON_Brep_CDT_VList_Face(
	struct bu_list *vhead,
	struct bu_list *vlfree,
	int face_index,
	int mode,
	const struct ON_Brep_CDT_State *s)
{
    point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    vect_t nv[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;

    p2t::CDT *cdt = (*s->faces)[face_index]->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = (*s->faces)[face_index]->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = (*s->faces)[face_index]->p2t_to_on3_norm_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();

    switch (mode) {
	case 0:
	    // 3D shaded triangles
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    ON_3dPoint *op = (*pointmap)[p];
		    ON_3dPoint *onorm = (*normalmap)[p];
		    VSET(pt[j], op->x, op->y, op->z);
		    VSET(nv[j], onorm->x, onorm->y, onorm->z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, nv[0], BN_VLIST_TRI_START);
		BN_ADD_VLIST(vlfree, vhead, nv[0], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_TRI_MOVE);
		BN_ADD_VLIST(vlfree, vhead, nv[1], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_TRI_DRAW);
		BN_ADD_VLIST(vlfree, vhead, nv[2], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_TRI_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_TRI_END);
		//bu_log("Face %d, Tri %zd: %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f\n", face_index, i, V3ARGS(pt[0]), V3ARGS(nv[0]), V3ARGS(pt[1]), V3ARGS(nv[1]), V3ARGS(pt[2]), V3ARGS(nv[2]));
	    }
	    break;
	case 1:
	    // 3D wireframe
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    ON_3dPoint *op = (*pointmap)[p];
		    VSET(pt[j], op->x, op->y, op->z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_DRAW);
	    }
	    break;
	case 2:
	    // 2D wireframe
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;

		for (size_t j = 0; j < 3; j++) {
		    if (j == 0) {
			p = t->GetPoint(2);
		    } else {
			p = t->GetPoint(j - 1);
		    }
		    pt1[0] = p->x;
		    pt1[1] = p->y;
		    pt1[2] = 0.0;
		    p = t->GetPoint(j);
		    pt2[0] = p->x;
		    pt2[1] = p->y;
		    pt2[2] = 0.0;
		    BN_ADD_VLIST(vlfree, vhead, pt1, BN_VLIST_LINE_MOVE);
		    BN_ADD_VLIST(vlfree, vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	    }
	    break;
	default:
	    return -1;
    }

    return 0;
}

int ON_Brep_CDT_VList(
	struct bn_vlblock *vbp,
	struct bu_list *vlfree,
	struct bu_color *c,
	int mode,
	const struct ON_Brep_CDT_State *s)
{
    int r, g, b;
    struct bu_list *vhead = NULL;
    int have_color = 0;

   if (UNLIKELY(!c) || mode < 0) {
       return -1;
   }

   have_color = bu_color_to_rgb_ints(c, &r, &g, &b);

   if (UNLIKELY(!have_color)) {
       return -1;
   }

   vhead = bn_vlblock_find(vbp, r, g, b);

   if (UNLIKELY(!vhead)) {
       return -1;
   }

   for (int i = 0; i < s->brep->m_F.Count(); i++) {
       if ((*s->faces)[i] && (*s->faces)[i]->cdt) {
	   (void)ON_Brep_CDT_VList_Face(vhead, vlfree, i, mode, s);
       }
   }

   return 0;
}


void
populate_3d_pnts(struct ON_Brep_CDT_State *s_cdt, struct on_brep_mesh_data *md, int face_index)
{
    ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    p2t::CDT *cdt = (*s_cdt->faces)[face_index]->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = (*s_cdt->faces)[face_index]->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = (*s_cdt->faces)[face_index]->p2t_to_on3_norm_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	md->tri_brep_face[t] = face_index;
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p = t->GetPoint(j);
	    if (p) {
		ON_3dPoint *op = (*pointmap)[p];
		ON_3dPoint *onorm = (*normalmap)[p];
		if (!op || !onorm) {
		    /* We've got some calculating to do... */
		    const ON_Surface *s = face.SurfaceOf();
		    ON_3dPoint pnt;
		    ON_3dVector norm;
		    if (surface_EvNormal(s, p->x, p->y, pnt, norm)) {
			if (face.m_bRev) {
			    norm = norm * -1.0;
			}
			if (!op) {
			    op = new ON_3dPoint(pnt);
			    CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
			    md->vfpnts.push_back(op);
			    (*pointmap)[p] = op;
			    md->on_pnt_to_bot_pnt[op] = md->vfpnts.size() - 1;
			    (*s_cdt->vert_to_on)[md->vfpnts.size() - 1] = op;
			}
			if (!onorm) {
			    onorm = new ON_3dPoint(norm);
			    s_cdt->w3dnorms->push_back(onorm);
			    md->vfnormals.push_back(onorm);
			    (*normalmap)[p] = onorm;
			    md->on_pnt_to_bot_norm[op] = md->vfnormals.size() - 1;
			}
		    } else {
			bu_log("Erm... eval failed, no normal info?\n");
			if (!op) {
			    pnt = s->PointAt(p->x, p->y);
			    op = new ON_3dPoint(pnt);
			    CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
			    md->vfpnts.push_back(op);
			    (*pointmap)[p] = op;
			    md->on_pnt_to_bot_pnt[op] = md->vfpnts.size() - 1;
			    (*s_cdt->vert_to_on)[md->vfpnts.size() - 1] = op;
			} else {
			    if (md->on_pnt_to_bot_pnt.find(op) == md->on_pnt_to_bot_pnt.end()) {
				md->vfpnts.push_back(op);
				(*pointmap)[p] = op;
				md->on_pnt_to_bot_pnt[op] = md->vfpnts.size() - 1;
				(*s_cdt->vert_to_on)[md->vfpnts.size() - 1] = op;
			    }
			}
		    }
		} else {
		    /* We've got them already, just add them */
		    if (md->on_pnt_to_bot_pnt.find(op) == md->on_pnt_to_bot_pnt.end()) {
			md->vfpnts.push_back(op);
			md->vfnormals.push_back(onorm);
			md->on_pnt_to_bot_pnt[op] = md->vfpnts.size() - 1;
			(*s_cdt->vert_to_on)[md->vfpnts.size() - 1] = op;
			md->on_pnt_to_bot_norm[op] = md->vfnormals.size() - 1;
		    }
		}
	    } else {
		bu_log("Face %d: p2t face without proper point info...\n", face.m_face_index);
	    }
	}
    }
}


void
triangles_build_edgemap(struct ON_Brep_CDT_State *s_cdt, struct on_brep_mesh_data *md, int face_index)
{
    p2t::CDT *cdt = (*s_cdt->faces)[face_index]->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = (*s_cdt->faces)[face_index]->p2t_to_on3_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	/* Make sure this face isn't degenerate */
	if (md->tris_degen.find(tris[i]) != md->tris_degen.end()) {
	    continue;
	}
	add_tri_edges(md->e2f, tris[i], pointmap);
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

