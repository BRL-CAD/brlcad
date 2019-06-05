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
    fcdt->rt_trims = new ON_RTree;
    fcdt->on_surf_points = new std::set<ON_2dPoint *>;

    fcdt->strim_pnts = new std::map<int,ON_3dPoint *>;
    fcdt->strim_norms = new std::map<int,ON_3dPoint *>;

    fcdt->on2_to_on3_map = new std::map<ON_2dPoint *, ON_3dPoint *>;
    fcdt->on2_to_p2t_map = new std::map<ON_2dPoint *, p2t::Point *>;
    fcdt->p2t_to_on2_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->p2t_to_on3_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->p2t_to_on3_norm_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->on3_to_tri_map = new std::map<ON_3dPoint *, std::set<p2t::Point *>>;

    fcdt->cdt = NULL;
    fcdt->p2t_extra_faces = new std::vector<p2t::Triangle *>;

    fcdt->degen_pnts = new std::set<p2t::Point *>;

    /* Mesh data */
    fcdt->tris_degen = new std::set<p2t::Triangle*>;
    fcdt->tris_zero_3D_area = new std::set<p2t::Triangle*>;
    fcdt->e2f = new EdgeToTri;
    fcdt->ecnt = new std::map<Edge, int>;

    return fcdt;
}

/* Clears old triangulation data */
void
ON_Brep_CDT_Face_Reset(struct ON_Brep_CDT_Face_State *fcdt)
{
    fcdt->p2t_to_trimpt->clear();
    fcdt->p2t_trim_ind->clear();
    fcdt->on_surf_points->clear();

    // I think this is cleanup code for the tree?
    ON_SimpleArray<void*> results;
    ON_BoundingBox bb = fcdt->rt_trims->BoundingBox();
    fcdt->rt_trims->Search2d((const double *) bb.m_min, (const double *) bb.m_max, results);
    if (results.Count() > 0) {
	for (int ri = 0; ri < results.Count(); ri++) {
	    const ON_Line *l = (const ON_Line *)*results.At(ri);
	    delete l;
	}
    }
    fcdt->rt_trims->RemoveAll();
    delete fcdt->rt_trims;
    fcdt->rt_trims = new ON_RTree;


    fcdt->on2_to_p2t_map->clear();
    fcdt->p2t_to_on2_map->clear();
    fcdt->p2t_to_on3_map->clear();
    fcdt->p2t_to_on3_norm_map->clear();
    fcdt->on3_to_tri_map->clear();

    /* Mesh data */
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
    fcdt->tris_degen->clear();
    fcdt->tris_zero_3D_area->clear();
    fcdt->e2f->clear();
    fcdt->ecnt->clear();
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

    // I think this is cleanup code for the tree?
    ON_SimpleArray<void*> results;
    ON_BoundingBox bb = fcdt->rt_trims->BoundingBox();
    fcdt->rt_trims->Search2d((const double *) bb.m_min, (const double *) bb.m_max, results);
    if (results.Count() > 0) {
	for (int ri = 0; ri < results.Count(); ri++) {
	    const ON_Line *l = (const ON_Line *)*results.At(ri);
	    delete l;
	}
    }
    fcdt->rt_trims->RemoveAll();

    delete fcdt->rt_trims;

    delete fcdt->on_surf_points;
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
    delete fcdt->degen_pnts;

    delete fcdt->tris_degen;
    delete fcdt->tris_zero_3D_area;
    delete fcdt->e2f;
    delete fcdt->ecnt;

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


    cdt->vfpnts = new std::vector<ON_3dPoint *>;
    cdt->vfnormals = new std::vector<ON_3dPoint *>;
    cdt->on_pnt_to_bot_pnt = new std::map<ON_3dPoint *, int>;
    cdt->on_pnt_to_bot_norm = new std::map<ON_3dPoint *, int>;
    cdt->tri_brep_face = new std::map<p2t::Triangle*, int>;

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

    delete s_cdt->vfpnts;
    delete s_cdt->vfnormals;
    delete s_cdt->on_pnt_to_bot_pnt;
    delete s_cdt->on_pnt_to_bot_norm;
    delete s_cdt->tri_brep_face;

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
populate_3d_pnts(struct ON_Brep_CDT_Face_State *f)
{
    int face_index = f->ind;
    ON_BrepFace &face = f->s_cdt->brep->m_F[face_index];
    p2t::CDT *cdt = f->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	(*f->s_cdt->tri_brep_face)[t] = face_index;
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
			    CDT_Add3DPnt(f->s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
			    f->s_cdt->vfpnts->push_back(op);
			    (*pointmap)[p] = op;
			    (*f->s_cdt->on_pnt_to_bot_pnt)[op] = f->s_cdt->vfpnts->size() - 1;
			    (*f->s_cdt->vert_to_on)[f->s_cdt->vfpnts->size() - 1] = op;
			}
			if (!onorm) {
			    onorm = new ON_3dPoint(norm);
			    f->s_cdt->w3dnorms->push_back(onorm);
			    f->s_cdt->vfnormals->push_back(onorm);
			    (*normalmap)[p] = onorm;
			    (*f->s_cdt->on_pnt_to_bot_norm)[op] = f->s_cdt->vfnormals->size() - 1;
			}
		    } else {
			bu_log("Erm... eval failed, no normal info?\n");
			if (!op) {
			    pnt = s->PointAt(p->x, p->y);
			    op = new ON_3dPoint(pnt);
			    CDT_Add3DPnt(f->s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
			    f->s_cdt->vfpnts->push_back(op);
			    (*pointmap)[p] = op;
			    (*f->s_cdt->on_pnt_to_bot_pnt)[op] = f->s_cdt->vfpnts->size() - 1;
			    (*f->s_cdt->vert_to_on)[f->s_cdt->vfpnts->size() - 1] = op;
			} else {
			    if (f->s_cdt->on_pnt_to_bot_pnt->find(op) == f->s_cdt->on_pnt_to_bot_pnt->end()) {
				f->s_cdt->vfpnts->push_back(op);
				(*pointmap)[p] = op;
				(*f->s_cdt->on_pnt_to_bot_pnt)[op] = f->s_cdt->vfpnts->size() - 1;
				(*f->s_cdt->vert_to_on)[f->s_cdt->vfpnts->size() - 1] = op;
			    }
			}
		    }
		} else {
		    /* We've got them already, just add them */
		    if (f->s_cdt->on_pnt_to_bot_pnt->find(op) == f->s_cdt->on_pnt_to_bot_pnt->end()) {
			f->s_cdt->vfpnts->push_back(op);
			f->s_cdt->vfnormals->push_back(onorm);
			(*f->s_cdt->on_pnt_to_bot_pnt)[op] = f->s_cdt->vfpnts->size() - 1;
			(*f->s_cdt->vert_to_on)[f->s_cdt->vfpnts->size() - 1] = op;
			(*f->s_cdt->on_pnt_to_bot_norm)[op] = f->s_cdt->vfnormals->size() - 1;
		    }
		}
	    } else {
		bu_log("Face %d: p2t face without proper point info...\n", face.m_face_index);
	    }
	}
    }
}

