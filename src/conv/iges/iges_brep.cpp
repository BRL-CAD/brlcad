/*                    I G E S _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file iges/iges_brep.cpp
 *
 * Convert the assembled NMG boundary representation produced by the
 * classic IGES importer into an OpenNURBS ON_Brep, written as a BRL-CAD
 * rt_brep object.
 *
 * The importer already parses the original IGES analytic geometry into
 * NMG: rational B-spline surfaces (IGES 128) become face_g_snurb, planes
 * (IGES 190/108) become face_g_plane, and parameter-space trim curves
 * (IGES 126/110 via 142/508) become edge_g_cnurb on the edgeuses.  All of
 * that data maps directly onto OpenNURBS:
 *
 *   face_g_snurb  -> ON_NurbsSurface   (exact: same order/knots/weights)
 *   face_g_plane  -> ON_PlaneSurface   (exact: the actual plane)
 *   edge_g_cnurb  -> ON_NurbsCurve     (2d parameter-space trim curve)
 *
 * so this conversion preserves the original representation rather than
 * tessellating it to a triangle mesh.  Shared topology (vertices, edges)
 * is de-duplicated through the NMG structure index, exactly as
 * rt_nmg_brep() does, to produce a watertight solid.
 */

#include "common.h"

/* OpenNURBS (pulls in the full ON_* API through libbrep) */
#include "brep.h"

#include "vmath.h"
#include "bn/tol.h"
#include "nmg.h"
#include "rt/geom.h"
#include "wdb.h"
#include "bu/log.h"
#include "bu/malloc.h"

#include "./iges_brep.h"


