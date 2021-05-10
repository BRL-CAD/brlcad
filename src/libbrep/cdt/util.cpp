/*                        C D T _ U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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
#include "bu/str.h"
#include "bg/tri_ray.h"
#include "./cdt.h"

/***************************************************
 * debugging routines
 ***************************************************/

void
on_bb_plot(const char *fname, ON_BoundingBox &bb) {
    FILE *plot = fopen (fname, "w");
    ON_BoundingBox_Plot(plot, bb);
    fclose(plot);
}

//#define PPOINT 23.75000091628566068,7.56415387307229370,4.69184016593825515
#define PPOINT 23.249999993258228,8.311619640259547,5.3252579308096335
bool
PPCHECK(ON_3dPoint &p)
{
    ON_3dPoint problem(PPOINT);
    if (problem.DistanceTo(p) < 0.01) {
	std::cout << "Saw problem point\n";
	return true;
    }
    return false;
}

#if 0
bool
VPCHECK(overt_t *v, const char *fname)
{
    if (fname && !BU_STR_EQUAL(v->omesh->fmesh->name, fname)) {
	return false;
    }
    ON_3dPoint p = v->vpnt();
    if (PPCHECK(p)) {
	std::cout << "Saw problem vert: " << v->omesh->fmesh->name << "-" << v->omesh->fmesh->f_id << "\n";
	return true;
    }
    return false;
}
#endif

#define PPOINT1 23.25001595493834472,7.56258605501400005,4.69182363386541645
#define PPOINT2 23.25001595493833406,7.50286345088642914,4.88080852489416639
#define PPOINT3 23.49789824853833409,7.50415086359986816,4.86995210942060908
static int
TPPCHECK(ON_3dPoint &p)
{
    ON_3dPoint problem1(PPOINT1);
    if (problem1.DistanceTo(p) < 0.01) {
	return 1;
    }

    ON_3dPoint problem2(PPOINT2);
    if (problem2.DistanceTo(p) < 0.01) {
	return 1;
    }

    ON_3dPoint problem3(PPOINT3);
    if (problem3.DistanceTo(p) < 0.01) {
	return 1;
    }

    return 0;
}
bool
TRICHECK(triangle_t &tri)
{
    cdt_mesh_t *fmesh1 = tri.m;
    int ppoint = 0;
    ON_3dPoint trip1, trip2, trip3;
    trip1 = *fmesh1->pnts[tri.v[0]];
    trip2 = *fmesh1->pnts[tri.v[1]];
    trip3 = *fmesh1->pnts[tri.v[2]];
    ppoint += TPPCHECK(trip1);
    ppoint += TPPCHECK(trip2);
    ppoint += TPPCHECK(trip3);

    if (ppoint == 3) {
	std::cout << "Have triangle: " << tri.ind << "\n";
    }

    return (ppoint == 3);
}


#define BB_PLOT_2D(min, max) {         \
    fastf_t pt[4][3];                  \
    VSET(pt[0], max[X], min[Y], 0);    \
    VSET(pt[1], max[X], max[Y], 0);    \
    VSET(pt[2], min[X], max[Y], 0);    \
    VSET(pt[3], min[X], min[Y], 0);    \
    pdv_3move(plot_file, pt[0]); \
    pdv_3cont(plot_file, pt[1]); \
    pdv_3cont(plot_file, pt[2]); \
    pdv_3cont(plot_file, pt[3]); \
    pdv_3cont(plot_file, pt[0]); \
}

#define TREE_LEAF_FACE_3D(valp, a, b, c, d)  \
    pdv_3move(plot_file, pt[a]); \
    pdv_3cont(plot_file, pt[b]); \
    pdv_3cont(plot_file, pt[c]); \
    pdv_3cont(plot_file, pt[d]); \
    pdv_3cont(plot_file, pt[a]); \

#define BB_PLOT(min, max) {                 \
    fastf_t pt[8][3];                       \
    VSET(pt[0], max[X], min[Y], min[Z]);    \
    VSET(pt[1], max[X], max[Y], min[Z]);    \
    VSET(pt[2], max[X], max[Y], max[Z]);    \
    VSET(pt[3], max[X], min[Y], max[Z]);    \
    VSET(pt[4], min[X], min[Y], min[Z]);    \
    VSET(pt[5], min[X], max[Y], min[Z]);    \
    VSET(pt[6], min[X], max[Y], max[Z]);    \
    VSET(pt[7], min[X], min[Y], max[Z]);    \
    TREE_LEAF_FACE_3D(pt, 0, 1, 2, 3);      \
    TREE_LEAF_FACE_3D(pt, 4, 0, 3, 7);      \
    TREE_LEAF_FACE_3D(pt, 5, 4, 7, 6);      \
    TREE_LEAF_FACE_3D(pt, 1, 5, 6, 2);      \
}