bool
build_poly2tri_polylines(struct ON_Brep_CDT_Face_State *f)
{
    // Process through loops, building Poly2Tri polygons for facetization.
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<ON_3dPoint *, std::set<p2t::Point *>> *on3_to_tri = f->on3_to_tri_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    std::vector<p2t::Point*> polyline;
    ON_BrepFace &face = f->s_cdt->brep->m_F[f->ind];
    int loop_cnt = face.LoopCount();
    ON_SimpleArray<BrepTrimPoint> *brep_loop_points = f->face_loop_points;
    bool outer = true;
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 2) {
	    for (int i = 1; i < num_loop_points; i++) {
		// map point to last entry to 3d point
		p2t::Point *p = new p2t::Point((brep_loop_points[li])[i].p2d.x, (brep_loop_points[li])[i].p2d.y);
		polyline.push_back(p);
		(*f->p2t_trim_ind)[p] = (brep_loop_points[li])[i].trim_ind;
		(*pointmap)[p] = (brep_loop_points[li])[i].p3d;
		(*on3_to_tri)[(brep_loop_points[li])[i].p3d].insert(p);
		(*normalmap)[p] = (brep_loop_points[li])[i].n3d;
	    }
	    for (int i = 1; i < brep_loop_points[li].Count(); i++) {
		// map point to last entry to 3d point
		ON_Line *line = new ON_Line((brep_loop_points[li])[i - 1].p2d, (brep_loop_points[li])[i].p2d);
		ON_BoundingBox bb = line->BoundingBox();

		bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
		bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
		bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
		bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
		bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
		bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

		f->rt_trims->Insert2d(bb.Min(), bb.Max(), line);
	    }
	    if (outer) {
		if (f->cdt) {
		    ON_Brep_CDT_Face_Reset(f);
		}
		f->cdt = new p2t::CDT(polyline);
		outer = false;
	    } else {
		f->cdt->AddHole(polyline);
	    }
	    polyline.clear();
	}
    }
    return outer;
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