namespace {

/* Bridge an NMG rational B-spline surface (face_g_snurb) to an
 * ON_NurbsSurface.  NMG stores the two extra clamped end knots that
 * OpenNURBS omits, and (for rational surfaces) stores homogeneous,
 * weight-premultiplied control points -- both match ON's expectations
 * once the end knots are dropped. */
ON_NurbsSurface *
snurb_to_on(const struct face_g_snurb *s)
{
    NMG_CK_FACE_G_SNURB(s);

    const int u_order = s->order[0];
    const int v_order = s->order[1];
    const int u_cv = s->s_size[1];	/* columns, u direction */
    const int v_cv = s->s_size[0];	/* rows, v direction */
    const int ncoords = RT_NURB_EXTRACT_COORDS(s->pt_type);
    const bool rational = RT_NURB_IS_PT_RATIONAL(s->pt_type) ? true : false;

    ON_NurbsSurface *ns =
	ON_NurbsSurface::New(3, rational, u_order, v_order, u_cv, v_cv);
    if (!ns)
	return NULL;

    /* internal knots only: KnotCount == order + cv_count - 2 */
    for (int i = 0; i < ns->KnotCount(0); i++)
	ns->SetKnot(0, i, s->u.knots[i + 1]);
    for (int i = 0; i < ns->KnotCount(1); i++)
	ns->SetKnot(1, i, s->v.knots[i + 1]);

    /* NMG control mesh is row-major: index = (row*u_cv + col)*ncoords,
     * with row the v index and col the u index. */
    for (int v = 0; v < v_cv; v++) {
	for (int u = 0; u < u_cv; u++) {
	    const fastf_t *cv = &s->ctl_points[(v * u_cv + u) * ncoords];
	    if (rational)
		ns->SetCV(u, v, ON::homogeneous_rational, cv);
	    else
		ns->SetCV(u, v, ON::not_rational, cv);
	}
    }

    return ns;
}


/* Bridge an NMG parameter-space curve (edge_g_cnurb, RT_NURB_PT_UV) to a
 * 2d ON_NurbsCurve suitable for use as a trim curve. */
ON_NurbsCurve *
cnurb_to_on_uv(const struct edge_g_cnurb *c)
{
    NMG_CK_CNURB(c);

    const int order = c->order;
    const int cv = c->c_size;
    const int ncoords = RT_NURB_EXTRACT_COORDS(c->pt_type);
    const bool rational = RT_NURB_IS_PT_RATIONAL(c->pt_type) ? true : false;

    ON_NurbsCurve *nc = ON_NurbsCurve::New(2, rational, order, cv);
    if (!nc)
	return NULL;

    for (int i = 0; i < nc->KnotCount(); i++)
	nc->SetKnot(i, c->k.knots[i + 1]);

    for (int i = 0; i < cv; i++) {
	const fastf_t *p = &c->ctl_points[i * ncoords];
	if (rational)
	    nc->SetCV(i, ON::homogeneous_rational, p);	/* [u*w, v*w, w] */
	else
	    nc->SetCV(i, ON::not_rational, p);		/* [u, v] */
    }

    return nc;
}


/* Recover the parameter-space (u, v) location stored on a vertexuse. */
bool
vu_uv(const struct vertexuse *vu, ON_2dPoint &uv)
{
    if (!vu->a.magic_p || *vu->a.magic_p != NMG_VERTEXUSE_A_CNURB_MAGIC)
	return false;

    const struct vertexuse_a_cnurb *a = vu->a.cnurb_p;
    fastf_t w = a->param[2];
    if (ZERO(w))
	w = 1.0;
    uv.Set(a->param[0] / w, a->param[1] / w);
    return true;
}


/* Build the 2d trim curve, in the owning face's surface domain, for one
 * edgeuse of a NURBS face. */
ON_Curve *
snurb_edge_uv_curve(const struct edgeuse *eu)
{
    if (eu->g.magic_p && *eu->g.magic_p == NMG_EDGE_G_CNURB_MAGIC) {
	const struct edge_g_cnurb *c = eu->g.cnurb_p;
	if (c->c_size > 1 && c->ctl_points)
	    return cnurb_to_on_uv(c);
    }

    /* plinear or no explicit curve: straight line between the
     * parameter-space endpoints carried on the vertexuses */
    ON_2dPoint uv0, uv1;
    if (!vu_uv(eu->vu_p, uv0) || !vu_uv(eu->eumate_p->vu_p, uv1))
	return NULL;

    ON_LineCurve *lc = new ON_LineCurve(uv0, uv1);
    lc->SetDomain(0.0, 1.0);
    return lc;
}


/* Build a 3d edge curve for a NURBS face by pushing the 2d trim curve up
 * onto the surface (OpenNURBS in this build has no ON_Surface::PushUp, so
 * sample-and-polyline).  Endpoints are snapped to the exact brep vertices
 * so shared edges stay watertight. */
ON_Curve *
push_up_edge(const ON_Surface *srf, const ON_Curve *uv,
	     const ON_3dPoint &p_start, const ON_3dPoint &p_end, double tol)
{
    const int nsamp = 32;
    const ON_Interval dom = uv->Domain();
    const double mtol = (tol > 0.0) ? tol : SMALL_FASTF;

    /* sample the surface along the parameter-space curve */
    ON_3dPointArray raw(nsamp + 1);
    for (int i = 0; i <= nsamp; i++) {
	const double t = dom.ParameterAt((double)i / (double)nsamp);
	const ON_3dPoint p2 = uv->PointAt(t);
	raw.Append(srf->PointAt(p2.x, p2.y));
    }

    /* Keep only finite points, dropping any that coincide (within tol) with
     * the previous kept point, so the polyline has neither non-finite vertices
     * nor zero-length segments (either of which makes it invalid).  The
     * endpoints are forced to the exact brep vertices so shared edges stay
     * watertight. */
    ON_3dPointArray pts(raw.Count());
    pts.Append(p_start);
    for (int i = 1; i < raw.Count() - 1; i++) {
	if (!raw[i].IsValid())
	    continue;
	if (raw[i].DistanceTo(pts[pts.Count() - 1]) > mtol)
	    pts.Append(raw[i]);
    }
    if (p_end.DistanceTo(pts[pts.Count() - 1]) > mtol)
	pts.Append(p_end);
    else if (pts.Count() >= 2)
	pts[pts.Count() - 1] = p_end;	/* replace near-duplicate tail */
    else
	pts.Append(p_end);		/* degenerate edge: minimal 2-pt line */

    /* A straight two-point curve is a cleaner, more robust representation than
     * a degenerate polyline; and if the polyline somehow fails validation, fall
     * back to a line so a single edge can never invalidate the whole brep. */
    if (pts.Count() == 2)
	return new ON_LineCurve(pts[0], pts[1]);

    ON_PolylineCurve *pc = new ON_PolylineCurve(pts);
    if (pc->IsValid())
	return pc;
    delete pc;
    return new ON_LineCurve(p_start, p_end);
}


/* Build an ON_PlaneSurface for a planar NMG face, sized to the face's
 * loops.  Returns the surface; the plane is returned via @p plane_out for
 * use when projecting trim endpoints. */
ON_Surface *
plane_to_on(const struct faceuse *fu, ON_Plane &plane_out)
{
    const struct face_g_plane *fg = fu->f_p->g.plane_p;
    const ON_3dVector normal(fg->N[0], fg->N[1], fg->N[2]);
    const ON_3dPoint origin(fg->N[0] * fg->N[3],
			    fg->N[1] * fg->N[3],
			    fg->N[2] * fg->N[3]);
    ON_Plane plane(origin, normal);

    double umin = 1.0e300, umax = -1.0e300;
    double vmin = 1.0e300, vmax = -1.0e300;
    const struct loopuse *lu;
    const struct edgeuse *eu;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    double u, v;
	    plane.ClosestPointTo(ON_3dPoint(eu->vu_p->v_p->vg_p->coord), &u, &v);
	    V_MIN(umin, u); V_MAX(umax, u);
	    V_MIN(vmin, v); V_MAX(vmax, v);
	}
    }
    if (umin > umax || vmin > vmax)
	return NULL;

    ON_PlaneSurface *ps = new ON_PlaneSurface(plane);
    ps->SetExtents(0, ON_Interval(umin, umax), true);
    ps->SetExtents(1, ON_Interval(vmin, vmax), true);
    plane_out = plane;
    return ps;
}


