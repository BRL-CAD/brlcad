/*                     B R E P _ D E B U G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2009 United States Government as represented by
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

#include <vector>
#include <list>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

#include "vmath.h"

#include "brep.h"
#include "dvec.h"

#include "raytrace.h"
#include "rtgeom.h"

#define fastf_t double

#ifdef __cplusplus
extern "C" {
#endif
    RT_EXPORT BU_EXTERN(int brep_command,
			(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int argc, const char *argv[], char *commtag));
#ifdef __cplusplus
}
#endif

/* FIXME: fugly */
static int hit_count = 0;

//debugging
static int icount = 0;

/********************************************************************************
 * Auxiliary functions
 ********************************************************************************/


using namespace brlcad;

extern void brep_preprocess_trims(ON_BrepFace& face, SurfaceTree* tree);
extern void brep_bvh_subdivide(BBNode* parent, const std::list<SurfaceTree*>& face_trees);

#define PLOTTING 1
#if PLOTTING

#include "plot3.h"

static int pcount = 0;
static FILE* plot = NULL;
static FILE*
plot_file()
{
    if (plot == NULL) {
	plot = fopen("out.pl", "w");
	point_t min, max;
	VSET(min, -2048, -2048, -2048);
	VSET(max, 2048, 2048, 2048);
	pdv_3space(plot, min, max);
    }
    return plot;
}


static FILE*
plot_file(const char *pname)
{
    if (plot != NULL) {
	(void)fclose(plot);
	plot = NULL;
    }
    plot = fopen(pname, "w");
    point_t min, max;
    VSET(min, -2048, -2048, -2048);
    VSET(max, 2048, 2048, 2048);
    pdv_3space(plot, min, max);
    
    return plot;
}


#define BLUEVIOLET 138, 43, 226
#define CADETBLUE 95, 159, 159
#define CORNFLOWERBLUE 66, 66, 111
#define LIGHTBLUE 173, 216, 230
#define DARKGREEN 0, 100, 0
#define KHAKI 189, 183, 107
#define FORESTGREEN 34, 139, 34
#define LIMEGREEN 124, 252, 0
#define PALEGREEN 152, 251, 152
#define DARKORANGE 255, 140, 0
#define DARKSALMON 233, 150, 122
#define LIGHTCORAL 240, 128, 128
#define PEACH 255, 218, 185
#define DEEPPINK 255, 20, 147
#define HOTPINK 255, 105, 180
#define INDIANRED 205, 92, 92
#define DARKVIOLET 148, 0, 211
#define MAROON 139, 28, 98
#define GOLDENROD 218, 165, 32
#define DARKGOLDENROD 184, 134, 11
#define LIGHTGOLDENROD 238, 221, 130
#define DARKYELLOW 155, 155, 52
#define LIGHTYELLOW 255, 255, 224
#define RED 255, 0, 0
#define GREEN 0, 255, 0
#define BLUE 0, 0, 255
#define YELLOW 255, 255, 0
#define MAGENTA 255, 0, 255
#define CYAN 0, 255, 255
#define BLACK 0, 0, 0
#define WHITE 255, 255, 255

#define M_COLOR_PLOT(c) pl_color(plot_file(), c)
#define COLOR_PLOT(r, g, b) pl_color(plot_file(), (r), (g), (b))
#define M_PT_PLOT(p) {				\
	point_t pp, ppp;		        \
	vect_t grow;				\
	VSETALL(grow, 0.01);			\
	VADD2(pp, p, grow);			\
	VSUB2(ppp, p, grow);			\
	pdv_3box(plot_file(), pp, ppp); 	\
    }
#define PT_PLOT(p) {				\
	point_t pp;				\
	VSCALE(pp, p, 1.001);			\
	pdv_3box(plot_file(), p, pp);		\
    }
#define LINE_PLOT(p1, p2) pdv_3move(plot_file(), p1); pdv_3line(plot_file(), p1, p2)
#define BB_PLOT(p1, p2) pdv_3box(plot_file(), p1, p2)

