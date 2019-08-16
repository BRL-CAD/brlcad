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
    point_t pc_orig;
    pl_color(plot, r, g, b); 
    for (size_t i = 0; i < 3; i++) {
	p2t::Point *pt = t->GetPoint(i);
	VSET(pc, pt->x, pt->y, 0);
	if (i == 0) {
	    VSET(pc_orig, pt->x, pt->y, 0);
	    pdv_3move(plot, pc);
	}
	pdv_3cont(plot, pc);
    }
    pdv_3cont(plot, pc_orig);
    fclose(plot);
}

void
plot_tri_2d(p2t::Triangle *t, struct bu_color *c, FILE *plot)
{
    point_t pc;
    point_t pc_orig;
    point_t center = VINIT_ZERO;
    pl_color_buc(plot, c);

    for (size_t i = 0; i < 3; i++) {
	p2t::Point *pt = t->GetPoint(i);
	center[X] += pt->x;
	center[Y] += pt->y;
    }
    center[X] = center[X]/3.0;
    center[Y] = center[Y]/3.0;
    center[Z] = 0;

    for (size_t i = 0; i < 3; i++) {
	p2t::Point *pt = t->GetPoint(i);
	VSET(pc, pt->x, pt->y, 0);
	if (i == 0) {
	    VSET(pc_orig, pt->x, pt->y, 0);
	    pdv_3move(plot, pc);
	}
	pdv_3cont(plot, pc);
    }
    pdv_3cont(plot, pc_orig);

    /* fill in the "interior" using red */
    pl_color(plot, 255,0,0);
    for (size_t i = 0; i < 3; i++) {
	p2t::Point *pt = t->GetPoint(i);
	VSET(pc, pt->x, pt->y, 0);
	pdv_3move(plot, pc);
	pdv_3cont(plot, center);
    }
}

void
plot_trimesh_2d(std::vector<trimesh::triangle_t> &farray, const char *filename)
{
    std::set<trimesh::index_t>::iterator f_it;
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    for (size_t i = 0; i < farray.size(); i++) {
	p2t::Triangle *t = farray[i].t;
	plot_tri_2d(t, &c, plot_file);
    }
    fclose(plot_file);
}

void
plot_tri_3d(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap, struct bu_color *c, FILE *c_plot)
{
    point_t p1, p2, p3;
    ON_3dPoint *on1 = (*pointmap)[t->GetPoint(0)];
    ON_3dPoint *on2 = (*pointmap)[t->GetPoint(1)];
    ON_3dPoint *on3 = (*pointmap)[t->GetPoint(2)];
    p1[X] = on1->x;
    p1[Y] = on1->y;
    p1[Z] = on1->z;
    p2[X] = on2->x;
    p2[Y] = on2->y;
    p2[Z] = on2->z;
    p3[X] = on3->x;
    p3[Y] = on3->y;
    p3[Z] = on3->z;
    pl_color_buc(c_plot, c);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p2);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p3);
    pdv_3move(c_plot, p2);
    pdv_3cont(c_plot, p3);
}

struct cdt_audit_info *
cdt_ainfo(int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d, fastf_t px, fastf_t py, fastf_t pz)
{
    struct cdt_audit_info *a = new struct cdt_audit_info;
    a->face_index = fid;
    a->vert_index = vid;
    a->trim_index = tid;
    a->edge_index = eid;
    a->surf_uv = ON_2dPoint(x2d, y2d);
    a->vert_pnt = ON_3dPoint(px, py, pz);
    return a;
}

void
CDT_Add2DPnt(struct ON_Brep_CDT_Face_State *f, ON_2dPoint *p, int fid, int vid, int tid, int eid, fastf_t tparam)
{
    f->w2dpnts->push_back(p);
    (*f->pnt2d_audit_info)[p] = cdt_ainfo(fid, vid, tid, eid, tid, tparam, 0.0, 0.0, 0.0);
}

void
CDT_Add3DPnt(struct ON_Brep_CDT_State *s, ON_3dPoint *p, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d)
{
    s->w3dpnts->push_back(p);
    (*s->pnt_audit_info)[p] = cdt_ainfo(fid, vid, tid, eid, x2d, y2d, 0.0, 0.0, 0.0);
}

