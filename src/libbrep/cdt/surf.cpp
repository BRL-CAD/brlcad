/*                        C D T _ S U R F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2022 United States Government as represented by
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
/** @file cdt_surf.cpp
 *
 * Constrained Delaunay Triangulation - Surface Point sampling of NURBS B-Rep
 * objects.
 */

#include "common.h"
#include "bn/rand.h"
#include "./cdt.h"

struct cdt_surf_info {
    std::set<ON_2dPoint *> on_surf_points;
    std::set<ON_2dPoint *> on_trim_points;
    struct ON_Brep_CDT_State *s_cdt;
    const ON_Surface *s;
    bool is_planar;
    const ON_BrepFace *f;
    RTree<void *, double, 2> *rtree_2d;
    RTree<void *, double, 2> rtree_trim_spnts_2d;
    std::map<int,ON_3dPoint *> *strim_pnts;
    std::map<int,ON_3dPoint *> *strim_norms;
    double u1, u2, v1, v2;
    fastf_t ulen;
    fastf_t u_lower_3dlen;
    fastf_t u_mid_3dlen;
    fastf_t u_upper_3dlen;
    fastf_t vlen;
    fastf_t v_lower_3dlen;
    fastf_t v_mid_3dlen;
    fastf_t v_upper_3dlen;
    fastf_t min_edge;
    fastf_t max_edge;
    fastf_t within_dist;
    fastf_t cos_within_ang;
    double surface_width;
    double surface_height;
    std::set<ON_BoundingBox *> leaf_bboxes;
};

void
cpolyedge_fdists(struct cdt_surf_info *s, cdt_mesh_t *fmesh, cpolyedge_t *pe)
{

    if (!pe->eseg) return;

    ON_3dPoint *p3d1 = fmesh->pnts[fmesh->p2d3d[pe->polygon->p2o[pe->v2d[0]]]];
    ON_3dPoint *p3d2 = fmesh->pnts[fmesh->p2d3d[pe->polygon->p2o[pe->v2d[1]]]];


    double dist = p3d1->DistanceTo(*p3d2);

    s->min_edge = (s->min_edge > dist) ? dist : s->min_edge;
    s->max_edge = (s->max_edge < dist) ? dist : s->max_edge;

    // Only want the next bit if we're dealing with non-linear edges
    if (pe->eseg->edge_type != 1 || !pe->next->eseg || pe->next->eseg->edge_type != 1) return;

    ON_Line line3d(*p3d1, *p3d2);
    fastf_t emid = (pe->eseg->edge_start + pe->eseg->edge_end) / 2.0;
    ON_3dPoint edge_mid_3d = pe->eseg->nc->PointAt(emid);
    double wdist = edge_mid_3d.DistanceTo(line3d.ClosestPointTo(edge_mid_3d));
    s->within_dist = (s->within_dist > wdist) ? s->within_dist : wdist;
}

// Calculate edge-based tolerance information to use for surface breakdowns, based on
// the outer trimming loop's polygon.
void sinfo_tol_calc(struct cdt_surf_info *s)
{
    const ON_BrepFace *face = s->f;
    cdt_mesh_t *fmesh = &s->s_cdt->fmeshes[face->m_face_index];
    cpolygon_t *cpoly = &fmesh->outer_loop;

    s->min_edge = DBL_MAX;
    s->max_edge = -DBL_MAX;
    s->within_dist = -DBL_MAX;

    size_t ecnt = 1;
    cpolyedge_t *pe = (*cpoly->poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    cpolyedge_fdists(s, fmesh, first);

    // Walk the loop
    while (first != next) {
	ecnt++;
	if (!next) break;
	cpolyedge_fdists(s, fmesh, next);
	next = next->next;
	if (ecnt > cpoly->poly.size()) {
	    std::cerr << "\nsinfo_tol_calc: ERROR! encountered infinite loop\n";
	    return;
	}
    }
}


class SPatch {

    public:
	SPatch(double u1, double u2, double v1, double v2)
	{
	    umin = u1;
	    umax = u2;
	    vmin = v1;
	    vmax = v2;
	}

	void plot(const char *filename);

	double umin;
	double umax;
	double vmin;
	double vmax;
};

void SPatch::plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    ON_2dPoint p2dmin(umin, vmin);
    ON_2dPoint p2dmax(umax, vmax);
    ON_Line line(p2dmin, p2dmax);
    ON_BoundingBox bb = line.BoundingBox();
    fastf_t pt[4][3];
    VSET(pt[0], bb.Max().x, bb.Min().y, 0);
    VSET(pt[1], bb.Max().x, bb.Max().y, 0);
    VSET(pt[2], bb.Min().x, bb.Max().y, 0);
    VSET(pt[3], bb.Min().x, bb.Min().y, 0);
    pdv_3move(plot_file, pt[0]);
    pdv_3cont(plot_file, pt[1]);
    pdv_3cont(plot_file, pt[2]);
    pdv_3cont(plot_file, pt[3]);
    pdv_3cont(plot_file, pt[0]);

    fclose(plot_file);
}

