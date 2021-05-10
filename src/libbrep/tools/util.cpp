/*                        U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
/** @file util.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <set>
#include <map>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bn/mat.h" /* bn_vec_perp */
#include "bn/tol.h"
#include "bview/plot3.h"
#include "brep/defines.h"
#include "brep/util.h"

void
ON_MinMaxInit(ON_3dPoint *min, ON_3dPoint *max)
{
    min->x = ON_DBL_MAX;
    min->y = ON_DBL_MAX;
    min->z = ON_DBL_MAX;
    max->x = -ON_DBL_MAX;
    max->y = -ON_DBL_MAX;
    max->z = -ON_DBL_MAX;
}


#define LN_DOTP_TOL 0.000001
ON_3dPoint
ON_LinePlaneIntersect(ON_Line &line, ON_Plane &plane)
{
    ON_3dPoint result;
    result.x = ON_DBL_MAX;
    result.y = ON_DBL_MAX;
    result.z = ON_DBL_MAX;
    ON_3dVector n = plane.Normal();
    ON_3dVector l = line.Direction();
    ON_3dPoint l0 = line.PointAt(0.5);
    ON_3dPoint p0 = plane.Origin();

    ON_3dVector p0l0 = p0 - l0;
    double p0l0n = ON_DotProduct(p0l0, n);
    double ln = ON_DotProduct(l, n);
    if (NEAR_ZERO(ln, LN_DOTP_TOL) && NEAR_ZERO(p0l0n, LN_DOTP_TOL)) {
	result.x = -ON_DBL_MAX;
	result.y = -ON_DBL_MAX;
	result.z = -ON_DBL_MAX;
	return result;
    }
    if (NEAR_ZERO(ln, LN_DOTP_TOL)) return result;
    double d = p0l0n/ln;

    result = d*l + l0;
    return result;
}


// Tikz LaTeX output from B-Rep wireframes
int
ON_BrepTikz(ON_String& s, const ON_Brep *brep, const char *c, const char *pre)
{
    struct bu_vls color = BU_VLS_INIT_ZERO;
    struct bu_vls output = BU_VLS_INIT_ZERO;
    struct bu_vls prefix = BU_VLS_INIT_ZERO;
    if (c) {
	bu_vls_sprintf(&color, "%s", c);
    } else {
	bu_vls_sprintf(&color, "gray");
    }
    if (pre) {
	bu_vls_sprintf(&prefix, "%s", pre);
    } else {
	bu_vls_trunc(&prefix, 0);
    }

    for (int i = 0; i < brep->m_V.Count(); i++) {
	bu_vls_printf(&output, "\\coordinate (%sV%d) at (%f, %f, %f);\n", bu_vls_addr(&prefix), i, brep->m_V[i].Point().x, brep->m_V[i].Point().y, brep->m_V[i].Point().z);
    }

    for (int i = 0; i < brep->m_E.Count(); i++) {
	const ON_BrepEdge *edge = &(brep->m_E[i]);
	//int ei = edge->m_edge_index;
	ON_Curve *ecv = edge->EdgeCurveOf()->Duplicate();
	if (ecv->IsLinear()) {
	    bu_vls_printf(&output, "\\draw[%s] (%sV%d) -- (%sV%d);\n", bu_vls_addr(&color), bu_vls_addr(&prefix), edge->Vertex(0)->m_vertex_index, bu_vls_addr(&prefix), edge->Vertex(1)->m_vertex_index);
	    delete ecv;
	    continue;
	}
	ecv = edge->EdgeCurveOf()->Duplicate();
	ON_Polyline poly;
	ON_Curve *ncv = edge->EdgeCurveOf()->Duplicate();
	int pnt_cnt = ON_Curve_PolyLine_Approx(&poly, ncv, BN_TOL_DIST);
	if (pnt_cnt) {
	    if (ecv->IsPolyline()) {
		ON_3dPoint p = poly[0];
		if (pnt_cnt) {
		    bu_vls_printf(&output, "\\draw[%s] (%f, %f, %f)", bu_vls_addr(&color), p.x, p.y, p.z);
		    for (int si = 1; si < poly.Count(); si++) {
			p = poly[si];
			bu_vls_printf(&output, " -- (%f, %f, %f)", p.x, p.y, p.z);
			if (si+1 == poly.Count()) {
			    bu_vls_printf(&output, ";\n");
			}
		    }
		}
	    } else {
		bu_vls_printf(&output, "\\draw[%s] plot [smooth] coordinates {", bu_vls_addr(&color));
		for (int si = 0; si < poly.Count(); si++) {
		    ON_3dPoint p = poly[si];
		    bu_vls_printf(&output, "(%f, %f, %f) ", p.x, p.y, p.z);
		}
		bu_vls_printf(&output, "};\n");
	    }
	}
	delete ecv;
	delete ncv;
    }

    s.Append(bu_vls_addr(&output), bu_vls_strlen(&output));
    bu_vls_free(&color);
    bu_vls_free(&output);
    bu_vls_free(&prefix);
    return 1;
}

