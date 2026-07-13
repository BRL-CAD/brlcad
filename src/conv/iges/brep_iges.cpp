/*                     B R E P _ I G E S . C P P
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
/** @file iges/brep_iges.cpp
 *
 * Faithful export of a BRL-CAD rt_brep (OpenNURBS ON_Brep) to IGES,
 * preserving the analytic NURBS geometry rather than tessellating it.
 *
 * Each ON_BrepFace becomes an IGES Trimmed (Parametric) Surface (entity
 * 144): its surface is written as a Rational B-Spline Surface (128), and
 * each trimming loop is written as a Curve on a Parametric Surface (142)
 * whose parameter-space and model-space curves are Rational B-Spline
 * Curves (126), joined by a Composite Curve (102) when a loop has more
 * than one segment.
 *
 * Any ON_Surface / ON_Curve is converted to NURBS form on the way out, so
 * planar, analytic, and free-form geometry are all handled uniformly.
 * The IGES importer (iges-g -t) reads these 144 entities back into a
 * trimmed-NURBS solid, which now round-trips to an rt_brep.
 */

#include "common.h"

#include <vector>

#include "brep.h"

#include "vmath.h"
#include "bn/tol.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"
#include "bu/log.h"

#include "./iges.h"


namespace {

/* Write an ON_Surface as an IGES 128 Rational B-Spline Surface; returns
 * its directory entry number, or 0 on failure. */
int
write_on_surface(const ON_Surface *srf, FILE *fp_dir, FILE *fp_param)
{
    ON_NurbsSurface ns;
    if (!srf->GetNurbForm(ns))
	return 0;

    const int k1 = ns.CVCount(0) - 1;
    const int k2 = ns.CVCount(1) - 1;
    const int m1 = ns.Order(0) - 1;
    const int m2 = ns.Order(1) - 1;
    const int nu = ns.CVCount(0);
    const int nv = ns.CVCount(1);
    const bool rat = ns.IsRational();

    /* IGES wants the full clamped knot vector (KnotCount + 2 values); ON
     * omits the two superfluous end knots, so add them back. */
    std::vector<double> uk(ns.KnotCount(0) + 2);
    uk[0] = ns.SuperfluousKnot(0, 0);
    for (int i = 0; i < ns.KnotCount(0); i++)
	uk[i + 1] = ns.Knot(0, i);
    uk[ns.KnotCount(0) + 1] = ns.SuperfluousKnot(0, 1);

    std::vector<double> vk(ns.KnotCount(1) + 2);
    vk[0] = ns.SuperfluousKnot(1, 0);
    for (int i = 0; i < ns.KnotCount(1); i++)
	vk[i + 1] = ns.Knot(1, i);
    vk[ns.KnotCount(1) + 1] = ns.SuperfluousKnot(1, 1);

    /* IGES order: u index fastest, then v.  Control points are euclidean;
     * weights are listed separately. */
    std::vector<double> wts((size_t)nu * nv);
    std::vector<double> pts((size_t)3 * nu * nv);
    for (int j = 0; j < nv; j++) {
	for (int i = 0; i < nu; i++) {
	    const int idx = j * nu + i;
	    ON_3dPoint p;
	    ns.GetCV(i, j, p);
	    pts[3 * idx + 0] = p.x;
	    pts[3 * idx + 1] = p.y;
	    pts[3 * idx + 2] = p.z;
	    wts[idx] = rat ? ns.Weight(i, j) : 1.0;
	}
    }

    const ON_Interval du = ns.Domain(0);
    const ON_Interval dv = ns.Domain(1);

    return write_nurb_surface_entity(k1, k2, m1, m2, rat ? 1 : 0,
				     &uk[0], &vk[0], &wts[0], &pts[0],
				     du.Min(), du.Max(), dv.Min(), dv.Max(),
				     fp_dir, fp_param);
}


/* Write an ON_Curve as an IGES 126 Rational B-Spline Curve; returns its
 * directory entry number, or 0 on failure. */
int
write_on_curve(const ON_Curve *crv, int planar, FILE *fp_dir, FILE *fp_param)
{
    ON_NurbsCurve nc;
    if (!crv->GetNurbForm(nc))
	return 0;

    const int k = nc.CVCount() - 1;
    const int m = nc.Order() - 1;
    const int n = nc.CVCount();
    const bool rat = nc.IsRational();

    std::vector<double> knots(nc.KnotCount() + 2);
    knots[0] = nc.SuperfluousKnot(0);
    for (int i = 0; i < nc.KnotCount(); i++)
	knots[i + 1] = nc.Knot(i);
    knots[nc.KnotCount() + 1] = nc.SuperfluousKnot(1);

    std::vector<double> wts(n);
    std::vector<double> pts((size_t)3 * n);
    for (int i = 0; i < n; i++) {
	ON_3dPoint p;
	nc.GetCV(i, p);		/* euclidean; z==0 for 2d parameter curves */
	pts[3 * i + 0] = p.x;
	pts[3 * i + 1] = p.y;
	pts[3 * i + 2] = p.z;
	wts[i] = rat ? nc.Weight(i) : 1.0;
    }

    const ON_Interval dom = nc.Domain();

    return write_nurb_curve_entity(k, m, rat ? 1 : 0, planar,
				   &knots[0], &wts[0], &pts[0],
				   dom.Min(), dom.Max(),
				   0.0, 0.0, 1.0, fp_dir, fp_param);
}


/* Write one trimming loop as a Curve on a Parametric Surface (142),
 * building the parameter-space and model-space boundary curves (joining
 * segments with a 102 Composite Curve as needed).  Returns the 142 DE, or
 * 0 on failure. */
int
write_loop(const ON_BrepLoop *loop, int surf_de, FILE *fp_dir, FILE *fp_param)
{
    const int nt = loop->TrimCount();
    if (nt <= 0)
	return 0;

    std::vector<int> bde;	/* parameter-space (2d) curve DEs */
    std::vector<int> cde;	/* model-space (3d) curve DEs */

    for (int ti = 0; ti < nt; ti++) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	if (!trim)
	    continue;
	const ON_Curve *c2 = trim->TrimCurveOf();
	if (!c2)
	    continue;
	const int b = write_on_curve(c2, 1, fp_dir, fp_param);
	if (!b)
	    continue;

	int c = 0;
	const ON_BrepEdge *edge = trim->Edge();
	if (edge) {
	    const ON_Curve *c3 = edge->EdgeCurveOf();
	    if (c3)
		c = write_on_curve(c3, 0, fp_dir, fp_param);
	}
	if (!c)
	    c = b;		/* singular trim: iges-g uses only the 2d curve */

	bde.push_back(b);
	cde.push_back(c);
    }

