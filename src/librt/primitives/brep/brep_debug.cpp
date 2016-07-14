/*                     B R E P _ D E B U G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2014 United States Government as represented by
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
/** @addtogroup g_ */
/** @{ */
/** @file brep_debug.cpp
 *
 * brep debugging utilities
 *
 */

#include "common.h"
#include "bu.h"

#include <vector>
#include <list>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

#include "poly2tri/poly2tri.h"

#include "vmath.h"
#include "plot3.h"
#include "brep.h"
#include "brep_debug.h"
#include "dvec.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

#include "brep_local.h"

#ifdef __cplusplus
extern "C" {
#endif
    RT_EXPORT extern int brep_command(struct bu_vls *vls, const char *solid_name, const struct rt_tess_tol* ttol, const struct bn_tol* tol, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int argc, const char *argv[], char *commtag);
    extern int single_conversion(struct rt_db_internal* intern, ON_Brep** brep, const struct db_i *dbip);
    RT_EXPORT extern int brep_conversion(struct rt_db_internal* in, struct rt_db_internal* out, const struct db_i *dbip);
    RT_EXPORT extern int brep_conversion_comb(struct rt_db_internal *old_internal, const char *name, const char *suffix, struct rt_wdb *wdbp, fastf_t local2mm);
    RT_EXPORT extern int brep_intersect_point_point(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_point_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_point_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_curve_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_curve_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
    RT_EXPORT extern int brep_intersect_surface_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j, struct bn_vlblock *vbp);
    extern void rt_comb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *UNUSED(tol), const struct db_i *dbip);
#ifdef __cplusplus
}
#endif

extern void poly2tri_CDT(struct bu_list *vhead, ON_BrepFace &face, const struct rt_tess_tol *ttol, const struct bn_tol *tol, const struct rt_view_info *info, bool watertight = false, int plottype = 0, int num_points = -1.0);

/********************************************************************************
 * Auxiliary functions
 ********************************************************************************/

using namespace brlcad;

FILE*
brep_plot_file(const char *pname)
{
    static FILE* plot = NULL;

    if (plot != NULL) {
	(void)fclose(plot);
	plot = NULL;
    }
    if (pname == NULL) {
	pname = "out.pl";
    }
    plot = fopen(pname, "w");
    point_t min, max;
    VSET(min, -2048, -2048, -2048);
    VSET(max, 2048, 2048, 2048);
    pdv_3space(plot, min, max);

    return plot;
}


#define ARB_FACE(valp, a, b, c, d)			\
    RT_ADD_VLIST(vhead, valp[a], BN_VLIST_LINE_MOVE);	\
    RT_ADD_VLIST(vhead, valp[b], BN_VLIST_LINE_DRAW);	\
    RT_ADD_VLIST(vhead, valp[c], BN_VLIST_LINE_DRAW);	\
    RT_ADD_VLIST(vhead, valp[d], BN_VLIST_LINE_DRAW);

#define BB_PLOT_VLIST(min, max) {		\
	fastf_t pt[8][3];			\
	VSET(pt[0], max[X], min[Y], min[Z]);	\
	VSET(pt[1], max[X], max[Y], min[Z]);	\
	VSET(pt[2], max[X], max[Y], max[Z]);	\
	VSET(pt[3], max[X], min[Y], max[Z]);	\
	VSET(pt[4], min[X], min[Y], min[Z]);	\
	VSET(pt[5], min[X], max[Y], min[Z]);	\
	VSET(pt[6], min[X], max[Y], max[Z]);	\
	VSET(pt[7], min[X], min[Y], max[Z]);	\
	ARB_FACE(pt, 0, 1, 2, 3);		\
	ARB_FACE(pt, 4, 0, 3, 7);		\
	ARB_FACE(pt, 5, 4, 7, 6);		\
	ARB_FACE(pt, 1, 5, 6, 2);		\
    }

void
plotsurfaceleafs(SurfaceTree* surf) {
    vect_t min;
    vect_t max;
    std::list<BBNode*> leaves;
    surf->getLeaves(leaves);

    for (std::list<BBNode*>::iterator i = leaves.begin(); i != leaves.end(); i++) {
	BBNode* bb = dynamic_cast<BBNode*>(*i);
	if (bb->m_trimmed) {
	    COLOR_PLOT(255, 0, 0);
	} else if (bb->m_checkTrim) {
	    COLOR_PLOT(0, 0, 255);
	} else {
	    COLOR_PLOT(255, 0, 255);
	}
	/*
	  if (bb->m_xgrow) {
	  M_COLOR_PLOT(PURERED);
	  } else if (bb->m_ygrow) {
	  M_COLOR_PLOT(GREEN);
	  } else if (bb->m_zgrow) {
	  M_COLOR_PLOT(BLUE);
	  } else {
	  COLOR_PLOT(100, 100, 100);
	  }
	*/
	//if ((!bb->m_trimmed) && (!bb->m_checkTrim)) {
	if (false) {
	    //bb->GetBBox(min, max);
	} else {
	    VSET(min, bb->m_u[0]+0.001, bb->m_v[0]+0.001, 0.0);
	    VSET(max, bb->m_u[1]-0.001, bb->m_v[1]-0.001, 0.0);
	    //VSET(min, bb->m_u[0], bb->m_v[0], 0.0);
	    //VSET(max, bb->m_u[1], bb->m_v[1], 0.0);
	}
	BB_PLOT(min, max);
	//}
    }
    return;
}


unsigned int
plotsurfaceleafs(SurfaceTree* surf, struct bn_vlblock *vbp, bool dim3d)
{
    register struct bu_list *vhead;
    fastf_t min[3], max[3];
    std::list<BBNode*> leaves;
    surf->getLeaves(leaves);

    VSETALL(min, 0.0);

    ON_TextLog tl(stderr);

    vhead = rt_vlblock_find(vbp, PURERED);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);
    vhead = rt_vlblock_find(vbp, BLUE);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);
    vhead = rt_vlblock_find(vbp, MAGENTA);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);

    for (std::list<BBNode*>::iterator i = leaves.begin(); i != leaves.end(); i++) {
	BBNode* bb = dynamic_cast<BBNode*>(*i);
	if (bb->m_trimmed) {
	    vhead = rt_vlblock_find(vbp, PURERED);
	} else if (bb->m_checkTrim) {
	    vhead = rt_vlblock_find(vbp, BLUE);
	} else {
	    vhead = rt_vlblock_find(vbp, MAGENTA);
	}
	if (dim3d) {
	    bb->GetBBox(min, max);
	} else {
	    VSET(min, bb->m_u[0]+0.001, bb->m_v[0]+0.001, 0.0);
	    VSET(max, bb->m_u[1]-0.001, bb->m_v[1]-0.001, 0.0);
	}
	BB_PLOT_VLIST(min, max);
    }

    return leaves.size();
}


void
plottrimleafs(SurfaceTree* st, struct bn_vlblock *vbp, bool dim3d)
{
    register struct bu_list *vhead;
    vect_t min;
    vect_t max;
    std::list<BRNode*> leaves;
    st->ctree->getLeaves(leaves);

    VSETALL(min, 0.0);

    ON_TextLog tl(stderr);

    vhead = rt_vlblock_find(vbp, PURERED);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);
    vhead = rt_vlblock_find(vbp, BLUE);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);
    vhead = rt_vlblock_find(vbp, MAGENTA);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);

    for (std::list<BRNode*>::iterator i = leaves.begin(); i != leaves.end(); i++) {
	BRNode* bb = dynamic_cast<BRNode*>(*i);
	if (bb->m_XIncreasing) {
	    vhead = rt_vlblock_find(vbp, GREEN);
	} else {
	    vhead = rt_vlblock_find(vbp, BLUE);
	}
	bb->GetBBox(min, max);
	if (dim3d) {
	    const ON_Surface *surf = st->getSurface();

	    ON_3dPoint p1 = surf->PointAt(min[0], min[1]);
	    ON_3dPoint p2 = surf->PointAt(max[0], max[1]);
	    VMOVE(min, p1);
	    VMOVE(max, p2);
	}
	BB_PLOT_VLIST(min, max);
    }

    return;
}


void
plotleaf3d(BBNode* bb)
{
    vect_t min;
    vect_t max;
    fastf_t u, v;
    ON_2dPoint uv[2];
    int trim1_status;
    int trim2_status;
    fastf_t closesttrim1;
    fastf_t closesttrim2;

    if (bb->m_trimmed) {
	COLOR_PLOT(255, 0, 0);
    } else if (bb->m_checkTrim) {
	COLOR_PLOT(0, 0, 255);
    } else {
	COLOR_PLOT(255, 0, 255);
    }

    if (true) {
	bb->GetBBox(min, max);
    } else {
	// VSET(min, bb->m_u[0]+0.001, bb->m_v[0]+0.001, 0.0);
	// VSET(max, bb->m_u[1]-0.001, bb->m_v[1]-0.001, 0.0);
    }
    BB_PLOT(min, max);

    M_COLOR_PLOT(YELLOW);
    point_t a, b;
    ON_3dPoint p;
    BRNode* trimBR = NULL;
    const ON_BrepFace* f = bb->m_face;
    const ON_Surface* surf = f->SurfaceOf();
    fastf_t uinc = (bb->m_u[1] - bb->m_u[0])/100.0;
    fastf_t vinc = (bb->m_v[1] - bb->m_v[0])/100.0;
    for (u=bb->m_u[0];u<bb->m_u[1];u=u+uinc) {
	for (v=bb->m_v[0];v<bb->m_v[1];v=v+vinc) {
	    uv[0].x = u;
	    uv[0].y = v;
	    uv[1].x = u+uinc;
	    uv[1].y = v+vinc;
	    trim1_status = bb->isTrimmed(uv[0], &trimBR, closesttrim1);
	    trim2_status = bb->isTrimmed(uv[1], &trimBR, closesttrim2);

	    if (((trim1_status != 1) || (fabs(closesttrim1) < BREP_EDGE_MISS_TOLERANCE)) &&
		((trim2_status != 1) || (fabs(closesttrim2) < BREP_EDGE_MISS_TOLERANCE))) {
		p = surf->PointAt(uv[0].x, uv[0].y);
		VMOVE(a, p);
		p = surf->PointAt(uv[1].x, uv[1].y);
		VMOVE(b, p);
		LINE_PLOT(a, b);
	    }
	}
    }

    return;
}


void
plotleafuv(BBNode* bb)
{
    vect_t min;
    vect_t max;

    if (bb->m_trimmed) {
	COLOR_PLOT(255, 0, 0);
    } else if (bb->m_checkTrim) {
	COLOR_PLOT(0, 0, 255);
    } else {
	COLOR_PLOT(255, 0, 255);
    }

    if (false) {
	// bb->GetBBox(min, max);
    } else {
	VSET(min, bb->m_u[0]+0.001, bb->m_v[0]+0.001, 0.0);
	VSET(max, bb->m_u[1]-0.001, bb->m_v[1]-0.001, 0.0);
    }
    BB_PLOT(min, max);

    return;
}


void
plottrim(ON_BrepFace &face, struct bn_vlblock *vbp, int plotres, bool dim3d)
{
    register struct bu_list *vhead;
    const ON_Surface* surf = face.SurfaceOf();
    fastf_t umin, umax;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    ON_TextLog tl(stderr);

    vhead = rt_vlblock_find(vbp, YELLOW);

    surf->GetDomain(0, &umin, &umax);
    for (int i = 0; i < face.LoopCount(); i++) {
	ON_BrepLoop* loop = face.Loop(i);
	// for each trim
	for (int j = 0; j < loop->m_ti.Count(); j++) {
	    ON_BrepTrim& trim = face.Brep()->m_T[loop->m_ti[j]];
	    const ON_Curve* trimCurve = trim.TrimCurveOf();
	    //trimCurve->Dump(tl);

	    ON_Interval dom = trimCurve->Domain();
	    // XXX todo: dynamically sample the curve
	    for (int k = 1; k <= plotres; k++) {
		ON_3dPoint p = trimCurve->PointAt(dom.ParameterAt((double) (k - 1) / (double) plotres));
		if (dim3d)
		    p = surf->PointAt(p.x, p.y);
		VMOVE(pt1, p);
		p = trimCurve->PointAt(dom.ParameterAt((double) k / (double) plotres));
		if (dim3d)
		    p = surf->PointAt(p.x, p.y);
		VMOVE(pt2, p);
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	    }
	}
    }

    return;
}


void
plottrim2d(ON_BrepFace &face, struct bn_vlblock *vbp, int plotres)
{
    register struct bu_list *vhead;
    const ON_Surface* surf = face.SurfaceOf();
    fastf_t umin, umax;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    ON_TextLog tl(stderr);

    vhead = rt_vlblock_find(vbp, YELLOW);

    surf->GetDomain(0, &umin, &umax);
    for (int i = 0; i < face.LoopCount(); i++) {
	ON_BrepLoop* loop = face.Loop(i);
	// for each trim
	for (int j = 0; j < loop->m_ti.Count(); j++) {
	    ON_BrepTrim& trim = face.Brep()->m_T[loop->m_ti[j]];
	    const ON_Curve* trimCurve = trim.TrimCurveOf();
	    //trimCurve->Dump(tl);

	    ON_Interval dom = trimCurve->Domain();
	    // XXX todo: dynamically sample the curve
	    for (int k = 1; k <= plotres; k++) {
		ON_3dPoint p = trimCurve->PointAt(dom.ParameterAt((double) (k - 1) / (double) plotres));
		//p = surf->PointAt(p.x, p.y);
		VMOVE(pt1, p);
		p = trimCurve->PointAt(dom.ParameterAt((double) k / (double) plotres));
		//p = surf->PointAt(p.x, p.y);
		VMOVE(pt2, p);
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	    }
	}
    }

    return;
}