void
CDT_Add3DNorm(struct ON_Brep_CDT_State *s, ON_3dPoint *normal, ON_3dPoint *vert, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d)
{
    s->w3dnorms->push_back(normal);
    (*s->pnt_audit_info)[normal] = cdt_ainfo(fid, vid, tid, eid, x2d, y2d, vert->x, vert->y, vert->z);
}

// Digest tessellation tolerances...
void
CDT_Tol_Set(struct brep_cdt_tol *cdt, double dist, fastf_t md, double t_abs, double t_rel, double t_dist)
{
    fastf_t max_dist = md;
    fastf_t min_dist = (t_abs < t_dist + ON_ZERO_TOLERANCE) ? t_dist : t_abs;
    fastf_t within_dist = 0.01 * dist; // default to 1% of dist

    if (t_rel > 0.0 + ON_ZERO_TOLERANCE) {
	double rel = t_rel * dist;
	// TODO - think about what we want maximum distance to mean and how it
	// relates to other tessellation settings...
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else {
	if (t_abs > 0.0 + ON_ZERO_TOLERANCE) {
	    within_dist = min_dist;
	}
    }

    cdt->min_dist = min_dist;
    cdt->max_dist = max_dist;
    cdt->within_dist = within_dist;
}

ON_3dVector
p2tTri_Normal(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap)
{
    ON_3dPoint *p1 = (*pointmap)[t->GetPoint(0)];
    ON_3dPoint *p2 = (*pointmap)[t->GetPoint(1)];
    ON_3dPoint *p3 = (*pointmap)[t->GetPoint(2)];

    if (!p1 || !p2 || !p3) {
	bu_exit(1, "Fatal: 2D poly2tri point has no corresponding 3D point in the map\n");
    }

    ON_3dVector e1 = *p2 - *p1;
    ON_3dVector e2 = *p3 - *p1;
    ON_3dVector tdir = ON_CrossProduct(e1, e2);
    tdir.Unitize();
    return tdir;
}

ON_3dVector
p2tTri_Brep_Normal(struct ON_Brep_CDT_Face_State *f, p2t::Triangle *t)
{
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    bool m_bRev = f->s_cdt->brep->m_F[f->ind].m_bRev;
    ON_3dPoint avgnorm(0,0,0);

    double norm_cnt = 0.0;

    for (size_t i = 0; i < 3; i++) {
	ON_3dPoint *p3d = (*pointmap)[t->GetPoint(i)];
	ON_3dPoint onrm;
	ON_3dPoint *n1 = NULL;
	if (f->s_cdt->singular_vert_to_norms->find(p3d) != f->s_cdt->singular_vert_to_norms->end()) {
	    // singular vert norms are a product of multiple faces - not useful for this
	    continue;
	} else {
	    n1 = (*normalmap)[t->GetPoint(i)];
	    if (!n1) {
		bu_exit(1, "Fatal: 2D poly2tri point has no corresponding 3D normal in the map\n");
	    }
	    onrm = *n1;
	    if (m_bRev) {
		onrm = onrm * -1;
	    }
	}
	avgnorm = avgnorm + *n1;
	norm_cnt = norm_cnt + 1.0;
    }

    ON_3dVector anrm = avgnorm/norm_cnt;
    anrm.Unitize();
    return anrm;
}



struct ON_Brep_CDT_Face_State *
ON_Brep_CDT_Face_Create(struct ON_Brep_CDT_State *s_cdt, int ind)
{
    struct ON_Brep_CDT_Face_State *fcdt = new struct ON_Brep_CDT_Face_State;

    fcdt->face_root = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fcdt->face_root, "%s%d_", bu_vls_cstr(&s_cdt->brep_root), ind);

    fcdt->s_cdt = s_cdt;
    fcdt->ind = ind;
    fcdt->has_singular_trims = 0;
    fcdt->singularities = new std::set<ON_3dPoint *>;

    fcdt->w2dpnts = new std::vector<ON_2dPoint *>;
    fcdt->w3dpnts = new std::vector<ON_3dPoint *>;
    fcdt->w3dnorms = new std::vector<ON_3dPoint *>;

    fcdt->face_loop_points = NULL;
    fcdt->p2t_trim_ind = new std::map<p2t::Point *, int>;
    fcdt->rt_trims = new ON_RTree;
    fcdt->rt_trims_3d = new ON_RTree;
    fcdt->on_surf_points = new std::set<ON_2dPoint *>;

    fcdt->strim_pnts = new std::map<int,ON_3dPoint *>;
    fcdt->strim_norms = new std::map<int,ON_3dPoint *>;

    fcdt->on3_to_norm_map = new std::map<ON_3dPoint *, ON_3dPoint *>;

    fcdt->p2t_to_on2_map = new std::map<p2t::Point *, ON_2dPoint *>;
    fcdt->p2t_to_on3_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->p2t_to_on3_norm_map = new std::map<p2t::Point *, ON_3dPoint *>;
    fcdt->on3_to_tri_map = new std::map<ON_3dPoint *, std::set<p2t::Point *>>;

    fcdt->tris = new std::set<p2t::Triangle *>;
    fcdt->degen_tris = new std::set<p2t::Triangle *>;

    fcdt->degen_pnts = new std::set<p2t::Point *>;
    fcdt->ext_degen_pnts = new std::set<p2t::Point *>;

    return fcdt;
}