static double
uline_len_est(struct cdt_surf_info *sinfo, double u1, double u2, double v)
{
    double t, lenfact, lenest;
    int active_half = (fabs(sinfo->v1 - v) < fabs(sinfo->v2 - v)) ? 0 : 1;
    t = (active_half == 0) ? 1 - fabs(sinfo->v1 - v)/fabs((sinfo->v2 - sinfo->v1)*0.5) : 1 - fabs(sinfo->v2 - v)/fabs((sinfo->v2 - sinfo->v1)*0.5);
    if (active_half == 0) {
	lenfact = sinfo->u_lower_3dlen * (1 - (t)) + sinfo->u_mid_3dlen * (t);
	lenest = (u2 - u1)/sinfo->ulen * lenfact;
    } else {
	lenfact = sinfo->u_mid_3dlen * (1 - (t)) + sinfo->u_upper_3dlen * (t);
	lenest = (u2 - u1)/sinfo->ulen * lenfact;
    }
    return lenest;
}


static double
vline_len_est(struct cdt_surf_info *sinfo, double u, double v1, double v2)
{
    double t, lenfact, lenest;
    int active_half = (fabs(sinfo->u1 - u) < fabs(sinfo->u2 - u)) ? 0 : 1;
    t = (active_half == 0) ? 1 - fabs(sinfo->u1 - u)/fabs((sinfo->u2 - sinfo->u1)*0.5) : 1 - fabs(sinfo->u2 - u)/fabs((sinfo->u2 - sinfo->u1)*0.5);
    if (active_half == 0) {
	lenfact = sinfo->v_lower_3dlen * (1 - (t)) + sinfo->v_mid_3dlen * (t);
	lenest = (v2 - v1)/sinfo->vlen * lenfact;
    } else {
	lenfact = sinfo->v_mid_3dlen * (1 - (t)) + sinfo->v_upper_3dlen * (t);
	lenest = (v2 - v1)/sinfo->vlen * lenfact;
    }
    return lenest;
}

static bool EdgeSegCallback(void *data, void *a_context) {
    cpolyedge_t *eseg = (cpolyedge_t *)data;
    std::set<cpolyedge_t *> *segs = (std::set<cpolyedge_t *> *)a_context;
    segs->insert(eseg);
    return true;
}

struct trim_seg_context {
    const ON_2dPoint *p;
    bool on_edge;
};

static bool SPntCallback(void *data, void *a_context) {
    cpolyedge_t *pe = (cpolyedge_t *)data;
    struct trim_seg_context *sc = (struct trim_seg_context *)a_context;
    ON_2dPoint p2d1(pe->polygon->pnts_2d[pe->v2d[0]].first, pe->polygon->pnts_2d[pe->v2d[0]].second);
    ON_2dPoint p2d2(pe->polygon->pnts_2d[pe->v2d[1]].first, pe->polygon->pnts_2d[pe->v2d[1]].second);
    ON_Line l(p2d1, p2d2);
    double t;
    if (l.ClosestPointTo(*sc->p, &t)) {
	// If the closest point on the line isn't in the line segment, skip
	if (t < 0 || t > 1) return true;
    }

    double mdist = l.MinimumDistanceTo(pe->spnt);
    double dist = l.MinimumDistanceTo(*sc->p);
    if (dist < mdist) {
	sc->on_edge = true;
    }
    return (sc->on_edge) ? false : true;
}

/* If we've got trimming curves involved, we need to be more careful about respecting
 * the min edge distance. */