void
plotUVDomain2d(ON_BrepFace &face, struct bn_vlblock *vbp)
{
    register struct bu_list *vhead;
    const ON_Surface* surf = face.SurfaceOf();
    fastf_t umin, umax, urange;
    fastf_t vmin, vmax, vrange;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    ON_TextLog tl(stderr);

    vhead = rt_vlblock_find(vbp, PURERED);

    double width, height;
    ON_BoundingBox loop_bb;
    ON_BoundingBox trim_bb;
#ifndef RESETDOMAIN
    if (face.GetSurfaceSize(&width, &height)) {
	face.SetDomain(0, 0.0, width);
	face.SetDomain(1, 0.0, height);

    }
#endif
    surf->GetDomain(0, &umin, &umax);
    surf->GetDomain(1, &vmin, &vmax);
    // add a little offset so we can see the boundary curves
    urange = umax - umin;
    vrange = vmax - vmin;
    umin = umin - 0.01*urange;
    vmin = vmin - 0.01*vrange;
    umax = umax + 0.01*urange;
    vmax = vmax + 0.01*vrange;

    //umin
    VSET(pt1, umin, vmin, 0.0);
    VSET(pt2, umin, vmax, 0.0);
    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
    // umax
    VSET(pt1, umax, vmin, 0.0);
    VSET(pt2, umax, vmax, 0.0);
    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
    //vmin
    VSET(pt1, umin, vmin, 0.0);
    VSET(pt2, umax, vmin, 0.0);
    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
    //vmax
    VSET(pt1, umin, vmax, 0.0);
    VSET(pt2, umax, vmax, 0.0);
    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);

    return;
}


void
plottrim(ON_BrepTrim& trim, struct bn_vlblock *vbp, int plotres, bool dim3d)
{
    register struct bu_list *vhead;
    ON_BrepFace *face = trim.Face();
    point_t pt;

    ON_TextLog tl(stderr);

    vhead = rt_vlblock_find(vbp, YELLOW);

    const ON_Curve* trimCurve = trim.TrimCurveOf();
    ON_Interval dom = trimCurve->Domain();

    //trimCurve->Dump(tl);
    for (int k = 0; k <= plotres; ++k) {
	ON_3dPoint p = trimCurve->PointAt(dom.ParameterAt((double)k / plotres));

	if (dim3d) {
	    p = face->PointAt(p.x, p.y);
	}
	VMOVE(pt, p);

	if (k != 0) {
	    RT_ADD_VLIST(vhead, pt, BN_VLIST_LINE_DRAW);
	} else {
	    RT_ADD_VLIST(vhead, pt, BN_VLIST_LINE_MOVE);
	}
    }
    return;
}


void
plottrimdirection(ON_BrepFace &face, struct bn_vlblock *vbp, int plotres)
{
    register struct bu_list *vhead;
    const ON_Surface* surf = face.SurfaceOf();
    fastf_t umin, umax;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    ON_TextLog tl(stderr);

    vhead = rt_vlblock_find(vbp, GREEN);

    surf->GetDomain(0, &umin, &umax);
    for (int i = 0; i < face.LoopCount(); i++) {
	ON_BrepLoop* loop = face.Loop(i);
	// for each trim
	for (int j = 0; j < loop->m_ti.Count(); j++) {
	    ON_BrepTrim& trim = face.Brep()->m_T[loop->m_ti[j]];
	    const ON_Curve* trimCurve = trim.TrimCurveOf();
	    //trimCurve->Dump(tl);

	    int knotcnt = trimCurve->SpanCount();
	    fastf_t *knots = new fastf_t[knotcnt + 1];

	    trimCurve->GetSpanVector(knots);
	    for (int k = 1; k <= knotcnt; k++) {
		fastf_t dist = knots[k] - knots[k-1];
		fastf_t step = dist/plotres;
		for (fastf_t t=knots[k-1]+step; t<=knots[k]; t=t+step) {
		    ON_3dPoint p = trimCurve->PointAt(t);
		    p = surf->PointAt(p.x, p.y);
		    ON_3dPoint prev = trimCurve->PointAt(t-step*0.1);
		    prev = surf->PointAt(prev.x, prev.y);
		    ON_3dVector N = surf->NormalAt(p.x, p.y);
		    N.Unitize();
		    ON_3dVector tan = p - prev;
		    tan.Unitize();
		    prev = p - tan;
		    ON_3dVector A = ON_CrossProduct(tan, N);
		    A.Unitize();
		    ON_3dVector B = ON_CrossProduct(N, tan);
		    B.Unitize();
		    ON_3dPoint a = prev + A;
		    ON_3dPoint b = prev + B;
		    VMOVE(pt1, p);
		    VMOVE(pt2, a);
		    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		    VMOVE(pt2, b);
		    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	    }
	}
    }

    return;
}


void
plotsurface(ON_Surface &surf, struct bn_vlblock *vbp, int isocurveres, int gridres, const int red = 200, const int green = 200, const int blue = 200)
{
    register struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;
    fastf_t hsv[3];
    unsigned char fill_rgb[3];

    VSET(fill_rgb,(unsigned char)red,(unsigned char)green,(unsigned char)blue);
    bu_rgb_to_hsv(fill_rgb,hsv);
    // simply fill with 50% lightness/value of outline
    hsv[2] = hsv[2] * 0.5;
    bu_hsv_to_rgb(hsv,fill_rgb);


    vhead = rt_vlblock_find(vbp, red, green, blue);

    ON_Interval udom = surf.Domain(0);
    ON_Interval vdom = surf.Domain(1);

    for (int u = 0; u <= gridres; u++) {
	if (u == 0 || u == gridres) {
	    vhead = rt_vlblock_find(vbp, red, green, blue);
	} else {
	    vhead = rt_vlblock_find(vbp, (int)(fill_rgb[0]), (int)(fill_rgb[1]), (int)(fill_rgb[2]));
	}
	for (int v = 1; v <= isocurveres; v++) {
	    ON_3dPoint p = surf.PointAt(udom.ParameterAt((double)u/(double)gridres), vdom.ParameterAt((double)(v-1)/(double)isocurveres));
	    VMOVE(pt1, p);
	    p = surf.PointAt(udom.ParameterAt((double)u/(double)gridres), vdom.ParameterAt((double)v/(double)isocurveres));
	    VMOVE(pt2, p);
	    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}
    }

    for (int v = 0; v <= gridres; v++) {
	if (v == 0 || v == gridres) {
	    vhead = rt_vlblock_find(vbp, red, green, blue);
	} else {
	    vhead = rt_vlblock_find(vbp, (int)(fill_rgb[0]), (int)(fill_rgb[1]), (int)(fill_rgb[2]));
	}
	for (int u = 1; u <= isocurveres; u++) {
	    ON_3dPoint p = surf.PointAt(udom.ParameterAt((double)(u-1)/(double)isocurveres), vdom.ParameterAt((double)v/(double)gridres));
	    VMOVE(pt1, p);
	    p = surf.PointAt(udom.ParameterAt((double)u/(double)isocurveres), vdom.ParameterAt((double)v/(double)gridres));
	    VMOVE(pt2, p);
	    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}
    }
    return;
}


void
plotsurfacenormals(ON_Surface &surf, struct bn_vlblock *vbp, int gridres)
{
    register struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    vhead = rt_vlblock_find(vbp, GREEN);

    ON_Interval udom = surf.Domain(0);
    ON_Interval vdom = surf.Domain(1);

    for (int u = 0; u <= gridres; u++) {
	for (int v = 1; v <= gridres; v++) {
	    ON_3dPoint p = surf.PointAt(udom.ParameterAt((double)u/(double)gridres), vdom.ParameterAt((double)(v-1)/(double)gridres));
	    ON_3dVector n = surf.NormalAt(udom.ParameterAt((double)u/(double)gridres), vdom.ParameterAt((double)(v-1)/(double)gridres));
	    n.Unitize();
	    VMOVE(pt1, p);
	    VSCALE(pt2, n, surf.BoundingBox().Diagonal().Length()*0.1);
	    VADD2(pt2, pt1, pt2);
	    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}
    }
    return;
}


void
plotsurfaceknots(ON_Surface &surf, struct bn_vlblock *vbp, bool dim3d)
{
    register struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;
    int spanu_cnt = surf.SpanCount(0);
    int spanv_cnt = surf.SpanCount(1);
    fastf_t *spanu = NULL;
    fastf_t *spanv = NULL;
    spanu = new fastf_t[spanu_cnt+1];
    spanv = new fastf_t[spanv_cnt+1];

#ifndef RESETDOMAIN
    double width, height;
    if (surf.GetSurfaceSize(&width, &height)) {
	surf.SetDomain(0, 0.0, width);
	surf.SetDomain(1, 0.0, height);

    }
#endif

    surf.GetSpanVector(0, spanu);
    surf.GetSpanVector(1, spanv);

    vhead = rt_vlblock_find(vbp, GREEN);

    ON_Interval udom = surf.Domain(0);
    ON_Interval vdom = surf.Domain(1);

    if (dim3d) {
	for (int u = 0; u <= spanu_cnt; u++) {
	    for (int v = 0; v <= spanv_cnt; v++) {
		ON_3dPoint p = surf.PointAt(spanu[u], spanv[v]);
		ON_3dVector n = surf.NormalAt(spanu[u], spanv[v]);
		n.Unitize();
		VMOVE(pt1, p);
		VSCALE(pt2, n, 3.0);
		VADD2(pt2, pt1, pt2);
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	    }
	}
    } else {
	for (int u = 0; u <= spanu_cnt; u++) {
	    for (int v = 0; v <= spanv_cnt; v++) {
		VSET(pt1, spanu[u], spanv[v], 0.0);
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_POINT_DRAW);
	    }
	}
    }
    return;
}


void
plotcurve(ON_Curve &curve, struct bn_vlblock *vbp, int plotres, const int red = 255, const int green = 255, const int blue = 0)
{
    register struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;

    vhead = rt_vlblock_find(vbp, red, green, blue);

    if (curve.IsLinear()) {
	/*
	  ON_BrepVertex& v1 = face.Brep()->m_V[trim.m_vi[0]];
	  ON_BrepVertex& v2 = face.Brep()->m_V[trim.m_vi[1]];
	  VMOVE(pt1, v1.Point());
	  VMOVE(pt2, v2.Point());
	  LINE_PLOT(pt1, pt2);
	*/

	int knotcnt = curve.SpanCount();
	fastf_t *knots = new fastf_t[knotcnt + 1];

	curve.GetSpanVector(knots);
	for (int i = 1; i <= knotcnt; i++) {
	    ON_3dPoint p = curve.PointAt(knots[i - 1]);
	    VMOVE(pt1, p);
	    p = curve.PointAt(knots[i]);
	    VMOVE(pt2, p);
	    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}

    } else {
	ON_Interval dom = curve.Domain();
	// XXX todo: dynamically sample the curve
	for (int i = 1; i <= plotres; i++) {
	    ON_3dPoint p = curve.PointAt(dom.ParameterAt((double) (i - 1)
							 / (double)plotres));
	    VMOVE(pt1, p);
	    p = curve.PointAt(dom.ParameterAt((double) i / (double)plotres));
	    VMOVE(pt2, p);
	    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}
    }
    return;
}


void
plotpoint(const ON_3dPoint &point, struct bn_vlblock *vbp, const int red = 255, const int green = 255, const int blue = 0)
{
    register struct bu_list *vhead;
    ON_3dPoint pointsize(4.0,0,0);
    vhead = rt_vlblock_find(vbp, red, green, blue);
    RT_ADD_VLIST(vhead, pointsize, BN_VLIST_POINT_SIZE);
    RT_ADD_VLIST(vhead, point, BN_VLIST_POINT_DRAW);
    return;
}


void plotcurveonsurface(ON_Curve *curve,
			ON_Surface *surface,
			struct bn_vlblock *vbp,
			int plotres,
			const int red = 255,
			const int green = 255,
			const int blue = 0)
{
    if (curve->Dimension() != 2)
	return;
    register struct bu_list *vhead;
    vhead = rt_vlblock_find(vbp, red, green, blue);

    for (int i = 0; i <= plotres; i++) {
	ON_2dPoint pt2d;
	ON_3dPoint pt3d;
	ON_3dPoint pt1, pt2;
	pt2d = curve->PointAt(curve->Domain().ParameterAt((double)i/plotres));
	pt3d = surface->PointAt(pt2d.x, pt2d.y);
	pt1 = pt2;
	pt2 = pt3d;
	if (i != 0) {
	    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}
    }
    return;
}


void
plottrim(const ON_Curve &curve, double from, double to)
{
    point_t pt1, pt2;
    // XXX todo: dynamically sample the curve
    for (int i = 0; i <= 10000; i++) {
	ON_3dPoint p = curve.PointAt(from + (to-from)*(double)i/10000.0);//dom.ParameterAt((double)i/10000.0));
	VMOVE(pt2, p);
	if (i != 0) {
	    LINE_PLOT(pt1, pt2);
	}
	VMOVE(pt1, p);
    }
}