#define ARB_FACE(valp, a, b, c, d)			\
    RT_ADD_VLIST(vhead, valp[a], BN_VLIST_LINE_MOVE);	\
    RT_ADD_VLIST(vhead, valp[b], BN_VLIST_LINE_DRAW);	\
    RT_ADD_VLIST(vhead, valp[c], BN_VLIST_LINE_DRAW);	\
    RT_ADD_VLIST(vhead, valp[d], BN_VLIST_LINE_DRAW);

#define BB_PLOT_VLIST(min, max) {		\
	double pt[8][3];			\
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
    double min[3], max[3];
    list<BBNode*> leaves;
    surf->getLeaves(leaves);

    for (list<BBNode*>::iterator i = leaves.begin(); i != leaves.end(); i++) {
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
	  M_COLOR_PLOT(RED); 
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
	    bb->GetBBox(min, max);
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


void
plotsurfaceleafs(SurfaceTree* surf, struct bn_vlblock *vbp, bool dim3d)
{
    register struct bu_list *vhead;
    double min[3], max[3];
    list<BBNode*> leaves;
    surf->getLeaves(leaves);

    ON_TextLog tl(stderr);
    vhead = rt_vlblock_find(vbp, RED);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);
    vhead = rt_vlblock_find(vbp, BLUE);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);
    vhead = rt_vlblock_find(vbp, MAGENTA);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);

    for (list<BBNode*>::iterator i = leaves.begin(); i != leaves.end(); i++) {
	BBNode* bb = dynamic_cast<BBNode*>(*i);
	if (bb->m_trimmed) {
	    vhead = rt_vlblock_find(vbp, RED);
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
    return;
}


void
plottrimleafs(SurfaceTree* st, struct bn_vlblock *vbp, bool dim3d)
{
    register struct bu_list *vhead;
    double min[3], max[3];
    list<BRNode*> leaves;
    st->ctree->getLeaves(leaves);

    ON_TextLog tl(stderr);
    vhead = rt_vlblock_find(vbp, RED);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);
    vhead = rt_vlblock_find(vbp, BLUE);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);
    vhead = rt_vlblock_find(vbp, MAGENTA);
    RT_ADD_VLIST(vhead, min, BN_VLIST_LINE_MOVE);

    for (list<BRNode*>::iterator i = leaves.begin(); i != leaves.end(); i++) {
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
    double min[3], max[3];
    double u, v;
    ON_2dPoint uv[2];
    int trim1_status;
    int trim2_status;
    double closesttrim1;
    double closesttrim2;

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
	VSET(min, bb->m_u[0]+0.001, bb->m_v[0]+0.001, 0.0);
	VSET(max, bb->m_u[1]-0.001, bb->m_v[1]-0.001, 0.0);
    }
    BB_PLOT(min, max);
	
    M_COLOR_PLOT(YELLOW);
    bool start=true;
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
	    trim1_status = bb->isTrimmed(uv[0], trimBR, closesttrim1);
	    trim2_status = bb->isTrimmed(uv[1], trimBR, closesttrim2);
			
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
    double min[3], max[3];

    if (bb->m_trimmed) {
	COLOR_PLOT(255, 0, 0);
    } else if (bb->m_checkTrim) {
	COLOR_PLOT(0, 0, 255); 
    } else {
	COLOR_PLOT(255, 0, 255); 
    }
	
    if (false) {
	bb->GetBBox(min, max);
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
    double umin, umax;
    double pt1[3], pt2[3];
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
    double umin, umax;
    double pt1[3], pt2[3];
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
plotUVDomain2d(ON_BrepFace &face, struct bn_vlblock *vbp, int plotres)
{
    register struct bu_list *vhead;
    const ON_Surface* surf = face.SurfaceOf();
    double umin, umax, urange;
    double vmin, vmax, vrange;
    double pt1[3], pt2[3];
    ON_2dPoint from, to;

    ON_TextLog tl(stderr);
    vhead = rt_vlblock_find(vbp, RED);

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
    ON_BrepFace *face= trim.Face();
    const ON_Surface* surf = face->SurfaceOf();
    double umin, umax;
    double pt1[3], pt2[3];
    ON_2dPoint from, to;

    ON_TextLog tl(stderr);
    vhead = rt_vlblock_find(vbp, YELLOW);

    surf->GetDomain(0, &umin, &umax);
    const ON_Curve* trimCurve = trim.TrimCurveOf();
    //trimCurve->Dump(tl);

    ON_Interval dom = trimCurve->Domain();
    // XXX todo: dynamically sample the curve
    for (int k = 1; k <= plotres; k++) {
	ON_3dPoint p = trimCurve->PointAt(dom.ParameterAt((double) (k - 1)
							  / (double) plotres));
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

    return;
}


void
plottrimdirection(ON_BrepFace &face, struct bn_vlblock *vbp, int plotres)
{
    register struct bu_list *vhead;
    const ON_Surface* surf = face.SurfaceOf();
    double umin, umax;
    double pt1[3], pt2[3];
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
	    double *knots = new double[knotcnt + 1];

	    trimCurve->GetSpanVector(knots);
	    for (int k = 1; k <= knotcnt; k++) {
		double dist = knots[k] - knots[k-1];
		double step = dist/plotres;
		for (double t=knots[k-1]+step; t<=knots[k]; t=t+step) {
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
plotsurface(ON_Surface &surf, struct bn_vlblock *vbp, int isocurveres, int gridres)
{
    register struct bu_list *vhead;
    double umin, umax;
    double vmin, vmax;
    double pt1[3], pt2[3];
    ON_2dPoint from, to;
	
    vhead = rt_vlblock_find(vbp, PEACH);
	
    ON_Interval udom = surf.Domain(0);
    ON_Interval vdom = surf.Domain(1);

    for (int u = 0; u <= gridres; u++) {
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
    double umin, umax;
    double vmin, vmax;
    double pt1[3], pt2[3];
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
	    VSCALE(pt2, n, 1.5);
	    VADD2(pt2, pt1, pt2);
	    RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
	    RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
	}
    }
    return;
}


void
plotcurve(ON_Curve &curve, struct bn_vlblock *vbp, int plotres)
{
    register struct bu_list *vhead;
    double umin, umax;
    double pt1[3], pt2[3];
    ON_2dPoint from, to;

    vhead = rt_vlblock_find(vbp, YELLOW);

    if (curve.IsLinear()) {
	/*
	  ON_BrepVertex& v1 = face.Brep()->m_V[trim.m_vi[0]];
	  ON_BrepVertex& v2 = face.Brep()->m_V[trim.m_vi[1]];
	  VMOVE(pt1, v1.Point());
	  VMOVE(pt2, v2.Point());
	  LINE_PLOT(pt1, pt2);
	*/

	int knotcnt = curve.SpanCount();
	double *knots = new double[knotcnt + 1];

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
    ON_Brep *brep = bs->brep;
    if ((si >= 0) && (si < brep->m_S.Count())) {
	const ON_Surface* srf = brep->m_S[si];

	if (srf) {
	    ON_wString wonstr;
	    ON_TextLog info_output(wonstr);
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
brep_face_info(struct brep_specific* bs, struct bu_vls *vls, int fi)
{
    ON_wString s;
    ON_TextLog dump(s);
    ON_Brep *brep = bs->brep;

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
brep_facetrim_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int index, int plotres, bool dim3d)
{
    register struct bu_list *vhead;
    struct bn_vlblock *surface_leafs_vbp;
	
    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
        bu_log("brep is NOT valid");
        //return -1;
    }
    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    if (!dim3d)
		plotUVDomain2d(face, vbp, plotres);
	    plottrim(face, vbp, plotres, dim3d);
	}
    } else if (index < brep->m_S.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    face.Dump(tl);
	    if (!dim3d)
		plotUVDomain2d(face, vbp, plotres);
	    plottrim(face, vbp, plotres, dim3d);
	}
    }
    return 0;
}


int
brep_trim_direction_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int index, int plotres)
{
    register struct bu_list *vhead;
    struct bn_vlblock *surface_leafs_vbp;

    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
        bu_log("brep is NOT valid");
        //return -1;
    }
    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    plottrimdirection(face, vbp, plotres);
	}
    } else if (index < brep->m_S.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    face.Dump(tl);
	    plottrimdirection(face, vbp, plotres);
	}
    }
    return 0;
}