static bool involves_trims(double *min_edge, struct cdt_surf_info *sinfo, ON_3dPoint &p1, ON_3dPoint &p2)
{
    double min_edge_dist = sinfo->max_edge;

    // Bump out the intersection box a bit - we want to be aggressive when adapting to local
    // trimming curve segments
    ON_3dPoint wpc = (p1+p2) * 0.5;
    ON_3dVector vec1 = p1 - wpc;
    ON_3dVector vec2 = p2 - wpc;
    ON_3dPoint wp1 = wpc + (vec1 * 1.1);
    ON_3dPoint wp2 = wpc + (vec2 * 1.1);

    ON_BoundingBox uvbb;
    uvbb.Set(wp1, true);
    uvbb.Set(wp2, true);

    //plot_on_bbox(uvbb, "uvbb.plot3");

    //plot_rtree_3d(sinfo->s_cdt->face_rtrees_3d[sinfo->f->m_face_index], "rtree.plot3");

    double fMin[3];
    fMin[0] = wp1.x;
    fMin[1] = wp1.y;
    fMin[2] = wp1.z;
    double fMax[3];
    fMax[0] = wp2.x;
    fMax[1] = wp2.y;
    fMax[2] = wp2.z;

    std::set<cpolyedge_t *> segs;
    size_t nhits = sinfo->s_cdt->face_rtrees_3d[sinfo->f->m_face_index].Search(fMin, fMax, EdgeSegCallback, (void *)&segs);
    //bu_log("new tree found %zu boxes and %zu segments\n", nhits, segs.size());

    if (!nhits) {
	return false;
    }

    // The nhits threshold is basically indirectly determining how many triangles
    // from the edge we're willing to have point to a single interior point. Too
    // many and we'll end up with a lot of long, thin triangles.
    if (nhits > 5) {
	// Lot of edges, probably a high level box we need to split - just return the overall min_edge
	(*min_edge) = sinfo->min_edge;
	return true;
    }

    std::set<cpolyedge_t *>::iterator s_it;
    for (s_it = segs.begin(); s_it != segs.end(); s_it++) {
	cpolyedge_t *seg = *s_it;
	ON_BoundingBox lbb;
	lbb.Set(*seg->eseg->e_start, true);
	lbb.Set(*seg->eseg->e_end, true);
	if (!uvbb.IsDisjoint(lbb)) {
	    fastf_t dist = lbb.Diagonal().Length();
	    if ((dist > BN_TOL_DIST) && (dist < min_edge_dist))  {
		min_edge_dist = dist;
	    }
	}
    }
    (*min_edge) = min_edge_dist;

    return true;
}

/* flags identifying which side of the surface we're calculating
 *
 *                        u_upper
 *                        (2,0)
 *
 *               u1v2---------------- u2v2
 *                |          |         |
 *                |          |         |
 *                |  u_lmid  |         |
 *    v_lower     |----------|---------|     v_upper
 *     (0,1)      |  (1,0)   |         |      (2,1)
 *                |         v|lmid     |
 *                |          |(1,1)    |
 *               u1v1-----------------u2v1
 *
 *                        u_lower
 *                         (0,0)
 */