void
plottrim(ON_Curve &curve)
{
    point_t pt1, pt2;
    // XXX todo: dynamically sample the curve
    ON_Interval dom = curve.Domain();
    for (int i = 0; i <= 10000; i++) {
	ON_3dPoint p = curve.PointAt(dom.ParameterAt((double)i/10000.0));
	VMOVE(pt2, p);
	if (i != 0) {
	    LINE_PLOT(pt1, pt2);
	}
	VMOVE(pt1, p);
    }
}


int
brep_info(struct brep_specific* bs, struct bu_vls *vls)
{
    ON_Brep *brep = bs->brep;
    bu_vls_printf(vls, "surfaces:  %d\n", brep->m_S.Count());
    bu_vls_printf(vls, "3d curve:  %d\n", brep->m_C3.Count());
    bu_vls_printf(vls, "2d curves: %d\n", brep->m_C2.Count());
    bu_vls_printf(vls, "vertices:  %d\n", brep->m_V.Count());
    bu_vls_printf(vls, "edges:     %d\n", brep->m_E.Count());
    bu_vls_printf(vls, "trims:     %d\n", brep->m_T.Count());
    bu_vls_printf(vls, "loops:     %d\n", brep->m_L.Count());
    bu_vls_printf(vls, "faces:     %d\n", brep->m_F.Count());

    return 0;
}


int
brep_surface_info(struct brep_specific* bs, struct bu_vls *vls, int si)
{
    ON_wString wonstr;
    ON_TextLog info_output(wonstr);
    ON_Brep *brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&info_output)) {
	bu_log("brep is NOT valid");
    }
    if ((si >= 0) && (si < brep->m_S.Count())) {
	const ON_Surface* srf = brep->m_S[si];
	if (srf) {
	    //ON_wString wonstr;
	    //ON_TextLog info_output(wonstr);
	    ON_Interval udom = srf->Domain(0);
	    ON_Interval vdom = srf->Domain(1);
	    const char* s = srf->ClassId()->ClassName();
	    if (!s)
		s = "";
	    bu_vls_printf(vls, "surface[%2d]: %s u(%g, %g) v(%g, %g)\n",
			  si, s,
			  udom[0], udom[1],
			  vdom[0], vdom[1]
		);
	    bu_vls_printf(vls, "NURBS form of Surface:\n");
	    ON_NurbsSurface *nsrf = ON_NurbsSurface::New();
	    srf->GetNurbForm(*nsrf, 0.0);
	    nsrf->Dump(info_output);
	    ON_String onstr = ON_String(wonstr);
	    const char *infodesc = onstr.Array();
	    bu_vls_strcat(vls, infodesc);
	    delete nsrf;
	} else {
	    bu_vls_printf(vls, "surface[%2d]: NULL\n", si);
	}
    }

    return 0;
}


int
brep_surface_bezier_info(struct brep_specific* bs, struct bu_vls *vls, int si)
{
    ON_wString wonstr;
    ON_TextLog info_output(wonstr);
    ON_Brep *brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&info_output)) {
	bu_log("brep is NOT valid");
    }

    if ((si >= 0) && (si < brep->m_S.Count())) {
	const ON_Surface* srf = brep->m_S[si];
	if (srf) {
	    //ON_wString wonstr;
	    //ON_TextLog info_output(wonstr);
	    ON_Interval udom = srf->Domain(0);
	    ON_Interval vdom = srf->Domain(1);
	    const char* s = srf->ClassId()->ClassName();
	    if (!s)
		s = "";
	    bu_vls_printf(vls, "surface[%2d]: %s u(%g, %g) v(%g, %g)\n",
			  si, s,
			  udom[0], udom[1],
			  vdom[0], vdom[1]
		);
	    ON_NurbsSurface *nsrf = ON_NurbsSurface::New();
	    srf->GetNurbForm(*nsrf, 0.0);
	    int knotlength0 = nsrf->m_order[0] + nsrf->m_cv_count[0] - 2;
	    int knotlength1 = nsrf->m_order[1] + nsrf->m_cv_count[1] - 2;
	    int order0 = nsrf->m_order[0];
	    int order1 = nsrf->m_order[1];
	    fastf_t *knot0 = nsrf->m_knot[0];
	    fastf_t *knot1 = nsrf->m_knot[1];
	    int cnt = 0;
	    bu_vls_printf(vls, "bezier patches:\n");
	    for (int i = 0; i < knotlength0; ++i) {
		for (int j = 0; j < knotlength1; ++j) {
		    ON_BezierSurface *bezier = new ON_BezierSurface;
		    if (nsrf->ConvertSpanToBezier(i, j, *bezier)) {
			info_output.Print("NO.%d segment\n", ++cnt);
			info_output.Print("spanindex u from %d to %d\n", i + order0 - 2, i + order0 - 1);
			info_output.Print("spanindex v from %d to %d\n", j + order1 - 2, j + order1 - 1);
			info_output.Print("knot u from %.2f to %.2f\n ", knot0[i + order0 - 2], knot0[i + order0 - 1]);
			info_output.Print("knot v from %.2f to %.2f\n ", knot1[j + order1 - 2], knot1[j + order1 - 1]);
			info_output.Print("domain u(%g, %g)\n", bezier->Domain(0)[0], bezier->Domain(0)[1]);
			info_output.Print("domain v(%g, %g)\n", bezier->Domain(1)[0], bezier->Domain(1)[1]);
			bezier->Dump(info_output);
			info_output.Print("\n");
		    }
		    delete bezier;
		}
	    }
	    ON_String onstr = ON_String(wonstr);
	    const char *infodesc = onstr.Array();
	    bu_vls_strcat(vls, infodesc);
	    delete nsrf;
	} else {
	    bu_vls_printf(vls, "surface[%2d]: NULL\n", si);
	}
    }
    return 0;
}


int
brep_face_info(struct brep_specific* bs, struct bu_vls *vls, int fi)
{
    ON_wString s;
    ON_TextLog dump(s);
    ON_Brep *brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&dump)) {
	bu_log("brep is NOT valid");
    }

    if (!((fi >= 0) && (fi < brep->m_F.Count())))
	return 0;
    //ON_wString s;
    //ON_TextLog dump(s);
    const ON_BrepFace& face = brep->m_F[fi];
    const ON_Surface* face_srf = face.SurfaceOf();
    dump.Print("face[%2d]: surface(%d) reverse(%d) loops(", fi, face.m_si, face.m_bRev);
    int fli;
    for (fli = 0; fli < face.m_li.Count(); fli++) {
	dump.Print((fli) ? ", %d" : "%d", face.m_li[fli]);
    }
    dump.Print(")\n");
    dump.PushIndent();

    for (fli = 0; fli < face.m_li.Count(); fli++) {
	const int li = face.m_li[fli];
	const ON_BrepLoop& loop = brep->m_L[li];
	const char* sLoopType = 0;
	switch (loop.m_type) {
	    case ON_BrepLoop::unknown:
		sLoopType = "unknown";
		break;
	    case ON_BrepLoop::outer:
		sLoopType = "outer";
		break;
	    case ON_BrepLoop::inner:
		sLoopType = "inner";
		break;
	    case ON_BrepLoop::slit:
		sLoopType = "slit";
		break;
	    case ON_BrepLoop::crvonsrf:
		sLoopType = "crvonsrf";
		break;
	    default:
		sLoopType = "unknown";
		break;
	}
	dump.Print("loop[%2d]: type(%s) %d trims(", li, sLoopType, loop.m_ti.Count());
	int lti;
	for (lti = 0; lti < loop.m_ti.Count(); lti++) {
	    dump.Print((lti) ? ", %d" : "%d", loop.m_ti[lti]);
	}
	dump.Print(")\n");
	dump.PushIndent();
	for (lti = 0; lti < loop.m_ti.Count(); lti++) {
	    const int ti = loop.m_ti[lti];
	    const ON_BrepTrim& trim = brep->m_T[ti];
	    const char* sTrimType = "?";
	    const char* sTrimIso = "-?";
	    const ON_Curve* c2 = trim.TrimCurveOf();
	    ON_NurbsCurve* nc2 = ON_NurbsCurve::New();
	    c2->GetNurbForm(*nc2, 0.0);
	    ON_3dPoint trim_start, trim_end;
	    switch (trim.m_type) {
		case ON_BrepTrim::unknown:
		    sTrimType = "unknown ";
		    break;
		case ON_BrepTrim::boundary:
		    sTrimType = "boundary";
		    break;
		case ON_BrepTrim::mated:
		    sTrimType = "mated   ";
		    break;
		case ON_BrepTrim::seam:
		    sTrimType = "seam    ";
		    break;
		case ON_BrepTrim::singular:
		    sTrimType = "singular";
		    break;
		case ON_BrepTrim::crvonsrf:
		    sTrimType = "crvonsrf";
		    break;
		default:
		    sTrimType = "unknown";
		    break;
	    }
	    switch (trim.m_iso) {
		case ON_Surface::not_iso:
		    sTrimIso = "";
		    break;
		case ON_Surface::x_iso:
		    sTrimIso = "-u iso";
		    break;
		case ON_Surface::W_iso:
		    sTrimIso = "-west side iso";
		    break;
		case ON_Surface::E_iso:
		    sTrimIso = "-east side iso";
		    break;
		case ON_Surface::y_iso:
		    sTrimIso = "-v iso";
		    break;
		case ON_Surface::S_iso:
		    sTrimIso = "-south side iso";
		    break;
		case ON_Surface::N_iso:
		    sTrimIso = "-north side iso";
		    break;
		default:
		    sTrimIso = "-unknown_iso_flag";
		    break;
	    }
	    dump.Print("trim[%2d]: edge(%2d) v0(%2d) v1(%2d) tolerance(%g, %g)\n", ti, trim.m_ei, trim.m_vi[0], trim.m_vi[1], trim.m_tolerance[0], trim.m_tolerance[1]);
	    dump.PushIndent();
	    dump.Print("type(%s%s) rev3d(%d) 2d_curve(%d)\n", sTrimType, sTrimIso, trim.m_bRev3d, trim.m_c2i);
	    if (c2) {
		trim_start = trim.PointAtStart();
		trim_end = trim.PointAtEnd();
		dump.Print("domain(%g, %g) start(%g, %g) end(%g, %g)\n", trim.Domain()[0], trim.Domain()[1], trim_start.x, trim_start.y, trim_end.x, trim_end.y);
		if (0 != face_srf) {
		    ON_3dPoint trim_srfstart = face_srf->PointAt(trim_start.x, trim_start.y);
		    ON_3dPoint trim_srfend = face_srf->PointAt(trim_end.x, trim_end.y);
		    dump.Print("surface points start(%g, %g, %g) end(%g, %g, %g)\n", trim_srfstart.x, trim_srfstart.y, trim_srfstart.z, trim_srfend.x, trim_srfend.y, trim_srfend.z);
		}
	    } else {
		dump.Print("domain(%g, %g) start(?, ?) end(?, ?)\n", trim.Domain()[0], trim.Domain()[1]);
	    }
	    dump.PopIndent();
	}
	dump.PopIndent();
    }
    dump.PopIndent();

    ON_String ss = s;
    bu_vls_printf(vls, "%s\n", ss.Array());

    return 0;
}