/* Clears old triangulation data */
void
ON_Brep_CDT_Face_Reset(struct ON_Brep_CDT_Face_State *fcdt, int full_surface_sample)
{
    fcdt->p2t_trim_ind->clear();

    if (full_surface_sample) {
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
    }
    fcdt->singularities->clear();

    fcdt->on3_to_norm_map->clear();
    fcdt->p2t_to_on2_map->clear();
    fcdt->p2t_to_on3_map->clear();
    fcdt->p2t_to_on3_norm_map->clear();
    fcdt->on3_to_tri_map->clear();

    /* Mesh data */
    std::set<p2t::Triangle *>::iterator ts_it;
    for (ts_it = fcdt->tris->begin(); ts_it != fcdt->tris->end(); ts_it++) {
	p2t::Triangle *t = *ts_it;
	delete t;
    }
    fcdt->tris->clear();
}

void
ON_Brep_CDT_Face_Destroy(struct ON_Brep_CDT_Face_State *fcdt)
{
    for (size_t i = 0; i < fcdt->w2dpnts->size(); i++) {
	delete (*(fcdt->w2dpnts))[i];
    }
    for (size_t i = 0; i < fcdt->w3dpnts->size(); i++) {
	delete (*(fcdt->w3dpnts))[i];
    }
    for (size_t i = 0; i < fcdt->w3dnorms->size(); i++) {
	delete (*(fcdt->w3dnorms))[i];
    }

    std::set<p2t::Triangle *>::iterator ts_it;
    for (ts_it = fcdt->tris->begin(); ts_it != fcdt->tris->end(); ts_it++) {
	p2t::Triangle *t = *ts_it;
	delete t;
    }
    delete fcdt->tris;

    delete fcdt->w2dpnts;
    delete fcdt->w3dpnts;
    delete fcdt->w3dnorms;
    if (fcdt->face_loop_points) {
	delete fcdt->face_loop_points;
    }

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
    delete fcdt->rt_trims_3d;

    delete fcdt->singularities;
    delete fcdt->on_surf_points;
    delete fcdt->strim_pnts;
    delete fcdt->strim_norms;
    delete fcdt->on_surf_points;
    delete fcdt->p2t_to_on2_map;
    delete fcdt->p2t_to_on3_map;
    delete fcdt->p2t_to_on3_norm_map;
    delete fcdt->on3_to_norm_map;
    delete fcdt->on3_to_tri_map;
    delete fcdt->degen_tris;
    delete fcdt->degen_pnts;
    delete fcdt->ext_degen_pnts;

    delete fcdt;
}