int
brep_surface_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int index, int plotres)
{
    register struct bu_list *vhead;
    struct bn_vlblock *surface_leafs_vbp;
	
    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
        bu_log("brep is NOT valid");
        //return -1;
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
	
    return 0;
}


int
brep_surface_normal_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int index, int plotres)
{
    register struct bu_list *vhead;
    struct bn_vlblock *surface_leafs_vbp;

    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
	//return -1;
    }
    if (index == -1) {
	for (index = 0; index < brep->m_S.Count(); index++) {
	    ON_Surface *surf = brep->m_S[index];
	    plotsurfacenormals(*surf, vbp, 10);
	}
    } else if (index < brep->m_S.Count()) {
	ON_Surface *surf = brep->m_S[index];
	surf->Dump(tl);
	plotsurfacenormals(*surf, vbp, 10);
    }

    return 0;
}


int
brep_edge3d_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int index, int plotres)
{
    register struct bu_list *vhead;
    struct bn_vlblock *surface_leafs_vbp;

    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
	//return -1;
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

    return 0;
}


int
brep_trim_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int index, int plotres, bool dim3d)
{
    register struct bu_list *vhead;
    struct bn_vlblock *surface_leafs_vbp;

    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
	bu_log("brep is NOT valid");
	//return -1;
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

    return 0;
}