int
brep_trim_info(struct brep_specific* bs, struct bu_vls *vls, int ti)
{
    ON_Brep* brep = bs->brep;
    ON_wString wstr;
    ON_TextLog dump(wstr);
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&dump)) {
	bu_log("brep is NOT valid");
    }

    if (!((ti >= 0) && (ti < brep->m_T.Count())))
	return 0;
    const ON_BrepTrim &trim = brep->m_T[ti];
    const ON_Surface* trim_srf = trim.SurfaceOf();
    const ON_BrepLoop &loop = brep->m_L[trim.m_li];
    const ON_BrepFace &face = brep->m_F[loop.m_fi];
    const char* sTrimType = "?";
    const char* sTrimIso = "-?";
    const ON_Curve* c2 = trim.TrimCurveOf();
    ON_NurbsCurve* nc2 = ON_NurbsCurve::New();
    c2->GetNurbForm(*nc2, 0.0);
    ON_3dPoint trim_start, trim_end;
    dump.Print("trim[%2d]: surface(%2d) faces(%2d) loops(%2d)\n", ti, face.m_si, face.m_face_index, loop.m_loop_index);
    switch (trim.m_type) {
	case ON_BrepTrim::unknown:
	    sTrimType = "unknown ";
	    break;
	case ON_BrepTrim::boundary:
	    sTrimType = "boundary";
	    break;
	case ON_BrepTrim::mated:
	    sTrimType = "mated   ";
	    break;
	case ON_BrepTrim::seam:
	    sTrimType = "seam    ";
	    break;
	case ON_BrepTrim::singular:
	    sTrimType = "singular";
	    break;
	case ON_BrepTrim::crvonsrf:
	    sTrimType = "crvonsrf";
	    break;
	default:
	    sTrimType = "unknown";
	    break;
    }
    switch (trim.m_iso) {
	case ON_Surface::not_iso:
	    sTrimIso = "";
	    break;
	case ON_Surface::x_iso:
	    sTrimIso = "-u iso";
	    break;
	case ON_Surface::W_iso:
	    sTrimIso = "-west side iso";
	    break;
	case ON_Surface::E_iso:
	    sTrimIso = "-east side iso";
	    break;
	case ON_Surface::y_iso:
	    sTrimIso = "-v iso";
	    break;
	case ON_Surface::S_iso:
	    sTrimIso = "-south side iso";
	    break;
	case ON_Surface::N_iso:
	    sTrimIso = "-north side iso";
	    break;
	default:
	    sTrimIso = "-unknown_iso_flag";
	    break;
    }
    dump.Print("\tedge(%2d) v0(%2d) v1(%2d) tolerance(%g, %g)\n", trim.m_ei, trim.m_vi[0], trim.m_vi[1], trim.m_tolerance[0], trim.m_tolerance[1]);
    dump.PushIndent();
    dump.Print("\ttype(%s%s) rev3d(%d) 2d_curve(%d)\n", sTrimType, sTrimIso, trim.m_bRev3d, trim.m_c2i);
    if (c2) {
	trim_start = trim.PointAtStart();
	trim_end = trim.PointAtEnd();
	dump.Print("\tdomain(%g, %g) start(%g, %g) end(%g, %g)\n", trim.Domain()[0], trim.Domain()[1], trim_start.x, trim_start.y, trim_end.x, trim_end.y);
	if (0 != trim_srf) {
	    ON_3dPoint trim_srfstart = trim_srf->PointAt(trim_start.x, trim_start.y);
	    ON_3dPoint trim_srfend = trim_srf->PointAt(trim_end.x, trim_end.y);
	    dump.Print("\tsurface points start(%g, %g, %g) end(%g, %g, %g)\n", trim_srfstart.x, trim_srfstart.y, trim_srfstart.z, trim_srfend.x, trim_srfend.y, trim_srfend.z);
	}
    } else {
	dump.Print("\tdomain(%g, %g) start(?, ?) end(?, ?)\n", trim.Domain()[0], trim.Domain()[1]);
    }
    dump.PopIndent();
    dump.Print("NURBS form of 2d_curve(trim)\n");
    nc2->Dump(dump);
    delete nc2;

    ON_String ss = wstr;
    bu_vls_printf(vls, "%s\n", ss.Array());
    return 0;
}


int
brep_trim_bezier_info(struct brep_specific* bs, struct bu_vls *vls, int ti)
{
    ON_Brep* brep = bs->brep;
    ON_wString wstr;
    ON_TextLog dump(wstr);
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&dump)) {
	bu_log("brep is NOT valid");
    }

    if (!((ti >= 0) && (ti < brep->m_T.Count())))
	return 0;
    const ON_BrepTrim &trim = brep->m_T[ti];
    const ON_Curve* c2 = trim.TrimCurveOf();
    ON_NurbsCurve* nc2 = ON_NurbsCurve::New();
    c2->GetNurbForm(*nc2, 0.0);
    int knotlength = nc2->m_order + nc2->m_cv_count - 2;
    int order = nc2->m_order;
    fastf_t *knot = nc2->m_knot;
    dump.Print("trim[%2d]: domain(%g, %g)\n", ti, nc2->Domain()[0], nc2->Domain()[1]);
    int cnt = 0;
    dump.Print("NURBS converts to Bezier\n");
    for (int i = 0; i < knotlength - 1; ++i) {
	ON_BezierCurve* bezier = new ON_BezierCurve;
	if (nc2->ConvertSpanToBezier(i, *bezier)) {
	    dump.Print("NO.%d segment\n", ++cnt);
	    dump.Print("spanindex from %d to %d\n", i + order - 2, i + order - 1);
	    dump.Print("knot from %.2f to %.2f\n ", knot[i + order - 2], knot[i + order - 1]);
	    dump.Print("domain(%g, %g)\n", bezier->Domain()[0], bezier->Domain()[1]);
	    bezier->Dump(dump);
	    dump.Print("\n");
	}
	delete bezier;
    }
    delete nc2;
    ON_String ss = wstr;
    bu_vls_printf(vls, "%s\n", ss.Array());
    return 0;
}


int
brep_curve_info(struct brep_specific* bs, struct bu_vls *vls, int ci)
{
    ON_Brep* brep = bs->brep;
    ON_wString wstr;
    ON_TextLog dump(wstr);
    if (brep == NULL) {
	return -1;
    }
    if (!((ci >= 0) && (ci < brep->m_C3.Count())))
	return 0;
    const ON_Curve *curve = brep->m_C3[ci];
    ON_NurbsCurve* nc3 = ON_NurbsCurve::New();
    curve->GetNurbForm(*nc3, 0.0);
    dump.Print("NURBS form of 3d_curve(edge) \n");
    nc3->Dump(dump);
    delete nc3;

    ON_String ss = wstr;
    bu_vls_printf(vls, "%s\n", ss.Array());
    return 0;
}



int
brep_edge_info(struct brep_specific* bs, struct bu_vls *vls, int ei)
{
    ON_Brep* brep = bs->brep;
    ON_wString wstr;
    ON_TextLog dump(wstr);
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&dump)) {
	bu_log("brep is NOT valid");
    }

    if (!((ei >= 0) && (ei < brep->m_E.Count())))
	return 0;
    const ON_BrepEdge &edge = brep->m_E[ei];
    int trim_cnt = edge.m_ti.Count();
    const ON_Curve* c3 = edge.EdgeCurveOf();
    ON_NurbsCurve* nc3 = ON_NurbsCurve::New();
    c3->GetNurbForm(*nc3, 0.0);
    ON_3dPoint edge_start, edge_end;
    dump.Print("edge[%2d]: for ", ei);
    for (int i = 0; i < trim_cnt; ++i) {
	dump.Print("trim[%2d] ", edge.m_ti[i]);
    }
    dump.Print("\n");
    dump.Print("v0(%2d) v1(%2d) 3d_curve(%2d) tolerance(%g, %g)\n", ei, edge.m_vi[0], edge.m_vi[1], edge.m_c3i, edge.m_tolerance);
    dump.PushIndent();
    if (c3) {
	edge_start = edge.PointAtStart();
	edge_end = edge.PointAtEnd();
	dump.Print("\tdomain(%g, %g) surface points start(%g, %g, %g) end(%g, %g, %g)\n", edge.Domain()[0], edge.Domain()[1], edge_start.x, edge_start.y, edge_start.z, edge_end.x, edge_end.y, edge_end.z);
    } else {
	dump.Print("\tdomain(%g, %g) start(?, ?) end(?, ?)\n", edge.Domain()[0], edge.Domain()[1]);
    }
    dump.PopIndent();
    dump.Print("NURBS form of 3d_curve(edge) \n");
    nc3->Dump(dump);
    delete nc3;

    ON_String ss = wstr;
    bu_vls_printf(vls, "%s\n", ss.Array());
    return 0;
}


int brep_facecdt_plot(struct bu_vls *vls, const char *solid_name,
		      const struct rt_tess_tol *ttol, const struct bn_tol *tol,
		      struct brep_specific* bs, struct rt_brep_internal*UNUSED(bi),
		      struct bn_vlblock *vbp, int index, int plottype, int num_points = -1)
{
    register struct bu_list *vhead = rt_vlblock_find(vbp, YELLOW);
    bool watertight = true;
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
	if (wstr.Length() > 0) {
	    ON_String onstr = ON_String(wstr);
	    const char *isvalidinfo = onstr.Array();
	    bu_vls_strcat(vls, "brep (");
	    bu_vls_strcat(vls, solid_name);
	    bu_vls_strcat(vls, ") is NOT valid:");
	    bu_vls_strcat(vls, isvalidinfo);
	} else {
	    bu_vls_strcat(vls, "brep (");
	    bu_vls_strcat(vls, solid_name);
	    bu_vls_strcat(vls, ") is NOT valid.");
	}
	//for now try to draw - return -1;
    }

    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace *face = brep->Face(face_index);
	const ON_Surface *s = face->SurfaceOf();
	double surface_width, surface_height;
	if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	    // reparameterization of the face's surface and transforms the "u"
	    // and "v" coordinates of all the face's parameter space trimming
	    // curves to minimize distortion in the map from parameter space to 3d..
	    face->SetDomain(0, 0.0, surface_width);
	    face->SetDomain(1, 0.0, surface_height);
	}
    }

    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    poly2tri_CDT(vhead, face, ttol, tol, NULL, watertight, plottype, num_points);
	}
    } else if (index < brep->m_F.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    face.Dump(tl);
	    poly2tri_CDT(vhead, face, ttol, tol, NULL, watertight, plottype, num_points);
	}
    }

    bu_vls_printf(vls, ON_String(wstr).Array());

    return 0;
}


int
brep_facetrim_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres, bool dim3d)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    if (!dim3d)
		plotUVDomain2d(face, vbp);
	    plottrim(face, vbp, plotres, dim3d);
	}
    } else if (index < brep->m_F.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    face.Dump(tl);
	    if (!dim3d)
		plotUVDomain2d(face, vbp);
	    plottrim(face, vbp, plotres, dim3d);
	}
    }

    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;
}


int
brep_trim_direction_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    plottrimdirection(face, vbp, plotres);
	}
    } else if (index < brep->m_F.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    face.Dump(tl);
	    plottrimdirection(face, vbp, plotres);
	}
    }

    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;
}


int
brep_surface_uv_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, double u, double v)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	for (index = 0; index < brep->m_S.Count(); index++) {
	    ON_Surface *surf = brep->m_S[index];
	    plotpoint(surf->PointAt(u, v), vbp, GREEN);
	}
    } else if (index < brep->m_S.Count()) {
	ON_Surface *surf = brep->m_S[index];
	surf->Dump(tl);
	plotpoint(surf->PointAt(u, v), vbp, GREEN);
    }

    bu_vls_printf(vls, ON_String(wstr).Array());

    return 0;
}


int
brep_surface_uv_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, ON_Interval &U, ON_Interval &V)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if ((index >= 0)&&(index < brep->m_S.Count())) {
	ON_Surface *surf = brep->m_S[index];
	register struct bu_list *vhead;
	fastf_t pt1[3], pt2[3];
	fastf_t delta = U.Length()/1000.0;

	vhead = rt_vlblock_find(vbp, YELLOW);
	for (int i = 0; i < 2; i++) {
	    fastf_t v = V.m_t[i];
	    for (fastf_t u = U.m_t[0]; u < U.m_t[1]; u = u + delta) {
		ON_3dPoint p = surf->PointAt(u, v);
		//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
		VMOVE(pt1, p);
		if (u + delta > U.m_t[1]) {
		    p = surf->PointAt(U.m_t[1], v);
		} else {
		    p = surf->PointAt(u + delta, v);
		}
		//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
		VMOVE(pt2, p);
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	    }
	}
	delta = V.Length()/1000.0;
	for (int i = 0; i < 2; i++) {
	    fastf_t u = U.m_t[i];
	    for (fastf_t v = V.m_t[0]; v < V.m_t[1]; v = v + delta) {
		ON_3dPoint p = surf->PointAt(u, v);
		//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
		VMOVE(pt1, p);
		if (v + delta > V.m_t[1]) {
		    p = surf->PointAt(u,V.m_t[1]);
		} else {
		    p = surf->PointAt(u, v + delta);
		}
		//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
		VMOVE(pt2, p);
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	    }
	}
    }

    bu_vls_printf(vls, ON_String(wstr).Array());

    return 0;
}


int
brep_surface_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	for (index = 0; index < brep->m_S.Count(); index++) {
	    ON_Surface *surf = brep->m_S[index];
	    plotsurface(*surf, vbp, plotres, 10);
	}
    } else if (index < brep->m_S.Count()) {
	ON_Surface *surf = brep->m_S[index];
	surf->Dump(tl);
	plotsurface(*surf, vbp, plotres, 10);
    }

    bu_vls_printf(vls, ON_String(wstr).Array());

    return 0;
}


int
brep_surface_normal_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	for (index = 0; index < brep->m_S.Count(); index++) {
	    ON_Surface *surf = brep->m_S[index];
	    plotsurfaceknots(*surf, vbp, true);
	    plotsurfacenormals(*surf, vbp, plotres);
	}
    } else if (index < brep->m_S.Count()) {
	ON_Surface *surf = brep->m_S[index];
	surf->Dump(tl);
	plotsurfaceknots(*surf, vbp, true);
	plotsurfacenormals(*surf, vbp, plotres);
    }

    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;
}


int
brep_surface_knot_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, bool dim3d)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	for (index = 0; index < brep->m_S.Count(); index++) {
	    ON_Surface *surf = brep->m_S[index];
	    plotsurfaceknots(*surf, vbp, dim3d);
	}
    } else if (index < brep->m_S.Count()) {
	ON_Surface *surf = brep->m_S[index];
	surf->Dump(tl);
	plotsurfaceknots(*surf, vbp, dim3d);
    }

    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;
}


int
brep_edge3d_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	int num_curves = brep->m_C3.Count();
	for (index = 0; index < num_curves; index++) {
	    ON_Curve *curve = brep->m_C3[index];
	    plotcurve(*curve, vbp, plotres);
	}
    } else if (index < brep->m_C3.Count()) {
	ON_Curve *curve = brep->m_C3[index];
	curve->Dump(tl);
	plotcurve(*curve, vbp, plotres);
    }

    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;
}