struct ON_Brep_CDT_State *
ON_Brep_CDT_Create(void *bv, const char *objname)
{
    struct ON_Brep_CDT_State *cdt = new struct ON_Brep_CDT_State;

    cdt->brep_root = BU_VLS_INIT_ZERO;
    if (objname) {
	bu_vls_sprintf(&cdt->brep_root, "%s_", objname);
    } else {
	bu_vls_trunc(&cdt->brep_root, 0);
    }

    /* Set status to "never evaluated" */
    cdt->status = BREP_CDT_UNTESSELLATED;

    ON_Brep *brep = (ON_Brep *)bv;
    cdt->orig_brep = brep;
    cdt->brep = NULL;

    /* By default, all tolerances are unset */
    cdt->tol.abs = -1;
    cdt->tol.rel = -1;
    cdt->tol.norm = -1;
    cdt->tol.absmax = -1;
    cdt->tol.absmin = -1;
    cdt->tol.relmax = -1;
    cdt->tol.relmin = -1;
    cdt->tol.rel_lmax = -1;
    cdt->tol.rel_lmin = -1;

    cdt->w3dpnts = new std::vector<ON_3dPoint *>;
    cdt->w3dnorms = new std::vector<ON_3dPoint *>;

    cdt->vert_pnts = new std::map<int, ON_3dPoint *>;
    cdt->vert_avg_norms = new std::map<int, ON_3dPoint *>;
    cdt->singular_vert_to_norms = new std::map<ON_3dPoint *, ON_3dPoint *>;

    cdt->edge_pnts = new std::set<ON_3dPoint *>;
    cdt->min_edge_seg_len = new std::map<int, double>;
    cdt->max_edge_seg_len = new std::map<int, double>;
    cdt->on_brep_edge_pnts = new std::map<ON_3dPoint *, std::set<BrepTrimPoint *>>;
    cdt->etrees = new std::map<int, struct BrepEdgeSegment *>;

    cdt->pnt_audit_info = new std::map<ON_3dPoint *, struct cdt_audit_info *>;

    cdt->faces = new std::map<int, struct ON_Brep_CDT_Face_State *>;

    cdt->tri_brep_face = new std::map<p2t::Triangle*, int>;
    cdt->bot_pnt_to_on_pnt = new std::map<int, ON_3dPoint *>;

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
    delete s_cdt->singular_vert_to_norms;

    delete s_cdt->edge_pnts;
    delete s_cdt->min_edge_seg_len;
    delete s_cdt->max_edge_seg_len;
    delete s_cdt->on_brep_edge_pnts;

    // TODO - free segment trees in etrees
    delete s_cdt->etrees;

    delete s_cdt->pnt_audit_info;
    delete s_cdt->faces;

    delete s_cdt->tri_brep_face;
    delete s_cdt->bot_pnt_to_on_pnt;

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

// Rules of precedence:
//
// 1.  absmax >= absmin
// 2.  absolute over relative
// 3.  relative local over relative global 
// 4.  normal (curvature)
void
cdt_tol_global_calc(struct ON_Brep_CDT_State *s)
{
    if (s->tol.abs < ON_ZERO_TOLERANCE && s->tol.rel < ON_ZERO_TOLERANCE && s->tol.norm < ON_ZERO_TOLERANCE &&
	    s->tol.absmax < ON_ZERO_TOLERANCE && s->tol.absmin < ON_ZERO_TOLERANCE && s->tol.relmax < ON_ZERO_TOLERANCE &&
	    s->tol.relmin < ON_ZERO_TOLERANCE && s->tol.rel_lmax < ON_ZERO_TOLERANCE && s->tol.rel_lmin < ON_ZERO_TOLERANCE) {
    }

    if (s->tol.abs > ON_ZERO_TOLERANCE) {
	s->absmax = s->tol.abs;
    }

    if (s->tol.absmax > ON_ZERO_TOLERANCE && s->tol.abs < ON_ZERO_TOLERANCE) {
	s->absmax = s->tol.absmax;
    }

    if (s->tol.absmin > ON_ZERO_TOLERANCE) {
	s->absmin = s->tol.absmin;
    }

    // If we don't have hard set limits globally, see if we have global relative
    // settings
    if (s->absmax < ON_ZERO_TOLERANCE || s->absmin < ON_ZERO_TOLERANCE) {

	/* If we've got nothing, set a default global relative tolerance */
	if (s->tol.rel < ON_ZERO_TOLERANCE && s->tol.relmax < ON_ZERO_TOLERANCE &&
		s->tol.relmin < ON_ZERO_TOLERANCE && s->tol.rel_lmax < ON_ZERO_TOLERANCE && s->tol.rel_lmin < ON_ZERO_TOLERANCE) {
	    s->tol.rel = BREP_CDT_DEFAULT_TOL_REL ;
	}

	// Need some bounding box information
	ON_BoundingBox bbox;
	if (!s->brep->GetTightBoundingBox(bbox)) {
	   bbox = s->brep->BoundingBox(); 
	}
	double len = bbox.Diagonal().Length();

	if (s->tol.rel > ON_ZERO_TOLERANCE) {
	    // Largest dimension, unless set by abs, is tol.rel * the bbox diagonal length
	    if ( s->tol.relmax < ON_ZERO_TOLERANCE) {
		s->absmax = len * s->tol.rel;
	    }

	    // Smallest dimension, unless otherwise set, is  0.01 * tol.rel * the bbox diagonal length.
	    if (s->tol.relmin < ON_ZERO_TOLERANCE) {
		s->absmin = len * s->tol.rel * 0.01;
	    }
	}

	if (s->absmax < ON_ZERO_TOLERANCE && s->tol.relmax > ON_ZERO_TOLERANCE) {
	    s->absmax = len * s->tol.relmax;
	}

	if (s->absmin < ON_ZERO_TOLERANCE && s->tol.relmin > ON_ZERO_TOLERANCE) {
	    s->absmin = len * s->tol.relmin;
	}

    }

    // Sanity
    s->absmax = (s->absmin > s->absmax) ? s->absmin : s->absmax;
    s->absmin = (s->absmax < s->absmin) ? s->absmax : s->absmin;

    // Get the normal based parameter as well
    if (s->tol.norm > ON_ZERO_TOLERANCE) {
	s->cos_within_ang = cos(s->tol.norm);
    } else {
	s->cos_within_ang = cos(ON_PI / 2.0);
    }
}

void
ON_Brep_CDT_Tol_Set(struct ON_Brep_CDT_State *s, const struct bg_tess_tol *t)
{
    if (!s) {
	return;
    }

    if (!t) {
	/* reset to defaults */
	s->tol.abs = -1;
	s->tol.rel = -1;
	s->tol.norm = -1;
	s->tol.absmax = -1;
	s->tol.absmin = -1;
	s->tol.relmax = -1;
	s->tol.relmin = -1;
	s->tol.rel_lmax = -1;
	s->tol.rel_lmin = -1;
	s->absmax = 0;
	s->absmin = 0;
	s->cos_within_ang = 0;
	if (s->brep) {
	    cdt_tol_global_calc(s);
	}
	return;
    }

    s->tol = *t;
    s->absmax = 0;
    s->absmin = 0;
    s->cos_within_ang = 0;
    if (s->brep) {
	cdt_tol_global_calc(s);
    }
}

void
ON_Brep_CDT_Tol_Get(struct bg_tess_tol *t, const struct ON_Brep_CDT_State *s)
{
    if (!t) {
	return;
    }

    if (!s) {
	/* set to defaults */
	t->abs = -1;
	t->rel = -1;
	t->norm = -1;
    	t->absmax = -1;
	t->absmin = -1;
	t->relmax = -1;
	t->relmin = -1;
	t->rel_lmax = -1;
	t->rel_lmin = -1;
    }

    *t = s->tol;
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
#if 0
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
#endif

    struct ON_Brep_CDT_Face_State *f = (*s->faces)[face_index];
    std::set<cdt_mesh::triangle_t>::iterator tr_it;
    std::set<cdt_mesh::triangle_t> tris = f->fmesh.tris;
    std::map<ON_3dPoint *, ON_3dPoint *> *normalmap = f->on3_to_norm_map;

    switch (mode) {
	case 0:
	    // 3D shaded triangles
	    for (tr_it = tris.begin(); tr_it != tris.end(); tr_it++) {
		for (size_t j = 0; j < 3; j++) {
		    ON_3dPoint *p3d = f->fmesh.pnts[(*tr_it).v[j]];
		    ON_3dPoint *onorm = NULL;
		    if (f->s_cdt->singular_vert_to_norms->find(p3d) != f->s_cdt->singular_vert_to_norms->end()) {
			// Use calculated normal for singularity points
			onorm = (*f->s_cdt->singular_vert_to_norms)[p3d];
		    } else {
			onorm = (*normalmap)[p3d];
		    }
		    VSET(pt[j], p3d->x, p3d->y, p3d->z);
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
	    for (tr_it = tris.begin(); tr_it != tris.end(); tr_it++) {
		for (size_t j = 0; j < 3; j++) {
		    ON_3dPoint *p3d = f->fmesh.pnts[(*tr_it).v[j]];
		    VSET(pt[j], p3d->x, p3d->y, p3d->z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_DRAW);
	    }
	    break;
	case 2:
#if 0
	    // 2D wireframe
	    for (tr_it = tris.begin(); tr_it != tris.end(); tr_it++) {
		p2t::Triangle *t = *tr_it;
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
#endif
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
       if ((*s->faces)[i] && (*s->faces)[i]->tris->size()) {
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
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    std::map<ON_3dPoint *, ON_3dPoint *> *nmap = f->on3_to_norm_map;
    std::set<p2t::Triangle *>::iterator tr_it;
    std::set<p2t::Triangle *> *tris = f->tris;
    for (tr_it = tris->begin(); tr_it != tris->end(); tr_it++) {
	p2t::Triangle *t = *tr_it;
	(*f->s_cdt->tri_brep_face)[t] = face_index;
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p = t->GetPoint(j);
	    if (p) {
		ON_3dPoint *op = (*pointmap)[p];
		ON_3dPoint *onorm = (*normalmap)[p];
		if (!op || !onorm) {
		    /* Don't know the point, normal or both.  Ask the Surface */
		    const ON_Surface *s = face.SurfaceOf();
		    ON_3dPoint pnt;
		    ON_3dVector norm;
		    bool surf_ev = surface_EvNormal(s, p->x, p->y, pnt, norm);
		    if (!surf_ev && !op) {
			// Couldn't get point and normal, go with just point
			pnt = s->PointAt(p->x, p->y);
		    }
		    if (!op) {
			op = new ON_3dPoint(pnt);
			CDT_Add3DPnt(f->s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
		    }
		    (*pointmap)[p] = op;

		    // We can only supply a normal if we either a) already had
		    // it or b) got it from surface_EvNormal - we don't have
		    // any PointAt style fallback
		    if (surf_ev && !onorm) {
			if (face.m_bRev) {
			    norm = norm * -1.0;
			}
			onorm = new ON_3dPoint(norm);
			CDT_Add3DNorm(f->s_cdt, onorm, op, face.m_face_index, -1, -1, -1, p->x, p->y);
		    }
		    if (onorm) {
			(*normalmap)[p] = onorm;
			(*nmap)[op] = onorm;
		    }
		}
	    } else {
		bu_log("Face %d: p2t face without proper point info...\n", face.m_face_index);
	    }
	}
    }
}

struct trimesh_info *
CDT_Face_Build_Halfedge(std::set<p2t::Triangle*> *triangles)
{
    struct trimesh_info *tm = new struct trimesh_info;
    std::set<p2t::Triangle*>::iterator s_it;

    // Assemble the set of unique 2D poly2tri points
    for (s_it = triangles->begin(); s_it != triangles->end(); s_it++) {
	p2t::Triangle *t = *s_it;
	for (size_t j = 0; j < 3; j++) {
	    tm->uniq_p2d.insert(t->GetPoint(j));
	}
    }

    // Assign all poly2tri 2D points a trimesh index
    std::set<p2t::Point *>::iterator u_it;
    {
	trimesh::index_t ind = 0;
	for (u_it = tm->uniq_p2d.begin(); u_it != tm->uniq_p2d.end(); u_it++) {
	    tm->p2dind[*u_it] = ind;
	    tm->ind2p2d[ind] = *u_it;
	    ind++;
	}
    }

    // Assemble the faces array
    for (s_it = triangles->begin(); s_it != triangles->end(); s_it++) {
	p2t::Triangle *t = *s_it;

	trimesh::triangle_t tmt;
	for (size_t j = 0; j < 3; j++) {
	    tmt.v[j] = tm->p2dind[t->GetPoint(j)];
	}
	tmt.t = t;
	tm->triangles.push_back(tmt);
	tm->t2ind[t] = tm->triangles.size()-1;
    }

    // Build the mesh topology information
    trimesh::unordered_edges_from_triangles(tm->triangles.size(), &tm->triangles[0], tm->edges);
    tm->mesh.build(tm->uniq_p2d.size(), tm->triangles.size(), &tm->triangles[0], tm->edges.size(), &tm->edges[0]);

    return tm;
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