static double
_cdt_get_uv_edge_3d_len(struct cdt_surf_info *sinfo, int c1, int c2)
{
    int line_set = 0;
    double wu1 = 0.0;
    double wv1 = 0.0;
    double wu2 = 0.0;
    double wv2 = 0.0;
    double umid = 0.0;
    double vmid = 0.0;

    /* u_lower */
    if (c1 == 0 && c2 == 0) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u2;
	wv1 = sinfo->v1;
	wv2 = sinfo->v1;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = sinfo->v1;
	line_set = 1;
    }

    /* u_lmid */
    if (c1 == 1 && c2 == 0) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u2;
	wv1 = (sinfo->v2 - sinfo->v1)/2.0;
	wv2 = (sinfo->v2 - sinfo->v1)/2.0;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    /* u_upper */
    if (c1 == 2 && c2 == 0) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u2;
	wv1 = sinfo->v2;
	wv2 = sinfo->v2;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = sinfo->v2;
	line_set = 1;
    }

    /* v_lower */
    if (c1 == 0 && c2 == 1) {
	wu1 = sinfo->u1;
	wu2 = sinfo->u1;
	wv1 = sinfo->v1;
	wv2 = sinfo->v2;
	umid = sinfo->u1;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    /* v_lmid */
    if (c1 == 1 && c2 == 1) {
	wu1 = (sinfo->u2 - sinfo->u1)/2.0;
	wu2 = (sinfo->u2 - sinfo->u1)/2.0;
	wv1 = sinfo->v1;
	wv2 = sinfo->v2;
	umid = (sinfo->u2 - sinfo->u1)/2.0;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    /* v_upper */
    if (c1 == 2 && c2 == 1) {
	wu1 = sinfo->u2;
	wu2 = sinfo->u2;
	wv1 = sinfo->v1;
	wv2 = sinfo->v2;
	umid = sinfo->u2;
	vmid = (sinfo->v2 - sinfo->v1)/2.0;
	line_set = 1;
    }

    if (!line_set) {
	bu_log("Invalid edge %d, %d specified\n", c1, c2);
	return DBL_MAX;
    }

    // 1st 3d point
    ON_3dPoint p1 = sinfo->s->PointAt(wu1, wv1);

    // middle 3d point
    ON_3dPoint pmid = sinfo->s->PointAt(umid, vmid);

    // last 3d point
    ON_3dPoint p2 = sinfo->s->PointAt(wu2, wv2);

    // length
    double d1 = pmid.DistanceTo(p1);
    double d2 = p2.DistanceTo(pmid);
    return d1+d2;
}

struct rtree_trimpnt_context {
    struct ON_Brep_CDT_State *s_cdt;
    cpolyedge_t *cseg;
    bool *use;
};

#if 1
static bool UseTrimPntCallback(void *data, void *a_context) {
    cpolyedge_t *tseg = (cpolyedge_t *)data;
    struct rtree_trimpnt_context *context= (struct rtree_trimpnt_context *)a_context;

    // Our own bbox isn't a problem
    if (tseg == context->cseg) return true;

    // If this is a foreign box overlap, see if spnt is inside tseg's bbox.  If it is,
    // we can't use spnt
    ON_BoundingBox bb(context->cseg->spnt, context->cseg->spnt);
    if (tseg->bb.Includes(bb)) {
	*(context->use) = false;
	return false;
    }

    return true;
}
#endif


static void
sinfo_init(struct cdt_surf_info *sinfo, struct ON_Brep_CDT_State *s_cdt, int face_index)
{
    ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    const ON_Surface *s = face.SurfaceOf();
    const ON_Brep *brep = face.Brep();

    sinfo->s_cdt = s_cdt;
    sinfo->s = s;

    double ptol = s->BoundingBox().Diagonal().Length()*0.001;
    ptol = (ptol < BREP_PLANAR_TOL) ? ptol : BREP_PLANAR_TOL;
    sinfo->is_planar = s->IsPlanar(NULL, ptol);

    sinfo->f = &face;
    sinfo->rtree_2d = &(s_cdt->face_rtrees_2d[face_index]);
    sinfo->strim_pnts = &(s_cdt->strim_pnts[face_index]);
    sinfo->strim_norms = &(s_cdt->strim_norms[face_index]);
    double t1, t2;
    s->GetDomain(0, &t1, &t2);
    sinfo->ulen = fabs(t2 - t1);
    s->GetDomain(1, &t1, &t2);
    sinfo->vlen = fabs(t2 - t1);
    s->GetDomain(0, &sinfo->u1, &sinfo->u2);
    s->GetDomain(1, &sinfo->v1, &sinfo->v2);
    sinfo->u_lower_3dlen = _cdt_get_uv_edge_3d_len(sinfo, 0, 0);
    sinfo->u_mid_3dlen   = _cdt_get_uv_edge_3d_len(sinfo, 1, 0);
    sinfo->u_upper_3dlen = _cdt_get_uv_edge_3d_len(sinfo, 2, 0);
    sinfo->v_lower_3dlen = _cdt_get_uv_edge_3d_len(sinfo, 0, 1);
    sinfo->v_mid_3dlen   = _cdt_get_uv_edge_3d_len(sinfo, 1, 1);
    sinfo->v_upper_3dlen = _cdt_get_uv_edge_3d_len(sinfo, 2, 1);
    sinfo->min_edge = (*s_cdt->min_edge_seg_len)[face_index];
    sinfo->max_edge = (*s_cdt->max_edge_seg_len)[face_index];

    // If the trims will be contributing points, we need to figure out which ones
    // and assemble an rtree for them.  We can't insert points from the general
    // build too close to them or we run the risk of duplicate points and very small
    // triangles.
    std::vector<cpolyedge_t *> ws = cdt_face_polyedges(s_cdt, face.m_face_index);
    std::vector<cpolyedge_t *>::iterator w_it;
    float *prand;
    bn_rand_init(prand, 0);
    for (w_it = ws.begin(); w_it != ws.end(); w_it++) {
	cpolyedge_t *tseg = *w_it;
	if (!tseg->defines_spnt) continue;
	bool include_pnt = true;

	struct rtree_trimpnt_context a_context;
	a_context.s_cdt = s_cdt;
	a_context.cseg = tseg;
	a_context.use = &include_pnt;

	double dlen = tseg->bb.Diagonal().Length();
	dlen = 0.01 * dlen;
	double px = tseg->spnt.x + (bn_rand_half(prand) * dlen);
	double py = tseg->spnt.y + (bn_rand_half(prand) * dlen);

	double tMin[2];
	tMin[0] = px - ON_ZERO_TOLERANCE;
	tMin[1] = py - ON_ZERO_TOLERANCE;
	double tMax[2];
	tMax[0] = px + ON_ZERO_TOLERANCE;
	tMax[1] = py + ON_ZERO_TOLERANCE;

	s_cdt->face_rtrees_2d[face.m_face_index].Search(tMin, tMax, UseTrimPntCallback, (void *)&a_context);

	if (include_pnt) {
	    //std::cout << "Accept\n";
	    sinfo->rtree_trim_spnts_2d.Insert(tMin, tMax, (void *)tseg);

	    sinfo->on_trim_points.insert(new ON_2dPoint(px, py));
	}

	// While we're at it, put boxes around the trim points too
	const ON_BrepTrim &trim = brep->m_T[tseg->trim_ind];
	ON_2dPoint pstart = trim.PointAt(tseg->trim_start);
	dlen = 0.25*tseg->bb.Diagonal().Length();
    	tMin[0] = pstart.x - dlen;
	tMin[1] = pstart.y - dlen;
	tMax[0] = pstart.x + dlen;
	tMax[1] = pstart.y + dlen;
	sinfo->rtree_trim_spnts_2d.Insert(tMin, tMax, (void *)tseg);

    }
#if 0
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%d-rtree_2d_trim_pnts.plot3", sinfo->f->m_face_index);
    plot_rtree_2d2(sinfo->rtree_trim_spnts_2d, bu_vls_cstr(&fname));
    bu_vls_free(&fname);
#endif

    // Calculate edge-based tolerance information to use for surface breakdowns -
    // we want the surface interiors to reflect the edges so they aren't too
    // dissimilar.
    sinfo_tol_calc(sinfo);

}


void
filter_surface_pnts(struct cdt_surf_info *sinfo)
{

    // Remove points that are troublesome per 2D filtering criteria
    std::set<ON_2dPoint *> rm_pnts;
    std::set<ON_2dPoint *>::iterator osp_it;
    for (osp_it = sinfo->on_surf_points.begin(); osp_it != sinfo->on_surf_points.end(); osp_it++) {
	const ON_2dPoint *p = *osp_it;
	double fMin[2];
	double fMax[2];
	fMin[0] = p->x - ON_ZERO_TOLERANCE;
	fMin[1] = p->y - ON_ZERO_TOLERANCE;
	fMax[0] = p->x + ON_ZERO_TOLERANCE;
	fMax[1] = p->y + ON_ZERO_TOLERANCE;

	if (sinfo->rtree_trim_spnts_2d.Search(fMin, fMax, NULL, NULL)) {
	    rm_pnts.insert((ON_2dPoint *)p);
	} else {
	    struct trim_seg_context sc;
	    sc.p = p;
	    sc.on_edge = false;

	    sinfo->rtree_2d->Search(fMin, fMax, SPntCallback, (void *)&sc);
	    if (sc.on_edge) {
		rm_pnts.insert((ON_2dPoint *)p);
	    }
	}
    }

    for (osp_it = rm_pnts.begin(); osp_it != rm_pnts.end(); osp_it++) {
	const ON_2dPoint *p = *osp_it;
	sinfo->on_surf_points.erase((ON_2dPoint *)p);
    }

    cdt_mesh_t *fmesh = &sinfo->s_cdt->fmeshes[sinfo->f->m_face_index];

    // Populate m_interior_pnts with the final set
    for (osp_it = sinfo->on_surf_points.begin(); osp_it != sinfo->on_surf_points.end(); osp_it++) {
	ON_2dPoint n2dp(**osp_it);

	// Calculate the 3D point and normal values.
	ON_3dPoint p3d;
	ON_3dVector norm = ON_3dVector::UnsetVector;
	if (!surface_EvNormal(sinfo->s, n2dp.x, n2dp.y, p3d, norm)) {
	    p3d = sinfo->s->PointAt(n2dp.x, n2dp.y);
	}

	// Last filtering pass before insertion: if we're too close to an edge
	// in 3D, the point is out.
	double fMin[3];
	fMin[0] = p3d.x - ON_ZERO_TOLERANCE;
	fMin[1] = p3d.y - ON_ZERO_TOLERANCE;
	fMin[2] = p3d.z - ON_ZERO_TOLERANCE;
	double fMax[3];
	fMax[0] = p3d.x + ON_ZERO_TOLERANCE;
	fMax[1] = p3d.y + ON_ZERO_TOLERANCE;
	fMax[2] = p3d.z + ON_ZERO_TOLERANCE;
	size_t nhits = sinfo->s_cdt->face_rtrees_3d[sinfo->f->m_face_index].Search(fMin, fMax, NULL, NULL);
	if (nhits) continue;


	long f_ind2d = fmesh->add_point(n2dp);
	fmesh->m_interior_pnts.insert(f_ind2d);
	if (fmesh->m_bRev) {
	    norm = -1 * norm;
	}

	long f3ind = fmesh->add_point(new ON_3dPoint(p3d));
	long fnind = fmesh->add_normal(new ON_3dPoint(norm));

	CDT_Add3DPnt(sinfo->s_cdt, fmesh->pnts[fmesh->pnts.size()-1], sinfo->f->m_face_index, -1, -1, -1, n2dp.x, n2dp.y);
	CDT_Add3DNorm(sinfo->s_cdt, fmesh->normals[fmesh->normals.size()-1], fmesh->pnts[fmesh->pnts.size()-1], sinfo->f->m_face_index, -1, -1, -1, n2dp.x, n2dp.y);

	fmesh->p2d3d[f_ind2d] = f3ind;
	fmesh->nmap[f3ind] = fnind;
    }

    // Points from trims are handled separately
    for (osp_it = sinfo->on_trim_points.begin(); osp_it != sinfo->on_trim_points.end(); osp_it++) {
	ON_2dPoint n2dp(**osp_it);

	// Calculate the 3D point and normal values.
	ON_3dPoint p3d;
	ON_3dVector norm = ON_3dVector::UnsetVector;
	if (!surface_EvNormal(sinfo->s, n2dp.x, n2dp.y, p3d, norm)) {
	    p3d = sinfo->s->PointAt(n2dp.x, n2dp.y);
	}

	long f_ind2d = fmesh->add_point(n2dp);
	fmesh->m_interior_pnts.insert(f_ind2d);
	if (fmesh->m_bRev) {
	    norm = -1 * norm;
	}

	long f3ind = fmesh->add_point(new ON_3dPoint(p3d));
	long fnind = fmesh->add_normal(new ON_3dPoint(norm));

	CDT_Add3DPnt(sinfo->s_cdt, fmesh->pnts[fmesh->pnts.size()-1], sinfo->f->m_face_index, -1, -1, -1, n2dp.x, n2dp.y);
	CDT_Add3DNorm(sinfo->s_cdt, fmesh->normals[fmesh->normals.size()-1], fmesh->pnts[fmesh->pnts.size()-1], sinfo->f->m_face_index, -1, -1, -1, n2dp.x, n2dp.y);

	fmesh->p2d3d[f_ind2d] = f3ind;
	fmesh->nmap[f3ind] = fnind;
    }

}

static bool
getSurfacePoint(
	         struct cdt_surf_info *sinfo,
		 SPatch &sp,
		 std::queue<SPatch> &nq,
		 int depth
		 )
{
    fastf_t u1 = sp.umin;
    fastf_t u2 = sp.umax;
    fastf_t v1 = sp.vmin;
    fastf_t v2 = sp.vmax;
    int split_u = 0;
    int split_v = 0;
    ON_2dPoint p2d(0.0, 0.0);
    fastf_t u = (u1 + u2) / 2.0;
    fastf_t v = (v1 + v2) / 2.0;

    //sp.plot("spatch.p3");

    double est1 = uline_len_est(sinfo, u1, u2, v1);
    double est2 = uline_len_est(sinfo, u1, u2, v2);
    double est3 = vline_len_est(sinfo, u1, v1, v2);
    double est4 = vline_len_est(sinfo, u2, v1, v2);

    double uavg = (est1+est2)/2.0;
    double vavg = (est3+est4)/2.0;

    if (est1 < 0.01*sinfo->within_dist && est2 < 0.01*sinfo->within_dist) {
	return false;
    }
    if (est3 < 0.01*sinfo->within_dist && est4 < 0.01*sinfo->within_dist) {
	return false;
    }

    if (uavg < sinfo->min_edge && vavg < sinfo->min_edge) {
	return false;
    }

    if (depth == 0 && !sinfo->is_planar) {
	split_u = 1;
	split_v = 1;
    }

    if (depth < 2 && sinfo->s->IsClosed(0)) {
	split_u = 1;
    }

    if (depth < 2 && sinfo->s->IsClosed(1)) {
	split_v = 1;

    }

    double ldfactor = 2.0;
    if (uavg > ldfactor * vavg) {
	split_u = 1;
    }

    if (vavg > ldfactor * uavg) {
	split_v = 1;
    }

    double min_edge_len = -1.0;

    if (!split_u || !split_v) {
	// If we're dealing with a curved surface, don't get bigger than max_edge
	if (!sinfo->is_planar) {
	    min_edge_len = sinfo->max_edge;
	    if (uavg > min_edge_len && vavg > min_edge_len) {
		split_u = 1;
	    }

	    if (uavg > min_edge_len && vavg > min_edge_len) {
		split_v = 1;
	    }
	}
    }

    if (!split_u || !split_v) {
	// Don't know if we're splitting in at least one direction - check if we're close
	// enough to trims to need to worry about edges
	ON_3dPoint p[4] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
	ON_3dVector norm[4] = {ON_3dVector(), ON_3dVector(), ON_3dVector(), ON_3dVector()};

	if ((surface_EvNormal(sinfo->s, u1, v1, p[0], norm[0]))
		&& (surface_EvNormal(sinfo->s, u2, v1, p[1], norm[1]))
		&& (surface_EvNormal(sinfo->s, u2, v2, p[2], norm[2]))
		&& (surface_EvNormal(sinfo->s, u1, v2, p[3], norm[3]))) {


	    ON_BoundingBox uvbb;
	    for (int i = 0; i < 4; i++) {
		uvbb.Set(p[i], true);
	    }
	    //plot_on_bbox(uvbb, "uvbb.plot3");

	    ON_3dPoint pmin = uvbb.Min();
	    ON_3dPoint pmax = uvbb.Max();
	    if (involves_trims(&min_edge_len, sinfo, pmin, pmax)) {

		if (min_edge_len > 0 && uavg > min_edge_len && vavg > min_edge_len) {
		    split_u = 1;
		}

		if (min_edge_len > 0 && uavg > min_edge_len && vavg > min_edge_len) {
		    split_v = 1;
		}
	    }
	}
    }

    // If we still don't completely know what we're doing, check how far in U and V our
    // midpoint is from the line between the midpoints in that direction.
    if (!sinfo->is_planar && (!split_u || !split_v)) {
	ON_3dPoint p[5] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
	ON_3dVector norm[5] = {ON_3dVector(), ON_3dVector(), ON_3dVector(), ON_3dVector(), ON_3dVector()};

	if ((surface_EvNormal(sinfo->s, u, v1, p[0], norm[0]))
		&& (surface_EvNormal(sinfo->s, u, v2, p[1], norm[1]))
		&& (surface_EvNormal(sinfo->s, u1, v, p[2], norm[2]))
		&& (surface_EvNormal(sinfo->s, u2, v, p[3], norm[3]))
		&& (surface_EvNormal(sinfo->s, u, v, p[4], norm[4]))
		) {
	    ON_Line uline(p[2], p[3]);
	    ON_Line vline(p[0], p[1]);
	    double uldist = p[4].DistanceTo(uline.ClosestPointTo(p[4]));
	    double vldist = p[4].DistanceTo(vline.ClosestPointTo(p[4]));

	    if (uldist > sinfo->within_dist) {
		split_u = 1;
	    }

	    if (vldist > sinfo->within_dist) {
		split_v = 1;
	    }
	}
    }

    if (split_u && split_v) {
	nq.push(SPatch(u1, u, v1, v));
	nq.push(SPatch(u1, u, v, v2));
	nq.push(SPatch(u, u2, v1, v));
	nq.push(SPatch(u, u2, v, v2));
	return true;
    }
    if (split_u) {
	nq.push(SPatch(u1, u, v1, v2));
	nq.push(SPatch(u, u2, v1, v2));
	return true;
    }
    if (split_v) {
	nq.push(SPatch(u1, u2, v1, v));
	nq.push(SPatch(u1, u2, v, v2));
	return true;
    }

    return false;
}

void
GetInteriorPoints(struct ON_Brep_CDT_State *s_cdt, int face_index)
{
    ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    const ON_Surface *s = face.SurfaceOf();
    struct cdt_surf_info sinfo;

    if (s->GetSurfaceSize(&sinfo.surface_width, &sinfo.surface_height)) {

	if ((sinfo.surface_width < ON_ZERO_TOLERANCE) || (sinfo.surface_height < ON_ZERO_TOLERANCE)) {
	    return;
	}

	sinfo_init(&sinfo, s_cdt, face_index);

	// Get the min and max dimensions in UV that will kick off the sampling
	// process.  May be a smaller trimmed subset of surface so worth
	// getting face boundary
	ON_BoundingBox lbox;
	face.OuterLoop()->GetBoundingBox(lbox);
	ON_3dPoint min = lbox.Min();
	ON_3dPoint max = lbox.Max();

	std::queue<SPatch> spq1, spq2;

	/**
	 * Sample portions of the surface to collect sufficient points
	 * to capture the surface shape according to the settings
	 *
	 * M = max
	 * m = min
	 * o = mid
	 *
	 *    umvM------uovM-------uMvM
	 *     |          |         |
	 *     |          |         |
	 *     |          |         |
	 *    umvo------uovo-------uMvo
	 *     |          |         |
	 *     |          |         |
	 *     |          |         |
	 *    umvm------uovm-------uMvm
	 *
	 * left bottom  = umvm
	 * left midy    = umvo
	 * left top     = umvM
	 * midx bottom  = uovm
	 * midx midy    = uovo
	 * midx top     = uovM
	 * right bottom = uMvm
	 * right midy   = uMvo
	 * right top    = uMvM
	 */

	ON_BOOL32 uclosed = s->IsClosed(0);
	ON_BOOL32 vclosed = s->IsClosed(1);
	double midx = (min.x + max.x) / 2.0;
	double midy = (min.y + max.y) / 2.0;

	if (uclosed && vclosed) {
	    /*
	     *     #--------------------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo------uovo--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm------uovm--------#
	     */
	    spq1.push(SPatch(min.x, midx, min.y, midy));

	    /*
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovo-------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovm-------uMvm
	     */
	    spq1.push(SPatch(midx, max.x, min.y, midy));

	    /*
	     *    umvM------uovM--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo------uovo--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     */
	    spq1.push(SPatch(min.x, midx, midy, max.y));

	    /*
	     *     #--------uovM------ uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovo-------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     */
	    spq1.push(SPatch(midx, max.x, midy, max.y));

	} else if (uclosed) {

	    /*
	     *    umvM------uovM--------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm------uovm--------#
	     */
	    spq1.push(SPatch(min.x, midx, min.y, max.y));

	    /*
	     *     #--------uovM------ uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #--------uovm-------uMvm
	     */
	    spq1.push(SPatch(midx, max.x, min.y, max.y));

	} else if (vclosed) {

	    /*
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo--------#--------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm--------#--------uMvm
	     */
	    spq1.push(SPatch(min.x, max.x, min.y, midy));

	    /*
	     *    umvM--------#------- uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvo--------#--------uMvo
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     */
	    spq1.push(SPatch(min.x, max.x, midy, max.y));

	} else {

	    /*
	     *    umvM--------#------- uMvM
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *     #----------#---------#
	     *     |          |         |
	     *     |          |         |
	     *     |          |         |
	     *    umvm--------#--------uMvm
	     */
	    spq1.push(SPatch(min.x, max.x, min.y, max.y));
	}

	std::queue<SPatch> *wq = &spq1;
	std::queue<SPatch> *nq = &spq2;
	int split_depth = 0;


	while (!wq->empty()) {
	    SPatch sp = wq->front();
	    wq->pop();
	    if (!getSurfacePoint(&sinfo, sp, *nq, split_depth)) {
		ON_BoundingBox bb(ON_2dPoint(sp.umin,sp.vmin),ON_2dPoint(sp.umax, sp.vmax));
		sinfo.leaf_bboxes.insert(new ON_BoundingBox(bb));
	    }

	    // Once we've processed the current level,
	    // work on the next if we need to
	    if (wq->empty() && !nq->empty()) {
		std::queue<SPatch> *tq = wq;
		wq = nq;
		nq = tq;
		// Let the counter know we're going deeper
		split_depth++;
		//std::cout << "split_depth: " << split_depth << "\n";
	    }
	}

	float *prand;
	std::set<ON_BoundingBox *>::iterator b_it;
	/* We want to jitter sampled 2D points out of linearity */
	bn_rand_init(prand, 0);
	for (b_it = sinfo.leaf_bboxes.begin(); b_it != sinfo.leaf_bboxes.end(); b_it++) {
	    ON_3dPoint p2d = (*b_it)->Center();
	    ON_3dPoint pmax = (*b_it)->Max();
	    ON_3dPoint pmin = (*b_it)->Min();
	    double ulen = pmax.x - pmin.x;
	    double vlen = pmax.y - pmin.y;
	    double px = p2d.x + (bn_rand_half(prand) * 0.3*ulen);
	    double py = p2d.y + (bn_rand_half(prand) * 0.3*vlen);

	    double tMin[2];
	    tMin[0] = (*b_it)->Min().x;
	    tMin[1] = (*b_it)->Min().y;
	    double tMax[2];
	    tMax[0] = (*b_it)->Max().x;
	    tMax[1] = (*b_it)->Max().y;
	    size_t nhits = sinfo.rtree_trim_spnts_2d.Search(tMin, tMax, NULL, NULL);

	    if (!nhits) {
		sinfo.on_surf_points.insert(new ON_2dPoint(px,py));
	    }
	}

	// Strip out points from the surface that are too close to the trimming curves.
	filter_surface_pnts(&sinfo);

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