static void
plot_nurbs_cv(struct bn_vlblock *vbp, int ucount, int vcount, ON_NurbsSurface *ns)
{
    register struct bu_list *vhead;
    vhead = rt_vlblock_find(vbp, PEACH);
    ON_3dPoint cp;
    fastf_t pt1[3], pt2[3];
    int i, j, k, temp;
    for (k = 0; k < 2; k++) {
	/*< two times i loop */

	for (i = 0; i < ucount; ++i) {
	    if (k == 1)
		ns->GetCV(0, i, cp);	   /*< i < ucount */
	    else
		ns->GetCV(i, 0, cp);       /*< i < vcount */

	    VMOVE(pt1, cp);
	    for (j = 0; j < vcount; ++j) {
		if (k == 1)
		    ns->GetCV(j, i, cp);
		else
		    ns->GetCV(i, j, cp);

		VMOVE(pt2, cp);
		RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		VMOVE(pt1, cp);
		RT_ADD_VLIST(vhead, cp, BN_VLIST_POINT_DRAW);
	    }
	}
	temp  = ucount;
	ucount = vcount;
	vcount = temp;
    }
}


int
brep_trim_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres, bool dim3d)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	int num_trims = brep->m_T.Count();
	for (index = 0; index < num_trims; index++) {
	    ON_BrepTrim &trim = brep->m_T[index];
	    plottrim(trim, vbp, plotres, dim3d);
	}
    } else if (index < brep->m_T.Count()) {
	ON_BrepTrim &trim = brep->m_T[index];
	plottrim(trim, vbp, plotres, dim3d);
    }

    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;
}


int
brep_surface_cv_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	for (index = 0; index < brep->m_S.Count(); index++) {
	    ON_Surface *surf = brep->m_S[index];
	    ON_NurbsSurface *ns = ON_NurbsSurface::New();
	    surf->GetNurbForm(*ns, 0.0);
	    int ucount, vcount;
	    ucount = ns->m_cv_count[0];
	    vcount = ns->m_cv_count[1];
	    surf->Dump(tl);
	    plot_nurbs_cv(vbp, ucount, vcount, ns);
	}
    } else if (index < brep->m_S.Count()) {
	ON_Surface *surf = brep->m_S[index];
	ON_NurbsSurface *ns = ON_NurbsSurface::New();
	surf->GetNurbForm(*ns, 0.0);
	int ucount, vcount;
	ucount = ns->m_cv_count[0];
	vcount = ns->m_cv_count[1];
	plot_nurbs_cv(vbp, ucount, vcount, ns);
    }
    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;

}


/* a binary predicate for std:list implemented as a function */
extern bool near_equal (double first, double second);


void
plotFace(SurfaceTree* st, struct bn_vlblock *vbp, int UNUSED(isocurveres), int gridres)
{
    register struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    ON_2dPoint from, to;
    std::list<BRNode*> m_trims_above_or_right;
    std::list<fastf_t> trim_hits;

    vhead = rt_vlblock_find(vbp, PEACH);

    const ON_Surface *surf = st->getSurface();
    CurveTree *ctree = st->ctree;
    ON_Interval udom = surf->Domain(0);
    ON_Interval vdom = surf->Domain(1);
    //bu_log("udom  %f, %f vdom %f, %f\n", udom.m_t[0], udom.m_t[1], vdom.m_t[0], vdom.m_t[1]);

    m_trims_above_or_right.clear();

    ON_2dPoint pt;
    for (int gd = 1; gd < gridres; gd++) {

	pt.x = udom.ParameterAt((double)gd /(double)gridres);
	pt.y = vdom.ParameterAt(0.0);

	if (ctree != NULL) {
	    m_trims_above_or_right.clear();
	    ctree->getLeavesAbove(m_trims_above_or_right, pt, 0.0000001);
	}
	int cnt=1;
	//bu_log("U - %f\n", pt.x);
	trim_hits.clear();
	for (std::list<BRNode*>::iterator i = m_trims_above_or_right.begin(); i != m_trims_above_or_right.end(); i++, cnt++) {
	    BRNode* br = dynamic_cast<BRNode*>(*i);

	    point_t bmin, bmax;
	    br->GetBBox(bmin, bmax);
	    if ((bmin[X] <= pt[X]) && (pt[X] <= bmax[X])) { //if check trim and in BBox
		fastf_t v = br->getCurveEstimateOfV(pt[X], 0.0000001);
		trim_hits.push_back(v);
		//bu_log("%d V %d - %f pt %f, %f bmin %f, %f  bmax %f, %f\n", br->m_face->m_face_index, cnt, v, pt.x, pt.y, bmin[X], bmin[Y], bmax[X], bmax[Y]);
	    }
	}
	trim_hits.sort();
	trim_hits.unique(near_equal);

	size_t hit_cnt = trim_hits.size();

	if ((hit_cnt > 1) && ((hit_cnt % 2) == 0)) {
	    //bu_log("U - %f\n", pt.x);
	    while (!trim_hits.empty()) {
		fastf_t tfrom = trim_hits.front();
		trim_hits.pop_front();
		fastf_t tto = trim_hits.front();
		trim_hits.pop_front();
		//bu_log("\tfrom - %f, to - %f\n", tfrom, tto);
		fastf_t deltay = (tto-tfrom)/50.0;
		for (fastf_t y = tfrom; y < tto - deltay; y = y + deltay) {
		    ON_3dPoint p = surf->PointAt(pt.x, y);
		    //bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
		    VMOVE(pt1, p);
		    p = surf->PointAt(pt.x, y+deltay);
		    if (y+deltay > tto) {
			p = surf->PointAt(pt.x, tto);
		    } else {
			p = surf->PointAt(pt.x, y+deltay);
		    }
		    //bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
		    VMOVE(pt2, p);
		    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}

	    }
	}
	// v direction
	pt.x = udom.ParameterAt(0.0);
	pt.y = vdom.ParameterAt((double)gd /(double)gridres);

	if (ctree != NULL) {
	    m_trims_above_or_right.clear();
	    ctree->getLeavesRight(m_trims_above_or_right, pt, 0.0000001);
	}

	cnt=1;
	//bu_log("V - %f\n", pt.x);
	trim_hits.clear();
	for (std::list<BRNode*>::iterator i = m_trims_above_or_right.begin(); i != m_trims_above_or_right.end(); i++, cnt++) {
	    BRNode* br = dynamic_cast<BRNode*>(*i);

	    point_t bmin, bmax;
	    br->GetBBox(bmin, bmax);
	    if ((bmin[Y] <= pt[Y]) && (pt[Y] <= bmax[Y])) { //if check trim and in BBox
		fastf_t u = br->getCurveEstimateOfU(pt[Y], 0.0000001);
		trim_hits.push_back(u);
		//bu_log("%d U %d - %f pt %f, %f bmin %f, %f  bmax %f, %f\n", br->m_face->m_face_index, cnt, u, pt.x, pt.y, bmin[X], bmin[Y], bmax[X], bmax[Y]);
	    }
	}
	trim_hits.sort();
	trim_hits.unique(near_equal);

	hit_cnt = trim_hits.size();

	if ((hit_cnt > 1) && ((hit_cnt % 2) == 0)) {
	    //bu_log("V - %f\n", pt.y);
	    while (!trim_hits.empty()) {
		fastf_t tfrom = trim_hits.front();
		trim_hits.pop_front();
		fastf_t tto = trim_hits.front();
		trim_hits.pop_front();
		//bu_log("\tfrom - %f, to - %f\n", tfrom, tto);
		fastf_t deltax = (tto-tfrom)/50.0;
		for (fastf_t x = tfrom; x < tto; x = x + deltax) {
		    ON_3dPoint p = surf->PointAt(x, pt.y);
		    VMOVE(pt1, p);
		    if (x+deltax > tto) {
			p = surf->PointAt(tto, pt.y);
		    } else {
			p = surf->PointAt(x+deltax, pt.y);
		    }
		    VMOVE(pt2, p);
		    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}

	    }
	}

    }

    return;
}


