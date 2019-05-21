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
    s->pnt_audit_info[p] = cdt_ainfo(fid, vid, tid, eid, x2d, y2d);
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

struct ON_Brep_CDT_State *
ON_Brep_CDT_Create(void *bv)
{
    ON_Brep *brep = (ON_Brep *)bv;
    struct ON_Brep_CDT_State *cdt = new struct ON_Brep_CDT_State;
    cdt->brep = brep;

    cdt->w3dpnts = new std::vector<ON_3dPoint *>;
    cdt->vert_pnts = new std::map<int, ON_3dPoint *>;
    cdt->w3dnorms = new std::vector<ON_3dPoint *>;
    cdt->vert_norms = new std::map<int, ON_3dPoint *>;
    cdt->strim_pnts = new std::map<int,std::map<int, ON_3dPoint *> >;
    cdt->strim_norms = new std::map<int,std::map<int, ON_3dPoint *> >;
    cdt->vert_to_on = new std::map<int, ON_3dPoint *>;
    cdt->edge_pnts = new std::set<ON_3dPoint *>;
    cdt->brep_face_loop_points = (ON_SimpleArray<BrepTrimPoint> **)bu_calloc(brep->m_F.Count(), sizeof(std::map<p2t::Point *, BrepTrimPoint *> *), "face loop pnts");
    cdt->face_degen_pnts = (std::set<p2t::Point *> **)bu_calloc(brep->m_F.Count(), sizeof(std::set<p2t::Point *> *), "degenerate edge points");

    cdt->p2t_faces = (p2t::CDT **)bu_calloc(brep->m_F.Count(), sizeof(p2t::CDT *), "poly2tri triangulations");
    cdt->p2t_extra_faces = (std::vector<p2t::Triangle *> **)bu_calloc(brep->m_F.Count(), sizeof(std::vector<p2t::Triangle *> *), "extra p2t faces");
    cdt->on2_to_on3_maps = (std::map<ON_2dPoint *, ON_3dPoint *> **)bu_calloc(brep->m_F.Count(), sizeof(std::map<ON_2dPoint *, ON_3dPoint *> *), "ON_2dPoint to ON_3dPoint maps");
    cdt->tri_to_on3_maps = (std::map<p2t::Point *, ON_3dPoint *> **)bu_calloc(brep->m_F.Count(), sizeof(std::map<p2t::Point *, ON_3dPoint *> *), "poly2tri point to ON_3dPoint maps");
    cdt->on3_to_tri_maps = (std::map<ON_3dPoint *, std::set<p2t::Point *>> **)bu_calloc(brep->m_F.Count(), sizeof(std::map<ON_3dPoint *, std::set<p2t::Point *>> *), "poly2tri point to ON_3dPoint maps");
    cdt->tri_to_on3_norm_maps = (std::map<p2t::Point *, ON_3dPoint *> **)bu_calloc(brep->m_F.Count(), sizeof(std::map<p2t::Point *, ON_3dPoint *> *), "poly2tri point to ON_3dVector normal maps");

    /* Set status to "never evaluated" */
    cdt->status = BREP_CDT_UNTESSELLATED;

    /* Set sane default tolerances.  May want to do
     * something better (perhaps brep dimension based...) */
    cdt->abs = BREP_CDT_DEFAULT_TOL_ABS;
    cdt->rel = BREP_CDT_DEFAULT_TOL_REL ;
    cdt->norm = BREP_CDT_DEFAULT_TOL_NORM ;
    cdt->dist = BREP_CDT_DEFAULT_TOL_DIST ;

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

    for (int i = 0; i < s_cdt->brep->m_F.Count(); i++) {
	std::vector<p2t::Triangle *> *ef = s_cdt->p2t_extra_faces[i];
	if (ef) {
	    std::vector<p2t::Triangle *>::iterator trit;
	    for (trit = ef->begin(); trit != ef->end(); trit++) {
		p2t::Triangle *t = *trit;
		delete t;
	    }
	}
    }

    for (int i = 0; i < s_cdt->brep->m_F.Count(); i++) {
	if (s_cdt->brep_face_loop_points[i] != NULL) {
	    delete [] s_cdt->brep_face_loop_points[i];
	}
	if (s_cdt->face_degen_pnts[i] != NULL) {
	    delete s_cdt->face_degen_pnts[i];
	}
    }

    for (int i = 0; i < s_cdt->brep->m_F.Count(); i++) {
	if (s_cdt->on2_to_on3_maps[i] != NULL) {
	    delete s_cdt->on2_to_on3_maps[i];
	}
    }
    bu_free(s_cdt->on2_to_on3_maps, "degen pnts");

    delete s_cdt->w3dpnts;
    delete s_cdt->vert_pnts;
    delete s_cdt->w3dnorms;
    delete s_cdt->vert_norms;
    delete s_cdt->strim_pnts;
    delete s_cdt->strim_norms;
    delete s_cdt->vert_to_on;
    delete s_cdt->edge_pnts;
    delete s_cdt->p2t_extra_faces;

    bu_free(s_cdt->brep_face_loop_points, "flp array");
    bu_free(s_cdt->face_degen_pnts, "degen pnts");
    // TODO - delete p2t data

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



/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