/* Get (creating if needed) the brep vertex index for an NMG vertex. */
int
brep_vertex(ON_Brep *brep, const struct vertex *v, long *vmap)
{
    const long idx = v->vg_p->index;
    if (vmap[idx] < 0) {
	ON_BrepVertex &bv = brep->NewVertex(ON_3dPoint(v->vg_p->coord), SMALL_FASTF);
	vmap[idx] = bv.m_vertex_index;
    }
    return (int)vmap[idx];
}


/* Add one edgeuse to the current loop: a 2d trim, the shared 3d edge (built
 * once per NMG edge), and their vertices. */
bool
add_trim(ON_Brep *brep, ON_Surface *surf, const ON_Plane *plane,
	 ON_BrepFace &face, ON_BrepLoop &loop, const struct edgeuse *eu,
	 long *vmap, long *emap, const struct bn_tol *tol)
{
    const struct vertex *v0 = eu->vu_p->v_p;
    const struct vertex *v1 = eu->eumate_p->vu_p->v_p;
    const int vi0 = brep_vertex(brep, v0, vmap);
    const int vi1 = brep_vertex(brep, v1, vmap);

    /* 2d trim curve, running v0 -> v1, in this face's surface domain */
    ON_Curve *c2d = NULL;
    if (plane) {
	double u0, vv0, u1, vv1;
	plane->ClosestPointTo(ON_3dPoint(v0->vg_p->coord), &u0, &vv0);
	plane->ClosestPointTo(ON_3dPoint(v1->vg_p->coord), &u1, &vv1);
	c2d = new ON_LineCurve(ON_2dPoint(u0, vv0), ON_2dPoint(u1, vv1));
	c2d->SetDomain(0.0, 1.0);
    } else {
	c2d = snurb_edge_uv_curve(eu);
    }
    if (!c2d)
	return false;
    const int c2i = brep->m_C2.Count();
    brep->m_C2.Append(c2d);

    /* shared 3d edge, de-duplicated by NMG edge index and always stored
     * with the smaller vertex index first (matches rt_nmg_brep) */
    const long eidx = eu->e_p->index;
    if (emap[eidx] < 0) {
	const bool small_first = (v0->vg_p->index <= v1->vg_p->index);
	const int a = small_first ? vi0 : vi1;
	const int b = small_first ? vi1 : vi0;

	ON_Curve *c3d;
	if (plane) {
	    c3d = new ON_LineCurve(brep->m_V[a].Point(), brep->m_V[b].Point());
	} else {
	    /* push_up runs v0 -> v1; reverse if the edge is stored v1 -> v0 */
	    c3d = push_up_edge(surf, c2d, brep->m_V[vi0].Point(), brep->m_V[vi1].Point(), tol->dist);
	    if (a == vi1)
		c3d->Reverse();
	}
	c3d->SetDomain(0.0, 1.0);
	const int c3i = brep->AddEdgeCurve(c3d);
	ON_BrepEdge &e = brep->NewEdge(brep->m_V[a], brep->m_V[b], c3i);
	e.m_tolerance = plane ? 0.0 : tol->dist;
	emap[eidx] = e.m_edge_index;
    }

    ON_BrepEdge &edge = brep->m_E[(int)emap[eidx]];
    const bool rev3d = (edge.m_vi[0] != vi0);	/* c2d runs v0->v1 */
    ON_BrepTrim &trim = brep->NewTrim(edge, rev3d, loop, c2i);
    trim.m_type = ON_BrepTrim::mated;
    trim.m_tolerance[0] = tol->dist;
    trim.m_tolerance[1] = tol->dist;
    ON_Interval pd = trim.ProxyCurveDomain();
    trim.m_iso = surf->IsIsoparametric(*c2d, &pd);

    return true;
}