void
drawisoUCheckForTrim(SurfaceTree* st, struct bn_vlblock *vbp, fastf_t from, fastf_t to, fastf_t v, int UNUSED(curveres))
{
    register struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    std::list<BRNode*> m_trims_right;
    std::list<fastf_t> trim_hits;

    vhead = rt_vlblock_find(vbp, YELLOW);

    const ON_Surface *surf = st->getSurface();
    CurveTree *ctree = st->ctree;
    fastf_t umin, umax;
    surf->GetDomain(0, &umin, &umax);

    m_trims_right.clear();

    fastf_t tol = 0.001;
    ON_2dPoint pt;
    pt.x = umin;
    pt.y = v;

    if (ctree != NULL) {
	m_trims_right.clear();
	ctree->getLeavesRight(m_trims_right, pt, tol);
    }

    int cnt = 1;
    //bu_log("V - %f\n", pt.x);
    trim_hits.clear();
    for (std::list<BRNode*>::iterator i = m_trims_right.begin(); i != m_trims_right.end(); i++, cnt++) {
	BRNode* br = dynamic_cast<BRNode*> (*i);

	point_t bmin, bmax;
	if (!br->m_Horizontal) {
	    br->GetBBox(bmin, bmax);
	    if (((bmin[Y] - tol) <= pt[Y]) && (pt[Y] <= (bmax[Y]+tol))) { //if check trim and in BBox
		fastf_t u = br->getCurveEstimateOfU(pt[Y], tol);
		trim_hits.push_back(u);
		//bu_log("%d U %d - %f pt %f, %f bmin %f, %f  bmax %f, %f\n", br->m_face->m_face_index, cnt, u, pt.x, pt.y, bmin[X], bmin[Y], bmax[X], bmax[Y]);
	    }
	}
    }
    trim_hits.sort();
    trim_hits.unique(near_equal);

    int hit_cnt = trim_hits.size();
    cnt = 1;
    //bu_log("\tdrawisoUCheckForTrim: hit_cnt %d from center  %f %f 0.0 to center %f %f 0.0\n", hit_cnt, from, v , to, v);

    if ((hit_cnt > 0) && ((hit_cnt % 2) == 0)) {
/*
  if ((hit_cnt % 2) != 0) {
  //bu_log("V - %f\n", pt.y);
  if (!trim_hits.empty()) {
  fastf_t end = trim_hits.front();
  trim_hits.pop_front();
  //bu_log("\tfrom - %f, to - %f\n", from, to);
  fastf_t deltax = (end - from) / 50.0;
  if (deltax > 0.001) {
  for (fastf_t x = from; x < end && x < to; x = x + deltax) {
  ON_3dPoint p = surf->PointAt(x, pt.y);
  VMOVE(pt1, p);
  if (x + deltax > end) {
  if (x + deltax > to) {
  p = surf->PointAt(to, pt.y);
  } else {
  p = surf->PointAt(end, pt.y);
  }
  } else {
  if (x + deltax > to) {
  p = surf->PointAt(to, pt.y);
  } else {
  p = surf->PointAt(x + deltax, pt.y);
  }
  }
  VMOVE(pt2, p);

  //bu_log(
  //		"\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
  //		cnt++, x, v, x + deltax, v);

  RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
  RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
  }
  }
  }
  }
*/
	while (!trim_hits.empty()) {
	    fastf_t start = trim_hits.front();
	    if (start < from) {
		start = from;
	    }
	    trim_hits.pop_front();
	    fastf_t end = trim_hits.front();
	    if (end > to) {
		end = to;
	    }
	    trim_hits.pop_front();
	    //bu_log("\tfrom - %f, to - %f\n", from, to);
	    fastf_t deltax = (end - start) / 50.0;
	    if (deltax > 0.001) {
		for (fastf_t x = start; x < end; x = x + deltax) {
		    ON_3dPoint p = surf->PointAt(x, pt.y);
		    VMOVE(pt1, p);
		    if (x + deltax > end) {
			p = surf->PointAt(end, pt.y);
		    } else {
			p = surf->PointAt(x + deltax, pt.y);
		    }
		    VMOVE(pt2, p);

		    //				bu_log(
		    //						"\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
		    //						cnt++, x, v, x + deltax, v);

		    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	    }
	}
    }

    return;
}


void
drawisoVCheckForTrim(SurfaceTree* st, struct bn_vlblock *vbp, fastf_t from, fastf_t to, fastf_t u, int UNUSED(curveres))
{
    register struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    std::list<BRNode*> m_trims_above;
    std::list<fastf_t> trim_hits;

    vhead = rt_vlblock_find(vbp, YELLOW);

    const ON_Surface *surf = st->getSurface();
    CurveTree *ctree = st->ctree;
    fastf_t vmin, vmax;
    surf->GetDomain(1, &vmin, &vmax);

    m_trims_above.clear();

    fastf_t tol = 0.001;
    ON_2dPoint pt;
    pt.x = u;
    pt.y = vmin;

    if (ctree != NULL) {
	m_trims_above.clear();
	ctree->getLeavesAbove(m_trims_above, pt, tol);
    }

    int cnt = 1;
    trim_hits.clear();
    for (std::list<BRNode*>::iterator i = m_trims_above.begin(); i != m_trims_above.end(); i++, cnt++) {
	BRNode* br = dynamic_cast<BRNode*>(*i);

	point_t bmin, bmax;
	if (!br->m_Vertical) {
	    br->GetBBox(bmin, bmax);

	    if (((bmin[X] - tol) <= pt[X]) && (pt[X] <= (bmax[X]+tol))) { //if check trim and in BBox
		fastf_t v = br->getCurveEstimateOfV(pt[X], tol);
		trim_hits.push_back(v);
		//bu_log("%d V %d - %f pt %f, %f bmin %f, %f  bmax %f, %f\n", br->m_face->m_face_index, cnt, v, pt.x, pt.y, bmin[X], bmin[Y], bmax[X], bmax[Y]);
	    }
	}
    }
    trim_hits.sort();
    trim_hits.unique(near_equal);

    size_t hit_cnt = trim_hits.size();
    cnt = 1;

    //bu_log("\tdrawisoVCheckForTrim: hit_cnt %d from center  %f %f 0.0 to center %f %f 0.0\n", hit_cnt, u, from, u, to);

    if ((hit_cnt > 0) && ((hit_cnt % 2) == 0)) {
/*
  if ((hit_cnt % 2) != 0) { //odd starting inside
  //bu_log("V - %f\n", pt.y);
  if (!trim_hits.empty()) {
  fastf_t end = trim_hits.front();
  trim_hits.pop_front();
  //bu_log("\tfrom - %f, to - %f\n", from, to);
  fastf_t deltay = (end - from) / 50.0;
  if (deltay > 0.001) {
  for (fastf_t y = from; y < end && y < to; y = y + deltay) {
  ON_3dPoint p = surf->PointAt(pt.x, y);
  VMOVE(pt1, p);
  if (y + deltay > end) {
  if (y + deltay > to) {
  p = surf->PointAt(pt.x, to);
  } else {
  p = surf->PointAt(pt.x, end);
  }
  } else {
  if (y + deltay > to) {
  p = surf->PointAt(pt.x, to);
  } else {
  p = surf->PointAt(pt.x, y + deltay);
  }
  }
  VMOVE(pt2, p);

  //bu_log(
  //		"\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
  //		cnt++, u, y, u, y + deltay);

  RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
  RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
  }
  }

  }
  }
*/
	while (!trim_hits.empty()) {
	    fastf_t start = trim_hits.front();
	    trim_hits.pop_front();
	    if (start < from) {
		start = from;
	    }
	    fastf_t end = trim_hits.front();
	    trim_hits.pop_front();
	    if (end > to) {
		end = to;
	    }
	    //bu_log("\tfrom - %f, to - %f\n", from, to);
	    fastf_t deltay = (end - start) / 50.0;
	    if (deltay > 0.001) {
		for (fastf_t y = start; y < end; y = y + deltay) {
		    ON_3dPoint p = surf->PointAt(pt.x, y);
		    VMOVE(pt1, p);
		    if (y + deltay > end) {
			p = surf->PointAt(pt.x, end);
		    } else {
			p = surf->PointAt(pt.x, y + deltay);
		    }
		    VMOVE(pt2, p);

		    //bu_log("\t\t%d from center  %f %f 0.0 to center %f %f 0.0\n",
		    //		cnt++, u, y, u, y + deltay);

		    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
		    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	    }
	}
    }
    return;
}


void
drawisoU(SurfaceTree* st, struct bn_vlblock *vbp, fastf_t from, fastf_t to, fastf_t v, int curveres)
{
    register struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    fastf_t deltau = (to - from) / curveres;
    const ON_Surface *surf = st->getSurface();

    vhead = rt_vlblock_find(vbp, YELLOW);
    for (fastf_t u = from; u < to; u = u + deltau) {
	ON_3dPoint p = surf->PointAt(u, v);
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
	VMOVE(pt1, p);
	if (u + deltau > to) {
	    p = surf->PointAt(to, v);
	} else {
	    p = surf->PointAt(u + deltau, v);
	}
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
	VMOVE(pt2, p);
	RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
    }
}


void
drawisoV(SurfaceTree* st, struct bn_vlblock *vbp, fastf_t from, fastf_t to, fastf_t u, int curveres)
{
    register struct bu_list *vhead;
    fastf_t pt1[3], pt2[3];
    fastf_t deltav = (to - from) / curveres;
    const ON_Surface *surf = st->getSurface();

    vhead = rt_vlblock_find(vbp, YELLOW);
    for (fastf_t v = from; v < to; v = v + deltav) {
	ON_3dPoint p = surf->PointAt(u, v);
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y, p.x, p.y, p.z);
	VMOVE(pt1, p);
	if (v + deltav > to) {
	    p = surf->PointAt(u, to);
	} else {
	    p = surf->PointAt(u, v + deltav);
	}
	//bu_log("p1 2d - %f, %f 3d - %f, %f, %f\n", pt.x, y+deltay, p.x, p.y, p.z);
	VMOVE(pt2, p);
	RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
    }
}


void
drawBBNode(SurfaceTree* st, struct bn_vlblock *vbp, BBNode * node) {
    if (node->isLeaf()) {
	//draw leaf
	if (node->m_trimmed) {
	    return; // nothing to do node is trimmed
	} else if (node->m_checkTrim) { // node may contain trim check all corners
	    fastf_t u =  node->m_u[0];
	    fastf_t v = node->m_v[0];
	    fastf_t from = u;
	    fastf_t to = node->m_u[1];
	    //bu_log("drawBBNode: node %x uvmin center %f %f 0.0, uvmax center %f %f 0.0\n", node, node->m_u[0], node->m_v[0], node->m_u[1], node->m_v[1]);

	    drawisoUCheckForTrim(st, vbp, from, to, v, 3); //bottom
	    v = node->m_v[1];
	    drawisoUCheckForTrim(st, vbp, from, to, v, 3); //top
	    from = node->m_v[0];
	    to = node->m_v[1];
	    drawisoVCheckForTrim(st, vbp, from, to, u, 3); //left
	    u = node->m_u[1];
	    drawisoVCheckForTrim(st, vbp, from, to, u, 3); //right

	    return;
	} else { // fully untrimmed just draw bottom and right edges
	    fastf_t u =  node->m_u[0];
	    fastf_t v = node->m_v[0];
	    fastf_t from = u;
	    fastf_t to = node->m_u[1];
	    drawisoU(st, vbp, from, to, v, 10); //bottom
	    from = v;
	    to = node->m_v[1];
	    drawisoV(st, vbp, from, to, u, 10); //right
	    return;
	}
    } else {
	if (node->m_children.size() > 0) {
	    for (std::vector<BBNode*>::iterator childnode = node->m_children.begin(); childnode
		     != node->m_children.end(); childnode++) {
		drawBBNode(st, vbp, *childnode);
	    }
	}
    }
}


void
plotFaceFromSurfaceTree(SurfaceTree* st, struct bn_vlblock *vbp, int UNUSED(isocurveres), int UNUSED(gridres)) {
    BBNode *root = st->getRootNode();
    drawBBNode(st, vbp, root);
}


int
brep_isosurface_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, int index, int plotres)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }
    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    SurfaceTree* st = new SurfaceTree(&face, true, 0);
	    plottrim(face, vbp, plotres, true);
	    plotFaceFromSurfaceTree(st, vbp, plotres, plotres);
	    delete st;
	}
    } else if (index < brep->m_F.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    SurfaceTree* st = new SurfaceTree(&face, true, 0);
	    plottrim(face, vbp, plotres, true);
	    plotFaceFromSurfaceTree(st, vbp, plotres, plotres);
	    delete st;
	}
    }

    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;
}


int
brep_surfaceleafs_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, bool dim3d, int index, int)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    const ON_Surface *s = face.SurfaceOf();
	    double surface_width,surface_height;
	    if (s->GetSurfaceSize(&surface_width,&surface_height)) {
		// reparameterization of the face's surface and transforms the "u"
		// and "v" coordinates of all the face's parameter space trimming
		// curves to minimize distortion in the map from parameter space to 3d..
		face.SetDomain(0, 0.0, surface_width);
		face.SetDomain(1, 0.0, surface_height);
	    }
	    SurfaceTree* st = new SurfaceTree(&face);
	    bu_log("Face: %d contains %d SBBs",index,plotsurfaceleafs(st, vbp, dim3d));
	}
    } else if (index < brep->m_F.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    const ON_Surface *s = face.SurfaceOf();
	    double surface_width,surface_height;
	    if (s->GetSurfaceSize(&surface_width,&surface_height)) {
		// reparameterization of the face's surface and transforms the "u"
		// and "v" coordinates of all the face's parameter space trimming
		// curves to minimize distortion in the map from parameter space to 3d..
		face.SetDomain(0, 0.0, surface_width);
		face.SetDomain(1, 0.0, surface_height);
	    }
	    SurfaceTree* st = new SurfaceTree(&face);
	    bu_log("Face: %d contains %d SBBs",index,plotsurfaceleafs(st, vbp, dim3d));
	}
    }

    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;
}


int
brep_trimleafs_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal*, struct bn_vlblock *vbp, bool dim3d, int index, int)
{
    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = bs->brep;
    if (brep == NULL) {
	return -1;
    }
    if (!brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
    }

    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    SurfaceTree* st = new SurfaceTree(&face);
	    plottrimleafs(st, vbp, dim3d);
	}
    } else if (index < brep->m_F.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    SurfaceTree* st = new SurfaceTree(&face);
	    plottrimleafs(st, vbp, dim3d);
	}
    }

    bu_vls_printf(vls, ON_String(wstr).Array());
    return 0;
}


void
info_usage(struct bu_vls *vls)
{
    bu_vls_printf(vls, "mged>brep brepname.s info\n");
    bu_vls_printf(vls, "\tinfo - return count information for specific BREP\n");
    bu_vls_printf(vls, "\tinfo C [index] or C [index-index] - return information for specific BREP '3D curve'\n");
    bu_vls_printf(vls, "\tinfo S [index] or S [index-index] - return information for specific BREP 'surface'\n");
    bu_vls_printf(vls, "\tinfo F [index] or F [index-index]- return information for specific BREP 'face'\n");
    bu_vls_printf(vls, "\tinfo T [index] or T [index-index]- return information for specific BREP 'trim'\n");
    bu_vls_printf(vls, "\tinfo E [index] or E [index-index]- return information for specific BREP 'edge'\n");
    bu_vls_printf(vls, "\tinfo TB [index] or TB [index-index]- return information for specific BREP 'piecewise bezier trim'\n");
    bu_vls_printf(vls, "\tinfo SB [index] or SB [index-index]- return information for specific BREP 'piecewise bezier surface'\n");
}


void
plot_usage(struct bu_vls *vls)
{
    bu_vls_printf(vls, "mged>brep brepname.s plot\n");
    bu_vls_printf(vls, "\tplot - plot entire BREP\n");
    bu_vls_printf(vls, "\tplot S [index] or S [index-index]- plot specific BREP 'surface'\n");
    bu_vls_printf(vls, "\tplot Suv index[-index] u v- plot specific BREP 'surface' 3d point at specified uv\n");
    bu_vls_printf(vls, "\tplot UV index[-index] u1 u2 v1 v2 - plot specific BREP 'surface' 3d bounds at specified uv bounds\n");
    bu_vls_printf(vls, "\tplot F [index] or F [index-index]- plot specific BREP 'face'\n");
    bu_vls_printf(vls, "\tplot I [index] or I [index-index]- plot specific BREP 'isosurface'\n");
    bu_vls_printf(vls, "\tplot SN [index] or SN [index-index]- plot specific BREP 'surface normal'\n");
    bu_vls_printf(vls, "\tplot KN [index] or KN [index-index]- plot specific BREP 'surface knot'\n");
    bu_vls_printf(vls, "\tplot F2d [index] or F2d [index-index]- plot specific BREP 'face in 2d'\n");
    bu_vls_printf(vls, "\tplot SBB [index] or SBB [index-index]- plot specific BREP 'surfaceleafs'\n");
    bu_vls_printf(vls, "\tplot SBB2d [index] or SBB2d [index-index]- plot specific BREP 'surfaceleafs in 2d'\n");
    bu_vls_printf(vls, "\tplot TD [index] or TD [index-index]- plot specific BREP 'trim direction'\n");
    bu_vls_printf(vls, "\tplot T [index] or T [index-index]- plot specific BREP 'trim'\n");
    bu_vls_printf(vls, "\tplot T2d [index] or T2d [index-index]- plot specific BREP 'trim in 2d'\n");
    bu_vls_printf(vls, "\tplot TBB [index] or TBB [index-index]- plot specific BREP 'trimleafs'\n");
    bu_vls_printf(vls, "\tplot TBB2d [index] or TBB2d [index-index]- plot specific BREP 'trimleafs in 2d'\n");
    bu_vls_printf(vls, "\tplot E [index] or E [index-index]- plot specific BREP 'edge3d'\n");
    bu_vls_printf(vls, "\tplot SCV [index] or SCV [index-index]- plot specific BREP 'nurbs control net'\n");
}