void
plot_rtree_2d(ON_RTree *rtree, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    ON_RTreeIterator rit(*rtree);
    const ON_RTreeBranch *rtree_leaf;
    for (rit.First(); 0 != (rtree_leaf = rit.Value()); rit.Next())
    {
	BB_PLOT_2D(rtree_leaf->m_rect.m_min, rtree_leaf->m_rect.m_max);
    }

    fclose(plot_file);
}

void
plot_rtree_2d2(RTree<void *, double, 2> &rtree, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    RTree<void *, double, 2>::Iterator tree_it;
    rtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	double m_min[2];
	double m_max[2];
	tree_it.GetBounds(m_min, m_max);
	BB_PLOT_2D(m_min, m_max);
	++tree_it;
    }

    //rtree.Save("test.rtree");

    fclose(plot_file);
}
void
plot_rtree_3d(RTree<void *, double, 3> &rtree, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);


    RTree<void *, double, 3>::Iterator tree_it;
    rtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	double m_min[3];
	double m_max[3];
	tree_it.GetBounds(m_min, m_max);
	BB_PLOT(m_min, m_max);
	++tree_it;
    }

    fclose(plot_file);
}

void
plot_bbox(point_t m_min, point_t m_max, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    BB_PLOT(m_min, m_max);

    fclose(plot_file);
}

void
plot_on_bbox(ON_BoundingBox &bb, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    BB_PLOT(bb.m_min, bb.m_max);

    fclose(plot_file);
}

void
plot_ce_bbox(struct ON_Brep_CDT_State *s_cdt, cpolyedge_t *pe, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    ON_2dPoint p2d1 = trim.PointAt(pe->trim_start);
    ON_2dPoint p2d2 = trim.PointAt(pe->trim_end);
    ON_Line line(p2d1, p2d2);
    ON_BoundingBox bb = line.BoundingBox();
    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;

    double dist = p2d1.DistanceTo(p2d2);
    double bdist = 0.5*dist;
    double xdist = bb.m_max.x - bb.m_min.x;
    double ydist = bb.m_max.y - bb.m_min.y;
    // If we're close to the edge, we want to know - the Search callback will
    // check the precise distance and make a decision on what to do.
    if (xdist < bdist) {
	bb.m_min.x = bb.m_min.x - 0.5*bdist;
	bb.m_max.x = bb.m_max.x + 0.5*bdist;
    }
    if (ydist < bdist) {
	bb.m_min.y = bb.m_min.y - 0.5*bdist;
	bb.m_max.y = bb.m_max.y + 0.5*bdist;
    }

    point_t m_min, m_max;
    m_min[0] = bb.Min().x;
    m_min[1] = bb.Min().y;
    m_min[2] = 0;
    m_max[0] = bb.Max().x;
    m_max[1] = bb.Max().y;
    m_max[2] = 0;

    BB_PLOT(m_min, m_max);

    fclose(plot_file);
}