int
brep_surfaceleafs_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, bool dim3d, int index, int plotres)
{
    register struct bu_list *vhead;
    struct bn_vlblock *surface_leafs_vbp;

    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
        bu_log("brep is NOT valid");
        //return -1;
    }
    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    SurfaceTree* st = new SurfaceTree(&face);
	    plotsurfaceleafs(st, vbp, dim3d);
	}
    } else if (index < brep->m_S.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    SurfaceTree* st = new SurfaceTree(&face);
	    plotsurfaceleafs(st, vbp, dim3d);
	}
    }
    return 0;
}


int
brep_trimleafs_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, bool dim3d, int index, int plotres)
{
    register struct bu_list *vhead;
    struct bn_vlblock *surface_leafs_vbp;

    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
        bu_log("brep is NOT valid");
        //return -1;
    }
    if (index == -1) {
	for (index = 0; index < brep->m_F.Count(); index++) {
	    ON_BrepFace& face = brep->m_F[index];
	    SurfaceTree* st = new SurfaceTree(&face);
	    plottrimleafs(st, vbp, dim3d);
	}
    } else if (index < brep->m_S.Count()) {
	ON_BrepFaceArray& faces = brep->m_F;
	if (index < faces.Count()) {
	    ON_BrepFace& face = faces[index];
	    SurfaceTree* st = new SurfaceTree(&face);
	    plottrimleafs(st, vbp, dim3d);
	}
    }
    return 0;
}


void
info_usage(struct bu_vls *vls)
{
    bu_vls_printf(vls, "mged>brep brepname.s info\n");
    bu_vls_printf(vls, "\tinfo - return count information for specific BREP\n");
    bu_vls_printf(vls, "\tinfo S [index] - return information for specific BREP 'surface'\n");
    bu_vls_printf(vls, "\tinfo F [index] - return information for specific BREP 'face'\n");
}


void
plot_usage(struct bu_vls *vls)
{
    bu_vls_printf(vls, "mged>brep brepname.s plot\n");
    bu_vls_printf(vls, "\tplot - plot entire BREP");
    bu_vls_printf(vls, "\tplot S [index] - plot specific BREP 'surface'\n");
    bu_vls_printf(vls, "\tplot F [index] - plot specific BREP 'face'\n");
}