    if (bde.empty())
	return 0;

    const int bcurve = (bde.size() > 1)
	? write_composite_curve_entity(&bde[0], (int)bde.size(), fp_dir, fp_param)
	: bde[0];
    const int ccurve = (cde.size() > 1)
	? write_composite_curve_entity(&cde[0], (int)cde.size(), fp_dir, fp_param)
	: cde[0];

    return write_curve_on_surface_entity(surf_de, bcurve, ccurve, fp_dir, fp_param);
}

/* Walk an ON_Brep and write each face as a Trimmed Surface (144), with its
 * surface as a 128 and its loops as 142/126/102.  Returns the DE of the
 * last face written, or 0 if nothing usable was written. */
int
write_on_brep(ON_Brep *brep, FILE *fp_dir, FILE *fp_param)
{
    if (!brep)
	return 0;

    int last_de = 0;
    for (int fi = 0; fi < brep->m_F.Count(); fi++) {
	const ON_BrepFace &face = brep->m_F[fi];
	const ON_Surface *srf = face.SurfaceOf();
	if (!srf)
	    continue;

	const int surf_de = write_on_surface(srf, fp_dir, fp_param);
	if (!surf_de) {
	    bu_log("brep_to_iges: face %d surface not representable, skipped\n", fi);
	    continue;
	}

	int outer_de = 0;
	std::vector<int> inner_des;
	for (int li = 0; li < face.LoopCount(); li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    if (!loop)
		continue;
	    const int cos_de = write_loop(loop, surf_de, fp_dir, fp_param);
	    if (!cos_de)
		continue;
	    if (loop->m_type == ON_BrepLoop::inner)
		inner_des.push_back(cos_de);
	    else
		outer_de = cos_de;
	}

	if (!outer_de) {
	    bu_log("brep_to_iges: face %d has no usable outer loop, skipped\n", fi);
	    continue;
	}

	last_de = write_trimmed_surface_entity(surf_de, outer_de,
					       inner_des.empty() ? NULL : &inner_des[0],
					       (int)inner_des.size(),
					       fp_dir, fp_param);
    }

    return last_de;
}


} /* anonymous namespace */


/* g-iges globals / helpers (C linkage). */
extern "C" int solid_is_brep;
extern "C" int nmg_to_iges(struct rt_db_internal *, char *, FILE *, FILE *, struct bu_list *);


extern "C" int
brep_to_iges(struct rt_db_internal *ip, char *name,
	     FILE *fp_dir, FILE *fp_param, struct bu_list *vlfree)
{
    (void)vlfree;

    RT_CK_DB_INTERNAL(ip);
    struct rt_brep_internal *bi = (struct rt_brep_internal *)ip->idb_ptr;
    if (!bi || bi->magic != RT_BREP_INTERNAL_MAGIC || !bi->brep) {
	bu_log("brep_to_iges: %s is not a valid brep\n", name ? name : "(null)");
	return 0;
    }

    ON::Begin();
    solid_is_brep = 1;

    const int de = write_on_brep(bi->brep, fp_dir, fp_param);
    if (!de) {
	bu_log("brep_to_iges: no faces written for %s\n", name ? name : "(null)");
	return 0;
    }
    return de;
}


/* Export a non-brep primitive by first converting it to an ON_Brep via its
 * ft_brep callback, so curved analytic primitives (eto, rpc, rhc, epa, ehy,
 * hyp, superell, revolve, particle, ...) are written as faithful NURBS
 * surfaces rather than being dropped or faceted.  Falls back to the faceted
 * NMG exporter when no brep callback exists or the brep is unusable. */
extern "C" int
primitive_brep_to_iges(struct rt_db_internal *ip, char *name,
		       FILE *fp_dir, FILE *fp_param, struct bu_list *vlfree)
{
    RT_CK_DB_INTERNAL(ip);

    if (ip->idb_type > 0 && ip->idb_type <= ID_MAXIMUM && OBJ[ip->idb_type].ft_brep) {
	struct bn_tol tol;
	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.0005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1.0e-6;
	tol.para = 1.0 - tol.perp;

	ON::Begin();

	ON_Brep *brep = NULL;
	OBJ[ip->idb_type].ft_brep(&brep, ip, &tol);
	if (brep) {
	    if (brep->IsValid()) {
		solid_is_brep = 1;
		const int de = write_on_brep(brep, fp_dir, fp_param);
		delete brep;
		if (de)
		    return de;
	    } else {
		bu_log("primitive_brep_to_iges: %s brep invalid, tessellating instead\n",
		       name ? name : "(null)");
		delete brep;
	    }
	}
    }

    /* fall back to a faceted (tessellated) export */
    return nmg_to_iges(ip, name, fp_dir, fp_param, vlfree);
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