int
single_conversion(struct rt_db_internal* intern, ON_Brep** brep, const struct db_i* dbip)
{
    if (*brep)
	delete *brep;

    if (intern->idb_type == ID_BREP) {
	// already a brep
	RT_BREP_CK_MAGIC(intern->idb_ptr);
	*brep = ((struct rt_brep_internal *)intern->idb_ptr)->brep->Duplicate();
	if (*brep != NULL)
	    return 0;
	return -2;
    }

    *brep = ON_Brep::New();
    ON_Brep *old = *brep;
    struct bn_tol tol;
    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = SMALL_FASTF;
    tol.para = 1.0 - tol.perp;
    if (intern->idb_type == ID_COMBINATION) {
	rt_comb_brep(brep, intern, &tol, dbip);
    } else {
	if (intern->idb_meth->ft_brep == NULL) {
	    delete old;
	    *brep = NULL;
	    return -1;
	}
	intern->idb_meth->ft_brep(brep, intern, &tol);
    }
    if (*brep == NULL) {
	delete old;
	return -2;
    }
    return 0;
}


int
brep_conversion(struct rt_db_internal* in, struct rt_db_internal* out, const struct db_i *dbip)
{
    if (out == NULL)
	return -1;

    ON_Brep* brep = NULL;
    int ret;
    if ((ret = single_conversion(in, &brep, dbip)) < 0) {
	return ret;
    }

    struct rt_brep_internal *bip_out;
    BU_ALLOC(bip_out, struct rt_brep_internal);
    bip_out->magic = RT_BREP_INTERNAL_MAGIC;
    bip_out->brep = brep;
    RT_DB_INTERNAL_INIT(out);
    out->idb_ptr = (void *)bip_out;
    out->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    out->idb_meth = &OBJ[ID_BREP];
    out->idb_minor_type = ID_BREP;

    return 0;
}


int
brep_conversion_tree(const struct db_i *dbip, const union tree *oldtree, union tree *newtree, const char *suffix, struct rt_wdb *wdbp, fastf_t local2mm)
{
    int ret = 0;
    *newtree = *oldtree;
    rt_comb_internal *comb = NULL;
    switch (oldtree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    /* convert right */
	    comb = new rt_comb_internal;
	    newtree->tr_b.tb_right = new tree;
	    RT_TREE_INIT(newtree->tr_b.tb_right);
	    ret = brep_conversion_tree(dbip, oldtree->tr_b.tb_right, newtree->tr_b.tb_right, suffix, wdbp, local2mm);
	    if (ret) {
		delete newtree->tr_b.tb_right;
		break;
	    }
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    /* convert left */
	    BU_ALLOC(newtree->tr_b.tb_left, union tree);
	    RT_TREE_INIT(newtree->tr_b.tb_left);
	    ret = brep_conversion_tree(dbip, oldtree->tr_b.tb_left, newtree->tr_b.tb_left, suffix, wdbp, local2mm);
	    if (!ret) {
		comb->tree = newtree;
	    } else {
		delete newtree->tr_b.tb_left;
		delete newtree->tr_b.tb_right;
	    }
	    break;
	case OP_DB_LEAF:
	    char *tmpname;
	    char *oldname;
	    oldname = oldtree->tr_l.tl_name;
	    tmpname = (char*)bu_malloc(strlen(oldname)+strlen(suffix)+1, "char");
	    newtree->tr_l.tl_name = (char*)bu_malloc(strlen(oldname)+strlen(suffix)+1, "char");
	    bu_strlcpy(tmpname, oldname, strlen(oldname)+1);
	    bu_strlcat(tmpname, suffix, strlen(oldname)+strlen(suffix)+1);
	    if (db_lookup(dbip, tmpname, LOOKUP_QUIET) == RT_DIR_NULL) {
		directory *dir;
		dir = db_lookup(dbip, oldname, LOOKUP_QUIET);
		if (dir != RT_DIR_NULL) {
		    rt_db_internal *intern;
		    BU_ALLOC(intern, struct rt_db_internal);
		    rt_db_get_internal(intern, dir, dbip, bn_mat_identity, &rt_uniresource);
		    if (BU_STR_EQUAL(intern->idb_meth->ft_name, "ID_COMBINATION")) {
			ret = brep_conversion_comb(intern, tmpname, suffix, wdbp, local2mm);
			if (ret) {
			    bu_free(tmpname, "char");
			    rt_db_free_internal(intern);
			    break;
			}
			bu_strlcpy(newtree->tr_l.tl_name, tmpname, strlen(tmpname)+1);
			bu_free(tmpname, "char");
			break;
		    }
		    // It's a primitive. If it's a b-rep object, just duplicate it. Otherwise call the
		    // function to convert it to b-rep.
		    ON_Brep** brep;

		    BU_ALLOC(brep, ON_Brep*);

		    if (BU_STR_EQUAL(intern->idb_meth->ft_name, "ID_BREP")) {
			*brep = ((struct rt_brep_internal *)intern->idb_ptr)->brep->Duplicate();
		    } else {
			ret = single_conversion(intern, brep, dbip);
			if (ret == -1) {
			    bu_log("The brep conversion of %s is unsuccessful.\n", oldname);
			    newtree = NULL;
			    bu_free(tmpname, "char");
			    bu_free(brep, "ON_Brep*");
			    break;
			} else if (ret == -2) {
			    ret = wdb_export(wdbp, tmpname, intern->idb_ptr, intern->idb_type, local2mm);
			    if (ret) {
				bu_log("ERROR: failure writing [%s] to disk\n", tmpname);
			    } else {
				bu_log("The conversion of [%s] (type: %s) is skipped. Implicit form remains as %s.\n",
				       oldname, intern->idb_meth->ft_label, tmpname);
				bu_strlcpy(newtree->tr_l.tl_name, tmpname, strlen(tmpname)+1);
			    }
			    bu_free(tmpname, "char");
			    bu_free(brep, "ON_Brep*");
			    break;
			}
		    }
		    if (brep != NULL) {
			rt_brep_internal *bi;
			BU_ALLOC(bi, struct rt_brep_internal);
			bi->magic = RT_BREP_INTERNAL_MAGIC;
			bi->brep = *brep;
			ret = wdb_export(wdbp, tmpname, (void *)bi, ID_BREP, local2mm);
			if (ret) {
			    bu_log("ERROR: failure writing [%s] to disk\n", tmpname);
			} else {
			    bu_log("%s is made.\n", tmpname);
			    bu_strlcpy(newtree->tr_l.tl_name, tmpname, strlen(tmpname)+1);
			}
		    } else {
			bu_log("The brep conversion of %s is unsuccessful.\n", oldname);
			newtree = NULL;
			ret = -1;
		    }
		    bu_free(brep, "ON_Brep*");
		} else {
		    bu_log("Cannot find %s.\n", oldname);
		    newtree = NULL;
		    ret = -1;
		}
	    } else {
		bu_log("%s already exists.\n", tmpname);
		bu_strlcpy(newtree->tr_l.tl_name, tmpname, strlen(tmpname)+1);
	    }
	    bu_free(tmpname, "char");
	    break;
	default:
	    bu_log("OPCODE NOT IMPLEMENTED: %d\n", oldtree->tr_op);
	    ret = -1;
    }
    if (comb)
	delete comb;
    return ret;
}