void
plot_pnt_3d(FILE *plot_file, ON_3dPoint *p, double r, int dir)
{
    point_t origin, bnp;
    VSET(origin, p->x, p->y, p->z);
    pdv_3move(plot_file, origin);

    if (dir == 0) {
	VSET(bnp, p->x+r, p->y, p->z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y, p->z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y+r, p->z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y-r, p->z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }
    if (dir == 1) {
	VSET(bnp, p->x+r, p->y+r, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x+r, p->y-r, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y+r, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y-r, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);

	VSET(bnp, p->x+r, p->y+r, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x+r, p->y-r, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y+r, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y-r, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }
}

double
ang_deg(const ON_3dVector &v1, const ON_3dVector &v2)
{
    double tdp = fabs(ON_DotProduct(v1, v2));
    double d_ang = (NEAR_EQUAL(tdp, 1.0, ON_ZERO_TOLERANCE)) ? 0 : acos(tdp);
    return d_ang * 180.0/ON_PI;
}

bool
on_closest_point(ON_3dPoint &s_p, ON_3dVector &s_n, struct ON_Brep_CDT_State *s_cdt, ON_3dPoint *p)
{
    ON_BoundingBox bbb = s_cdt->brep->BoundingBox();

    bool have_pnt = false;
    double cpdist = DBL_MAX;

    ON_BoundingBox pbb(*p,*p);

    ON_3dPoint c_point = ON_3dPoint::UnsetPoint;
    ON_3dVector c_normal = ON_3dVector::UnsetVector;

    for (int i = 0; i < s_cdt->brep->m_F.Count(); i++) {
	ON_BoundingBox fbb = s_cdt->brep->m_F[i].BoundingBox();
	if (!fbb.IsDisjoint(pbb)) {
	    ON_2dPoint surf_p2d;
	    ON_3dPoint surf_p3d = ON_3dPoint::UnsetPoint;
	    double cdist;
	    surface_GetClosestPoint3dFirstOrder(s_cdt->brep->m_F[i].SurfaceOf(), *p, surf_p2d, surf_p3d, cdist, 0, ON_ZERO_TOLERANCE, 10*bbb.Diagonal().Length());
	    if (NEAR_EQUAL(cdist, DBL_MAX, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    if (cdist < cpdist) {
		ON_3dPoint s_point = ON_3dPoint::UnsetPoint;
		ON_3dVector s_norm = ON_3dVector::UnsetVector;
		c_point = surf_p3d;
		bool neval = surface_EvNormal(s_cdt->brep->m_F[i].SurfaceOf(), surf_p2d.x, surf_p2d.y, s_point, s_norm);
		if (!neval) {
		    continue;
		}
		c_point = s_point;
		if (s_cdt->brep->m_F[i].m_bRev) {
		    c_normal = -1 * s_norm;
		} else {
		    c_normal = s_norm;
		}
		have_pnt = true;
	    }
	}
    }

    s_p = c_point;
    s_n = c_normal;

    return have_pnt;
}

std::vector<cpolyedge_t *>
cdt_face_polyedges(struct ON_Brep_CDT_State *s_cdt, int face_index)
{
    std::vector<cpolyedge_t *> ws;

    ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    cdt_mesh_t *fmesh = &s_cdt->fmeshes[face_index];
    int loop_cnt = face.LoopCount();
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	cpolygon_t *cpoly = NULL;
	if (is_outer) {
	    cpoly = &fmesh->outer_loop;
	} else {
	    cpoly = fmesh->inner_loops[li];
	}

	size_t ecnt = 1;
	cpolyedge_t *pe = (*cpoly->poly.begin());
	cpolyedge_t *first = pe;
	cpolyedge_t *next = pe->next;
	first->split_status = 0;
	ws.push_back(first);
	//Walk the loop
	while (first != next) {
	    ecnt++;
	    if (!next) break;
	    next->split_status = 0;
	    ws.push_back(next);
	    next = next->next;
	    if (ecnt > cpoly->poly.size()) {
		std::cerr << "\ncdt_face_polyedges: ERROR! encountered infinite loop\n";
		ws.clear();
		return ws;
	    }
	}
    }

    return ws;
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

struct ON_Brep_CDT_State *
ON_Brep_CDT_Create(void *bv, const char *objname)
{
    struct ON_Brep_CDT_State *cdt = new struct ON_Brep_CDT_State;

    static const char *nname = "(NULL)";

    cdt->name = (objname) ? objname : nname;

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

    cdt->pnt_audit_info = new std::map<ON_3dPoint *, struct cdt_audit_info *>;

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

    delete s_cdt->pnt_audit_info;

    delete s_cdt->bot_pnt_to_on_pnt;

    delete s_cdt;
}

const char *
ON_Brep_CDT_ObjName(struct ON_Brep_CDT_State *s_cdt)
{
    return s_cdt->name;
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
		s->ovlp_max_len = s->absmin;
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
	s->cos_within_ang = -1.0;
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
	s->absmax = -1;
	s->absmin = -1;
	s->cos_within_ang = -1;
	if (s->brep) {
	    cdt_tol_global_calc(s);
	}
	return;
    }

    s->tol = *t;
    s->absmax = -1;
    s->absmin = -1;
    s->cos_within_ang = -1;
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
	struct ON_Brep_CDT_State *s)
{
    point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    vect_t nv[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
#if 0
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;
#endif

    cdt_mesh_t *fmesh = &(s->fmeshes[face_index]);
    RTree<size_t, double, 3>::Iterator tree_it;
    fmesh->tris_tree.GetFirst(tree_it);
    size_t t_ind;
    triangle_t tri;

    switch (mode) {
	case 0:
	    // 3D shaded triangles
	    while (!tree_it.IsNull()) {
		t_ind = *tree_it;
		tri = fmesh->tris_vect[t_ind];
		for (size_t j = 0; j < 3; j++) {
		    ON_3dPoint *p3d = fmesh->pnts[tri.v[j]];
		    ON_3dPoint onorm;
		    if (s->singular_vert_to_norms->find(p3d) != s->singular_vert_to_norms->end()) {
			// Use calculated normal for singularity points
			onorm = *(*s->singular_vert_to_norms)[p3d];
		    } else {
			onorm = *fmesh->normals[fmesh->nmap[tri.v[j]]];
		    }
		    VSET(pt[j], p3d->x, p3d->y, p3d->z);
		    VSET(nv[j], onorm.x, onorm.y, onorm.z);
		}
		//tri one
		BV_ADD_VLIST(vlfree, vhead, nv[0], BV_VLIST_TRI_START);
		BV_ADD_VLIST(vlfree, vhead, nv[0], BV_VLIST_TRI_VERTNORM);
		BV_ADD_VLIST(vlfree, vhead, pt[0], BV_VLIST_TRI_MOVE);
		BV_ADD_VLIST(vlfree, vhead, nv[1], BV_VLIST_TRI_VERTNORM);
		BV_ADD_VLIST(vlfree, vhead, pt[1], BV_VLIST_TRI_DRAW);
		BV_ADD_VLIST(vlfree, vhead, nv[2], BV_VLIST_TRI_VERTNORM);
		BV_ADD_VLIST(vlfree, vhead, pt[2], BV_VLIST_TRI_DRAW);
		BV_ADD_VLIST(vlfree, vhead, pt[0], BV_VLIST_TRI_END);
		//bu_log("Face %d, Tri %zd: %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f\n", face_index, i, V3ARGS(pt[0]), V3ARGS(nv[0]), V3ARGS(pt[1]), V3ARGS(nv[1]), V3ARGS(pt[2]), V3ARGS(nv[2]));
		++tree_it;
	    }
	    break;
	case 1:
	    // 3D wireframe
	    while (!tree_it.IsNull()) {
		t_ind = *tree_it;
		tri = fmesh->tris_vect[t_ind];
		for (size_t j = 0; j < 3; j++) {
		    ON_3dPoint *p3d = fmesh->pnts[tri.v[j]];
		    VSET(pt[j], p3d->x, p3d->y, p3d->z);
		}
		//tri one
		BV_ADD_VLIST(vlfree, vhead, pt[0], BV_VLIST_LINE_MOVE);
		BV_ADD_VLIST(vlfree, vhead, pt[1], BV_VLIST_LINE_DRAW);
		BV_ADD_VLIST(vlfree, vhead, pt[2], BV_VLIST_LINE_DRAW);
		BV_ADD_VLIST(vlfree, vhead, pt[0], BV_VLIST_LINE_DRAW);
		++tree_it;
	    }
	    break;
	case 2:
#if 0
	    // 2D wireframe
	    pt1[0] = p->x;
	    pt1[1] = p->y;
	    pt1[2] = 0.0;
	    p = t->GetPoint(j);
	    pt2[0] = p->x;
	    pt2[1] = p->y;
	    pt2[2] = 0.0;
	    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
#endif
	    break;
	default:
	    return -1;
    }

    return 0;
}

int ON_Brep_CDT_VList(
	struct bv_vlblock *vbp,
	struct bu_list *vlfree,
	struct bu_color *c,
	int mode,
	struct ON_Brep_CDT_State *s)
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

   vhead = bv_vlblock_find(vbp, r, g, b);

   if (UNLIKELY(!vhead)) {
       return -1;
   }

   for (int i = 0; i < s->brep->m_F.Count(); i++) {
       if (s->fmeshes[i].tris_vect.size()) {
	   (void)ON_Brep_CDT_VList_Face(vhead, vlfree, i, mode, s);
       }
   }

   return 0;
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