int
brep_command(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int argc, const char *argv[], char *commtag)
{
    char *command;
    int ret = 0;

    if (argc == 2)
	command = "info";
    else
	command = (char *) argv[2];

    snprintf(commtag, 64, "_BC_"); //default name pre/postfix tag for fake bn_vlblock solid

    if (strcmp(command, "info") == 0) {
	if (strcmp(command, "?") == 0) {
	    info_usage(vls);
	} else if (argc == 3) {
	    ret = brep_info(bs, vls);
	} else if (argc == 5) {
	    const char *part = argv[3];
	    const char *strindex = argv[4];
	    if (strcmp(strindex, "?") == 0) {
		info_usage(vls);
	    } else {
		int index = atoi(strindex);
		if (strcmp(part, "S") == 0) {
		    ret = brep_surface_info(bs, vls, index);
		} else if (strcmp(part, "F") == 0) {
		    ret = brep_face_info(bs, vls, index);
		}
	    }
	}
    } else if (strcmp(command, "plot") == 0) {
	if (strcmp(command, "?") == 0) {
	    plot_usage(vls);
	} else if (argc == 3) {
	    plot_usage(vls);
	} else if (argc >= 4) {
	    const char *part = argv[3];
	    int index = -1;
	    int plotres = 100;
	    if (argc == 6) {
		const char *strres = argv[5];
		plotres = atoi(strres);
	    }
	    if (argc >= 5) {
		const char *strindex = argv[4];
		if (strcmp(strindex, "all") == 0) {
		    index = -1;
		} else {
		    index = atoi(strindex);
		}
	    }
	    if (strcmp(part, "S") == 0) {
		snprintf(commtag, 64, "_BC_S_");
		ret = brep_surface_plot(vls, bs, bi, vbp, index, plotres);
	    } else if (strcmp(part, "SN") == 0) {
		snprintf(commtag, 64, "_BC_SN_");
		ret = brep_surface_normal_plot(vls, bs, bi, vbp, index, plotres);
	    } else if (strcmp(part, "F") == 0) {
		snprintf(commtag, 64, "_BC_F_");
		ret = brep_facetrim_plot(vls, bs, bi, vbp, index, plotres, true);
	    } else if (strcmp(part, "F2d") == 0) {
		snprintf(commtag, 64, "_BC_F2d_");
		ret = brep_facetrim_plot(vls, bs, bi, vbp, index, plotres, false);
	    } else if (strcmp(part, "SBB") == 0) {
		snprintf(commtag, 64, "_BC_SBB_");
		ret = brep_surfaceleafs_plot(vls, bs, bi, vbp, true, index, plotres);
	    } else if (strcmp(part, "SBB2d") == 0) {
		snprintf(commtag, 64, "_BC_SBB2d_");
		ret = brep_surfaceleafs_plot(vls, bs, bi, vbp, false, index, plotres);
	    }  else if (strcmp(part, "TD") == 0) {
		snprintf(commtag, 64, "_BC_TD_");
		ret = brep_trim_direction_plot(vls, bs, bi, vbp, index, plotres);
	    } else if (strcmp(part, "T") == 0) {
		snprintf(commtag, 64, "_BC_T_");
		ret = brep_trim_plot(vls, bs, bi, vbp, index, plotres, true);
	    } else if (strcmp(part, "T2d") == 0) {
		snprintf(commtag, 64, "_BC_T2d_");
		ret = brep_trim_plot(vls, bs, bi, vbp, index, plotres, false);
	    } else if (strcmp(part, "TBB") == 0) {
		snprintf(commtag, 64, "_BC_TBB_");
		ret = brep_trimleafs_plot(vls, bs, bi, vbp, true, index, plotres);
	    } else if (strcmp(part, "TBB2d") == 0) {
		snprintf(commtag, 64, "_BC_TBB2d_");
		ret = brep_trimleafs_plot(vls, bs, bi, vbp, false, index, plotres);
	    } else if (strcmp(part, "E") == 0) {
		snprintf(commtag, 64, "_BC_E_");
		ret = brep_edge3d_plot(vls, bs, bi, vbp, index, plotres);
	    }
	}
    }
    return ret;
}
#endif /* PLOTTING */

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