#define TREE_LEAF_FACE_3D(pf, valp, a, b, c, d)  \
    pdv_3move(pf, pt[a]); \
    pdv_3cont(pf, pt[b]); \
    pdv_3cont(pf, pt[c]); \
    pdv_3cont(pf, pt[d]); \
    pdv_3cont(pf, pt[a]); \

void
ON_BoundingBox_Plot(FILE *pf, ON_BoundingBox &bb)
{
    fastf_t pt[8][3];
    point_t min, max;
    min[0] = bb.Min().x;
    min[1] = bb.Min().y;
    min[2] = bb.Min().z;
    max[0] = bb.Max().x;
    max[1] = bb.Max().y;
    max[2] = bb.Max().z;
    VSET(pt[0], max[X], min[Y], min[Z]);
    VSET(pt[1], max[X], max[Y], min[Z]);
    VSET(pt[2], max[X], max[Y], max[Z]);
    VSET(pt[3], max[X], min[Y], max[Z]);
    VSET(pt[4], min[X], min[Y], min[Z]);
    VSET(pt[5], min[X], max[Y], min[Z]);
    VSET(pt[6], min[X], max[Y], max[Z]);
    VSET(pt[7], min[X], min[Y], max[Z]);
    TREE_LEAF_FACE_3D(pf, pt, 0, 1, 2, 3);
    TREE_LEAF_FACE_3D(pf, pt, 4, 0, 3, 7);
    TREE_LEAF_FACE_3D(pf, pt, 5, 4, 7, 6);
    TREE_LEAF_FACE_3D(pf, pt, 1, 5, 6, 2);
}

void
ON_Plane_Plot(FILE *pf, ON_Plane &plane)
{
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(pf, r, g, b);
    ON_3dPoint ocenter = plane.Origin();
    ON_3dVector onorm = plane.Normal();
    point_t center, norm;

    VSET(center, ocenter.x, ocenter.y, ocenter.z);
    VSET(norm, onorm.x, onorm.y, onorm.z);

    vect_t xbase, ybase, tip;
    vect_t x_1, x_2, y_1, y_2;
    bn_vec_perp(xbase, norm);
    VCROSS(ybase, xbase, norm);
    VUNITIZE(xbase);
    VUNITIZE(ybase);
    VSCALE(xbase, xbase, 10);
    VSCALE(ybase, ybase, 10);
    VADD2(x_1, center, xbase);
    VSUB2(x_2, center, xbase);
    VADD2(y_1, center, ybase);
    VSUB2(y_2, center, ybase);

    pdv_3move(pf, x_1);
    pdv_3cont(pf, x_2);
    pdv_3move(pf, y_1);
    pdv_3cont(pf, y_2);

    pdv_3move(pf, x_1);
    pdv_3cont(pf, y_1);
    pdv_3move(pf, x_2);
    pdv_3cont(pf, y_2);

    pdv_3move(pf, x_2);
    pdv_3cont(pf, y_1);
    pdv_3move(pf, x_1);
    pdv_3cont(pf, y_2);

    VSCALE(tip, norm, 5);
    VADD2(tip, center, tip);
    pdv_3move(pf, center);
    pdv_3cont(pf, tip);
}

int
brep_translate_scv(
        ON_Brep *brep,
        int surface_index,
        int i,
        int j,
        fastf_t dx,
        fastf_t dy,
        fastf_t dz)
{
    ON_NurbsSurface *nurbsSurface = NULL;
    if (surface_index < 0 || surface_index >= brep->m_S.Count()) {
        bu_log("brep_translate_scv: invalid surface index %d\n", surface_index);
        return -1;
    }

    ON_Surface *surface = brep->m_S[surface_index];
    if (surface) {
        nurbsSurface = dynamic_cast<ON_NurbsSurface *>(surface);
    } else {
        return -1;
    }

    double *cv = NULL;
    if (nurbsSurface) {
        cv = nurbsSurface->CV(i, j);
    } else {
        return -2;
    }

    if (cv) {
        ON_3dPoint newPt;
        newPt.x = cv[X] + dx;
        newPt.y = cv[Y] + dy;
        newPt.z = cv[Z] + dz;
        nurbsSurface->SetCV(i, j, newPt);
    } else {
        return -3;
    }

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