/* Convert one OT_SAME faceuse and its loops. */
bool
add_face(ON_Brep *brep, const struct faceuse *fu, long *vmap, long *emap,
	 const struct bn_tol *tol)
{
    const struct face *f = fu->f_p;
    if (!f->g.magic_p)
	return false;

    ON_Surface *surf = NULL;
    ON_Plane plane;
    const ON_Plane *plane_ptr = NULL;

    if (*f->g.magic_p == NMG_FACE_G_SNURB_MAGIC) {
	surf = snurb_to_on(f->g.snurb_p);
    } else if (*f->g.magic_p == NMG_FACE_G_PLANE_MAGIC) {
	surf = plane_to_on(fu, plane);
	plane_ptr = &plane;
    } else {
	bu_log("IGES: unsupported NMG face geometry, cannot build brep\n");
	return false;
    }
    if (!surf)
	return false;

    const int sidx = brep->AddSurface(surf);
    ON_BrepFace &face = brep->NewFace(sidx);
    face.m_bRev = f->flip ? true : false;

    const struct loopuse *lu;
    const struct edgeuse *eu;
    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
	if (BU_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC)
	    continue;	/* single-vertex loop, skip */
	const ON_BrepLoop::TYPE lt =
	    (lu->orientation == OT_SAME) ? ON_BrepLoop::outer : ON_BrepLoop::inner;
	ON_BrepLoop &loop = brep->NewLoop(lt, face);
	for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
	    if (!add_trim(brep, surf, plane_ptr, face, loop, eu, vmap, emap, tol))
		return false;
	}
    }

    return true;
}

} /* anonymous namespace */


extern "C" int
iges_nmg_to_brep(struct rt_wdb *wdbp, const char *name, struct model *m,
		 const struct bn_tol *tol)
{
    if (!wdbp || !name || !m)
	return 0;
    NMG_CK_MODEL(m);

    ON::Begin();

    ON_Brep *brep = ON_Brep::New();
    if (!brep)
	return 0;

    long *vmap = (long *)bu_malloc(m->maxindex * sizeof(long), "iges brep vmap");
    long *emap = (long *)bu_malloc(m->maxindex * sizeof(long), "iges brep emap");
    for (long i = 0; i < m->maxindex; i++) {
	vmap[i] = -1;
	emap[i] = -1;
    }

    bool ok = true;
    const struct nmgregion *r;
    const struct shell *s;
    const struct faceuse *fu;
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		NMG_CK_FACEUSE(fu);
		if (fu->orientation != OT_SAME)
		    continue;
		if (!add_face(brep, fu, vmap, emap, tol)) {
		    ok = false;
		    break;
		}
	    }
	    if (!ok)
		break;
	}
	if (!ok)
	    break;
    }

    bu_free(vmap, "iges brep vmap");
    bu_free(emap, "iges brep emap");

    if (!ok) {
	delete brep;
	return 0;
    }

    /* finalize derived data (iso/type flags, tolerances, bounding boxes) */
    brep->SetTrimIsoFlags();
    brep->SetTrimTypeFlags();
    brep->SetVertexTolerances(true);
    brep->SetTrimBoundingBoxes(true);

    ON_wString wonstr;
    ON_TextLog log(wonstr);
    if (!brep->IsValid(&log)) {
	ON_String onstr(wonstr);
	bu_log("IGES: brep for '%s' failed validation, will fall back:\n%s\n",
	       name, onstr.Array() ? onstr.Array() : "(no detail)");
	delete brep;
	return 0;
    }

    const int ret = mk_brep(wdbp, name, (void *)brep);
    delete brep;
    return (ret == 0) ? 1 : 0;
}


extern "C" int
iges_snurbs_to_brep(struct rt_wdb *wdbp, const char *name,
		    struct face_g_snurb **surfs, int count)
{
    if (!wdbp || !name || !surfs || count <= 0)
	return 0;

    ON::Begin();

    ON_Brep *brep = ON_Brep::New();
    if (!brep)
	return 0;

    int added = 0;
    for (int i = 0; i < count; i++) {
	if (!surfs[i])
	    continue;
	ON_NurbsSurface *ns = snurb_to_on(surfs[i]);
	if (!ns)
	    continue;
	/* the ON_Surface& overload copies the surface and builds a complete
	 * natural-boundary face (corner vertices, boundary edges, 4 trims) */
	if (brep->NewFace(*ns))
	    added++;
	delete ns;
    }

    if (!added) {
	delete brep;
	return 0;
    }

    brep->SetTrimIsoFlags();
    brep->SetTrimTypeFlags();
    brep->SetVertexTolerances(true);
    brep->SetTrimBoundingBoxes(true);

    ON_wString wonstr;
    ON_TextLog log(wonstr);
    if (!brep->IsValid(&log)) {
	ON_String onstr(wonstr);
	bu_log("IGES: surface brep for '%s' failed validation, will fall back:\n%s\n",
	       name, onstr.Array() ? onstr.Array() : "(no detail)");
	delete brep;
	return 0;
    }

    const int ret = mk_brep(wdbp, name, (void *)brep);
    delete brep;
    return (ret == 0) ? 1 : 0;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