int
brep_conversion_comb(struct rt_db_internal *old_internal, const char *name, const char *suffix, struct rt_wdb *wdbp, fastf_t local2mm)
{
    RT_CK_COMB(old_internal->idb_ptr);
    rt_comb_internal *comb_internal;
    comb_internal = (rt_comb_internal *)old_internal->idb_ptr;
    int ret;
    if (comb_internal->tree == NULL) {
	// Empty tree. Also output an empty comb.
	ret = wdb_export(wdbp, name, comb_internal, ID_COMBINATION, local2mm);
	if (ret)
	    return ret;
	bu_log("%s is made.\n", name);
	return 0;
    }
    RT_CK_TREE(comb_internal->tree);
    union tree *oldtree = comb_internal->tree;
    rt_comb_internal *new_internal;

    BU_ALLOC(new_internal, struct rt_comb_internal);
    *new_internal = *comb_internal;
    BU_ALLOC(new_internal->tree, union tree);
    RT_TREE_INIT(new_internal->tree);

    union tree *newtree = new_internal->tree;

    ret = brep_conversion_tree(wdbp->dbip, oldtree, newtree, suffix, wdbp, local2mm);
    if (!ret) {
	ret = wdb_export(wdbp, name, (void *)new_internal, ID_COMBINATION, local2mm);
    } else {
	bu_free(new_internal->tree, "tree");
	bu_free(new_internal, "rt_comb_internal");
    }
    if (!ret)
	bu_log("%s is made.\n", name);

    return ret;
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

int
translate_command(
	struct bu_vls *result,
	struct brep_specific *bs,
	int argc,
	const char *argv[])
{
    //  0         1          2      3    4   5 6 7  8  9
    // brep <solid_name> translate SCV index i j dx dy dz
    if (argc != 10) {
	return -1;
    }
    ON_Brep *brep = bs->brep;

    if (BU_STR_EQUAL(argv[3], "SCV")) {
	int surface_index = atoi(argv[4]);
	int i = atoi(argv[5]);
	int j = atoi(argv[6]);
	fastf_t dx = atof(argv[7]);
	fastf_t dy = atof(argv[8]);
	fastf_t dz = atof(argv[9]);

	int ret = brep_translate_scv(brep, surface_index, i, j, dx, dy, dz);
	switch (ret) {
	    case -1:
		bu_vls_printf(result, "No surface %d.\n", surface_index);
		break;
	    case -2:
		bu_vls_printf(result, "Surface %d is not a NURBS surface.\n", surface_index);
		break;
	    case -3:
		bu_vls_printf(result, "No control vertex (%d, %d).\n", i, j);
	}
	return ret;
    } else {
	return -1;
    }

    return 0;
}


int
brep_command(struct bu_vls *vls, const char *solid_name, const struct rt_tess_tol *ttol, const struct bn_tol *tol, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int argc, const char *argv[], char *commtag)
{
    const char *command;
    int ret = 0;

    if (argc == 2)
	command = "info";
    else
	command = (const char *) argv[2];

    snprintf(commtag, 64, "_BC_"); //default name pre/postfix tag for fake bn_vlblock solid

    if (BU_STR_EQUAL(command, "info")) {
	if (argc == 3) {
	    ret = brep_info(bs, vls);
	    info_usage(vls);
	} else if (argc == 4) {
	    const char *part = argv[3];
	    ON_Brep *brep = bs->brep;
	    if (BU_STR_EQUAL(part, "S")) {
		for (int i = 0; i < brep->m_S.Count(); ++i) {
		    ret = brep_surface_info(bs, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "F")) {
		for (int i = 0; i < brep->m_F.Count(); ++i) {
		    ret = brep_face_info(bs, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "T")) {
		for (int i = 0; i < brep->m_T.Count(); ++i) {
		    ret = brep_trim_info(bs, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "E")) {
		for (int i = 0; i < brep->m_E.Count(); ++i) {
		    ret = brep_edge_info(bs, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "SB")) {
		for (int i = 0; i < brep->m_S.Count(); ++i) {
		    ret = brep_surface_bezier_info(bs, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "TB")) {
		for (int i = 0; i < brep->m_T.Count(); ++i) {
		    ret = brep_trim_bezier_info(bs, vls, i);
		}
	    } else if (BU_STR_EQUAL(part, "C")) {
		for (int i = 0; i < brep->m_C3.Count(); ++i) {
		    ret = brep_curve_info(bs, vls, i);
		}
	    }
	} else if (argc == 5) {
	    const char *part = argv[3];
	    const char *strindex = argv[4];
	    int startindex = -1;
	    int endindex = -1;
	    if (BU_STR_EQUAL(strindex, "all")) {
		ON_Brep *brep = bs->brep;
		if (BU_STR_EQUAL(part, "S")) {
		    for (int i = 0; i < brep->m_S.Count(); ++i) {
			ret = brep_surface_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "F")) {
		    for (int i = 0; i < brep->m_F.Count(); ++i) {
			ret = brep_face_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "T")) {
		    for (int i = 0; i < brep->m_T.Count(); ++i) {
			ret = brep_trim_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "E")) {
		    for (int i = 0; i < brep->m_E.Count(); ++i) {
			ret = brep_edge_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "SB")) {
		    for (int i = 0; i < brep->m_S.Count(); ++i) {
			ret = brep_surface_bezier_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "TB")) {
		    for (int i = 0; i < brep->m_T.Count(); ++i) {
			ret = brep_trim_bezier_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "C")) {
		    for (int i = 0; i < brep->m_C3.Count(); ++i) {
			ret = brep_curve_info(bs, vls, i);
		    }
		}
	    } else if (BU_STR_EQUAL(strindex, "?")) {
		info_usage(vls);
	    } else {
		ON_Brep *brep = bs->brep;
		const char *dash = strchr(strindex, '-');
		if (dash) {
		    struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
		    bu_vls_strcpy(&tmpstr, strindex);
		    bu_vls_trunc(&tmpstr, dash - strindex);
		    startindex = atoi(bu_vls_addr(&tmpstr));
		    bu_vls_strcpy(&tmpstr, ++dash);
		    endindex = atoi(bu_vls_addr(&tmpstr));
		    bu_vls_free(&tmpstr);
		} else {
		    startindex = atoi(strindex);
		    endindex = startindex;
		}
		if (BU_STR_EQUAL(part, "S")) {
		    int S_count = brep->m_S.Count() - 1 ? endindex : brep->m_S.Count() - 1 < endindex;
		    for (int i = startindex; i <= S_count; ++i) {
			ret = brep_surface_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "F")) {
		    int F_count = brep->m_F.Count() - 1 ? endindex : brep->m_F.Count() - 1 < endindex;
		    for (int i = startindex; i <= F_count; ++i) {
			ret = brep_face_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "T")) {
		    int T_count = brep->m_T.Count() - 1 ? endindex : brep->m_T.Count() - 1 < endindex;
		    for (int i = startindex; i <= T_count; ++i) {
			ret = brep_trim_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "E")) {
		    int E_count = brep->m_E.Count() - 1 ? endindex : brep->m_E.Count() - 1 < endindex;
		    for (int i = startindex; i <= E_count; ++i) {
			ret = brep_edge_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "SB")) {
		    int S_count = brep->m_S.Count() - 1 ? endindex : brep->m_S.Count() - 1 < endindex;
		    for (int i = startindex; i <= S_count; ++i) {
			ret = brep_surface_bezier_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "TB")) {
		    int T_count = brep->m_T.Count() - 1 ? endindex : brep->m_T.Count() - 1 < endindex;
		    for (int i = startindex; i <= T_count; ++i) {
			ret = brep_trim_bezier_info(bs, vls, i);
		    }
		} else if (BU_STR_EQUAL(part, "C")) {
		    int C_count = brep->m_C3.Count() - 1 ? endindex : brep->m_C3.Count() - 1 < endindex;
		    for (int i = startindex; i <= C_count; ++i) {
			ret = brep_curve_info(bs, vls, i);
		    }
		}
	    }
	}
    } else if (BU_STR_EQUAL(command, "plot")) {
	if (argc == 3) {
	    plot_usage(vls);
	} else if (argc >= 4) {
	    const char *part = argv[3];
	    int startindex = -1;
	    int endindex = -1;
	    int numpoints = -1;
	    int plotres = 100;
	    if (argc == 6) {
		const char *strres = argv[5];
		plotres = numpoints = atoi(strres);
	    }
	    if (argc >= 5) {
		const char *str = argv[4];
		if (BU_STR_EQUAL(str, "all")) {
		    startindex = endindex = -1;
		} else if (BU_STR_EQUAL(str, "?")) {
		    plot_usage(vls);
		} else {
		    const char *dash = strchr(str, '-');
		    if (dash) {
			struct bu_vls tmpstr = BU_VLS_INIT_ZERO;
			bu_vls_strcpy(&tmpstr, str);
			bu_vls_trunc(&tmpstr, dash - str);
			startindex = atoi(bu_vls_addr(&tmpstr));
			bu_vls_strcpy(&tmpstr, ++dash);
			endindex = atoi(bu_vls_addr(&tmpstr));
			bu_vls_free(&tmpstr);
		    } else {
			startindex = atoi(str);
			endindex = startindex;
		    }
		}
	    }
	    if (BU_STR_EQUAL(part, "S")) {
		snprintf(commtag, 64, "_BC_S_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_surface_plot(vls, bs, bi, vbp, i, plotres);
		}
	    } else if (BU_STR_EQUAL(part, "Suv")) {
		double u = 0.0;
		double v = 0.0;
		if (argc == 7) {
		    const char *ustr = argv[5];
		    const char *vstr = argv[6];

		    u = atof(ustr);
		    v = atof(vstr);
		}
		snprintf(commtag, 64, "_BC_Suv_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_surface_uv_plot(vls, bs, bi, vbp, i, u, v);
		}
	    } else if (BU_STR_EQUAL(part, "UV")) {
		ON_Interval u;
		ON_Interval v;
		if (argc == 9) {
		    const char *u1str = argv[5];
		    const char *u2str = argv[6];
		    const char *v1str = argv[7];
		    const char *v2str = argv[8];

		    u.m_t[0] = atof(u1str);
		    u.m_t[1] = atof(u2str);
		    v.m_t[0] = atof(v1str);
		    v.m_t[1] = atof(v2str);
		}
		snprintf(commtag, 64, "_BC_UV_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_surface_uv_plot(vls, bs, bi, vbp, i, u, v);
		}
	    } else if (BU_STR_EQUAL(part, "I")) {
		snprintf(commtag, 64, "_BC_I_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_isosurface_plot(vls, bs, bi, vbp, i, plotres);
		}
	    } else if (BU_STR_EQUAL(part, "SN")) {
		snprintf(commtag, 64, "_BC_SN_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_surface_normal_plot(vls, bs, bi, vbp, i,
						   plotres);
		}
	    } else if (BU_STR_EQUAL(part, "KN2d")) {
		snprintf(commtag, 64, "_BC_KN2d_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_surface_knot_plot(vls, bs, bi, vbp, i, false);
		}
	    } else if (BU_STR_EQUAL(part, "KN")) {
		snprintf(commtag, 64, "_BC_KN_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_surface_knot_plot(vls, bs, bi, vbp, i, true);
		}
	    } else if (BU_STR_EQUAL(part, "F")) {
		snprintf(commtag, 64, "_BC_F_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_facetrim_plot(vls, bs, bi, vbp, i, plotres,
					     true);
		}
	    } else if (BU_STR_EQUAL(part, "F2d")) {
		snprintf(commtag, 64, "_BC_F2d_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_facetrim_plot(vls, bs, bi, vbp, i, plotres,
					     false);
		}
	    } else if (BU_STR_EQUAL(part, "FCDT")) {
		snprintf(commtag, 64, "_BC_FCDT_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_facecdt_plot(vls, solid_name, ttol, tol, bs, bi, vbp, i, 0);
		}
	    } else if (BU_STR_EQUAL(part, "FCDTw")) {
		snprintf(commtag, 64, "_BC_FCDT_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_facecdt_plot(vls, solid_name, ttol, tol, bs, bi, vbp, i, 1, numpoints);
		}
	    } else if (BU_STR_EQUAL(part, "FCDT2d")) {
		snprintf(commtag, 64, "_BC_FCDT2d_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_facecdt_plot(vls, solid_name, ttol, tol, bs, bi, vbp, i, 2);
		}
	    } else if (BU_STR_EQUAL(part, "FCDTm2d")) {
		snprintf(commtag, 64, "_BC_FCDTm2d_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_facecdt_plot(vls, solid_name, ttol, tol, bs, bi, vbp, i, 3, numpoints);
		}
	    } else if (BU_STR_EQUAL(part, "FCDTp2d")) {
		snprintf(commtag, 64, "_BC_FCDTp2d_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_facecdt_plot(vls, solid_name, ttol, tol, bs, bi, vbp, i, 4, numpoints);
		}
	    } else if (BU_STR_EQUAL(part, "SBB")) {
		snprintf(commtag, 64, "_BC_SBB_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_surfaceleafs_plot(vls, bs, bi, vbp, true, i,
						 plotres);
		}
	    } else if (BU_STR_EQUAL(part, "SBB2d")) {
		snprintf(commtag, 64, "_BC_SBB2d_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_surfaceleafs_plot(vls, bs, bi, vbp, false, i,
						 plotres);
		}
	    } else if (BU_STR_EQUAL(part, "TD")) {
		snprintf(commtag, 64, "_BC_TD_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_trim_direction_plot(vls, bs, bi, vbp, i,
						   plotres);
		}
	    } else if (BU_STR_EQUAL(part, "T")) {
		snprintf(commtag, 64, "_BC_T_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_trim_plot(vls, bs, bi, vbp, i, plotres, true);
		}
	    } else if (BU_STR_EQUAL(part, "T2d")) {
		snprintf(commtag, 64, "_BC_T2d_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_trim_plot(vls, bs, bi, vbp, i, plotres, false);
		}
	    } else if (BU_STR_EQUAL(part, "TBB")) {
		snprintf(commtag, 64, "_BC_TBB_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_trimleafs_plot(vls, bs, bi, vbp, true, i,
					      plotres);
		}
	    } else if (BU_STR_EQUAL(part, "TBB2d")) {
		snprintf(commtag, 64, "_BC_TBB2d_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_trimleafs_plot(vls, bs, bi, vbp, false, i,
					      plotres);
		}
	    } else if (BU_STR_EQUAL(part, "E")) {
		snprintf(commtag, 64, "_BC_E_");
		for (int i = startindex; i <= endindex; i++) {
		    ret = brep_edge3d_plot(vls, bs, bi, vbp, i, plotres);
		}
	    } else if (BU_STR_EQUAL(part, "SCV")) {
		snprintf(commtag, 64, "_BC_SCV_");
		for (int i = startindex; i <= endindex; ++i) {
		    ret = brep_surface_cv_plot(vls, bs, bi, vbp, i);
		}
	    }
	}
    } else if (BU_STR_EQUAL(command, "translate")) {
	ret = translate_command(vls, bs, argc, argv);
    }
    return ret;
}


int
brep_intersect_point_point(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    ON_Brep *brep1 = bi1->brep;
    ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_V.Count() || j < 0 || j >= brep2->m_V.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_V.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_V.Count() - 1);
	return -1;
    }

    ON_ClassArray<ON_PX_EVENT> events;
    if (ON_Intersect(brep1->m_V[i].Point(), brep2->m_V[j].Point(), events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_point_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    ON_Brep *brep1 = bi1->brep;
    ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_V.Count() || j < 0 || j >= brep2->m_C3.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_V.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_C3.Count() - 1);
	return -1;
    }

    ON_ClassArray<ON_PX_EVENT> events;
    if (ON_Intersect(brep1->m_V[i].Point(), *(brep2->m_C3[j]), events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_point_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    ON_Brep *brep1 = bi1->brep;
    ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_V.Count() || j < 0 || j >= brep2->m_S.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_V.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_S.Count() - 1);
	return -1;
    }

    ON_ClassArray<ON_PX_EVENT> events;
    if (ON_Intersect(brep1->m_V[i].Point(), *(brep2->m_S[j]), events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_curve_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    ON_Brep *brep1 = bi1->brep;
    ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_C3.Count() || j < 0 || j >= brep2->m_C3.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_C3.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_C3.Count() - 1);
	return -1;
    }

    ON_SimpleArray<ON_X_EVENT> events;
    if (ON_Intersect(brep1->m_C3[i], brep2->m_C3[j], events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_curve_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    ON_Brep *brep1 = bi1->brep;
    ON_Brep *brep2 = bi2->brep;

    if (i < 0 || i >= brep1->m_C3.Count() || j < 0 || j >= brep2->m_S.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_C3.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_S.Count() - 1);
	return -1;
    }

    ON_SimpleArray<ON_X_EVENT> events;
    if (ON_Intersect(brep1->m_C3[i], brep2->m_S[j], events)) {
	for (int k = 0; k < events.Count(); k++) {
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    events[k].Dump(textlog);
	    ON_String str = ON_String(wstr);
	    bu_log("Intersection event %d:\n %s", k + 1, str.Array());
	}
    } else {
	bu_log("No intersection.\n");
    }

    return 0;
}


int
brep_intersect_surface_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j, struct bn_vlblock *vbp)
{
    RT_CK_DB_INTERNAL(intern1);
    RT_CK_DB_INTERNAL(intern2);
    struct rt_brep_internal *bi1, *bi2;
    bi1 = (struct rt_brep_internal *)intern1->idb_ptr;
    bi2 = (struct rt_brep_internal *)intern2->idb_ptr;
    RT_BREP_CK_MAGIC(bi1);
    RT_BREP_CK_MAGIC(bi2);

    ON_Brep *brep1 = bi1->brep;
    ON_Brep *brep2 = bi2->brep;

    ON_NurbsSurface surf1;
    ON_NurbsSurface surf2;

    if (i < 0 || i >= brep1->m_S.Count() || j < 0 || j >= brep2->m_S.Count()) {
	bu_log("Out of range: \n");
	bu_log("\t0 <= i <= %d\n", brep1->m_S.Count() - 1);
	bu_log("\t0 <= j <= %d\n", brep2->m_S.Count() - 1);
	return -1;
    }

    brep1->m_S[i]->GetNurbForm(surf1);
    brep2->m_S[j]->GetNurbForm(surf2);

    ON_ClassArray<ON_SSX_EVENT> events;
    if (ON_Intersect(&surf1, &surf2, events) < 0) {
	bu_log("Intersection failed\n");
	return -1;
    }

    plotsurface(surf1, vbp, 100, 10, PURERED);
    plotsurface(surf2, vbp, 100, 10, BLUE);

    // Plot the intersection curves (or points) (3D and 2D)
    for (int k = 0; k < events.Count(); k++) {
	switch (events[k].m_type) {
	    case ON_SSX_EVENT::ssx_overlap:
	    case ON_SSX_EVENT::ssx_tangent:
	    case ON_SSX_EVENT::ssx_transverse:
		plotcurveonsurface(events[k].m_curveA, &surf1, vbp, 1000, PEACH);
		plotcurveonsurface(events[k].m_curveB, &surf2, vbp, 1000, DARKVIOLET);
		plotcurve(*(events[k].m_curve3d), vbp, 1000, GREEN);
		break;
	    case ON_SSX_EVENT::ssx_tangent_point:
	    case ON_SSX_EVENT::ssx_transverse_point:
		plotpoint(surf1.PointAt(events[k].m_pointA.x, events[k].m_pointB.y), vbp, PEACH);
		plotpoint(surf2.PointAt(events[k].m_pointB.x, events[k].m_pointB.y), vbp, DARKVIOLET);
		plotpoint(events[k].m_point3d, vbp, GREEN);
		break;
	    default:
		break;
	}
    }

    return 0;
}


/** @} */

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
