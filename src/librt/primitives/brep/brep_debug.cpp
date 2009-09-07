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
/** @addtogroup g_  */
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
#include "vector.h"

#include "raytrace.h"
#include "rtgeom.h"

#define fastf_t double

#ifdef __cplusplus
extern "C" {
#endif
RT_EXPORT BU_EXTERN(int brep_info,
		    (struct brep_specific* bs,struct bu_vls *vls));
RT_EXPORT BU_EXTERN(int brep_face_info,
		    (struct brep_specific* bs,struct bu_vls *vls,int si));
RT_EXPORT BU_EXTERN(int brep_surface_info,
		    (struct brep_specific* bs,struct bu_vls *vls,int si));
RT_EXPORT BU_EXTERN(int brep_surface_plot,
		    (struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp,int index,int plotres));
RT_EXPORT BU_EXTERN(int brep_facetrim_plot,
		    (struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp,int index));
RT_EXPORT BU_EXTERN(int brep_command,
		    (struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp,int argc, char *argv[]));
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
	plot = fopen("out.pl","w");
	point_t min, max;
	VSET(min,-2048,-2048,-2048);
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
	plot = fopen(pname,"w");
	point_t min, max;
	VSET(min,-2048,-2048,-2048);
	VSET(max, 2048, 2048, 2048);
	pdv_3space(plot, min, max);
    
    return plot;
}

#define BLUEVIOLET 138,43,226
#define CADETBLUE 95,159,159
#define CORNFLOWERBLUE 66,66,111
#define LIGHTBLUE 173,216,230
#define DARKGREEN 0,100,0
#define KHAKI 189,183,107
#define FORESTGREEN 34,139,34
#define LIMEGREEN 124,252,0
#define PALEGREEN 152,251,152
#define DARKORANGE 255,140,0
#define DARKSALMON 233,150,122
#define LIGHTCORAL 240,128,128
#define PEACH 255,218,185
#define DEEPPINK 255,20,147
#define HOTPINK 255,105,180
#define INDIANRED 205,92,92
#define DARKVIOLET 148,0,211
#define MAROON 139,28,98
#define GOLDENROD 218,165,32
#define DARKGOLDENROD 184,134,11
#define LIGHTGOLDENROD 238,221,130
#define DARKYELLOW 155,155,52
#define LIGHTYELLOW 255,255,224
#define RED 255,0,0
#define GREEN 0,255,0
#define BLUE 0,0,255
#define YELLOW 255,255,0
#define MAGENTA 255,0,255
#define CYAN 0,255,255
#define BLACK 0,0,0
#define WHITE 255,255,255

#define M_COLOR_PLOT(c) pl_color(plot_file(), c)
#define COLOR_PLOT(r, g, b) pl_color(plot_file(),(r),(g),(b))
#define M_PT_PLOT(p) 	{ 		\
    point_t pp,ppp;		        \
    vect_t grow;                        \
    VSETALL(grow,0.01);                  \
    VADD2(pp, p, grow);                 \
    VSUB2(ppp, p, grow);                \
    pdv_3box(plot_file(), pp, ppp); 	\
}
#define PT_PLOT(p) 	{ 		\
    point_t 	pp; 			\
    VSCALE(pp, p, 1.001); 		\
    pdv_3box(plot_file(), p, pp); 	\
}
#define LINE_PLOT(p1, p2) pdv_3move(plot_file(), p1); pdv_3line(plot_file(), p1, p2)
#define BB_PLOT(p1, p2) pdv_3box(plot_file(), p1, p2)




void plotsurfaceleafs(SurfaceTree* surf) {
    double min[3],max[3];
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
			bb->GetBBox(min,max);
		} else {
		    VSET(min,bb->m_u[0]+0.001,bb->m_v[0]+0.001,0.0);
		    VSET(max,bb->m_u[1]-0.001,bb->m_v[1]-0.001,0.0);
		    //VSET(min,bb->m_u[0],bb->m_v[0],0.0);
		    //VSET(max,bb->m_u[1],bb->m_v[1],0.0);
		}
		BB_PLOT(min,max);
		//}
    }
    return;
}
void plotleaf3d(BBNode* bb) {
    double min[3],max[3];
	double u,v;
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
		bb->GetBBox(min,max);
	} else {
		VSET(min,bb->m_u[0]+0.001,bb->m_v[0]+0.001,0.0);
		VSET(max,bb->m_u[1]-0.001,bb->m_v[1]-0.001,0.0);
	}
	BB_PLOT(min,max);
	
	M_COLOR_PLOT(YELLOW);
	bool start=true;
	point_t a,b;
	ON_3dPoint p;
	BRNode* trimBR = NULL;
	const ON_BrepFace* f = bb->m_face;
	const ON_Surface* surf = f->SurfaceOf();
	fastf_t uinc = (bb->m_u[1] - bb->m_u[0])/100.0;
	fastf_t vinc = (bb->m_v[1] - bb->m_v[0])/100.0;
	for(u=bb->m_u[0];u<bb->m_u[1];u=u+uinc) {
		for(v=bb->m_v[0];v<bb->m_v[1];v=v+vinc) {
			uv[0].x = u;
			uv[0].y = v;
			uv[1].x = u+uinc;
			uv[1].y = v+vinc;
			trim1_status = bb->isTrimmed(uv[0],trimBR,closesttrim1);
			trim2_status = bb->isTrimmed(uv[1],trimBR,closesttrim2);
			
			if (((trim1_status != 1) || (fabs(closesttrim1) < BREP_EDGE_MISS_TOLERANCE)) &&
				((trim2_status != 1) || (fabs(closesttrim2) < BREP_EDGE_MISS_TOLERANCE))) {
				p = surf->PointAt(uv[0].x,uv[0].y);
				VMOVE(a,p);
				p = surf->PointAt(uv[1].x,uv[1].y);
				VMOVE(b,p);
				LINE_PLOT(a,b);
			}
		}
	}
	
    return;
}
void plotleafuv(BBNode* bb) {
    double min[3],max[3];

	if (bb->m_trimmed) {
		COLOR_PLOT(255, 0, 0);
	} else if (bb->m_checkTrim) {
		COLOR_PLOT(0, 0, 255); 
	} else {
		COLOR_PLOT(255, 0, 255); 
	}
	
	if (false) {
		bb->GetBBox(min,max);
	} else {
		VSET(min,bb->m_u[0]+0.001,bb->m_v[0]+0.001,0.0);
		VSET(max,bb->m_u[1]-0.001,bb->m_v[1]-0.001,0.0);
	}
	BB_PLOT(min,max);
	
    return;
}

void plottrim(ON_BrepFace &face, struct bn_vlblock *vbp) {
	register struct bu_list *vhead;
    const ON_Surface* surf = face.SurfaceOf();
    double umin, umax;
    double pt1[3],pt2[3];
    ON_2dPoint from, to;
	
    vhead = rt_vlblock_find(vbp,DEEPPINK);
	
    surf->GetDomain(0, &umin, &umax);
    for (int i = 0; i < face.LoopCount(); i++) {
		ON_BrepLoop* loop = face.Loop(i);
		// for each trim
		for (int j = 0; j < loop->m_ti.Count(); j++) {
			ON_BrepTrim& trim = face.Brep()->m_T[loop->m_ti[j]];
			const ON_Curve* trimCurve = trim.TrimCurveOf();
			
			if (trimCurve->IsLinear()) {
				/*
				 ON_BrepVertex& v1 = face.Brep()->m_V[trim.m_vi[0]];
				 ON_BrepVertex& v2 = face.Brep()->m_V[trim.m_vi[1]];
				 VMOVE(pt1, v1.Point());
				 VMOVE(pt2, v2.Point());
				 LINE_PLOT(pt1,pt2);
				 */
				
				int knotcnt = trimCurve->SpanCount();
				double *knots = new double[knotcnt+1];
				
				trimCurve->GetSpanVector(knots);
				for(int i=1;i<=knotcnt;i++) {
					ON_3dPoint p = trimCurve->PointAt(knots[i-1]);
		    p = surf->PointAt(p.x,p.y);
					VMOVE(pt1, p);
					p = trimCurve->PointAt(knots[i]);
		    p = surf->PointAt(p.x,p.y);
					VMOVE(pt2, p);
					RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
					RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
				}
				
			} else {
				ON_Interval dom = trimCurve->Domain();
				// XXX todo: dynamically sample the curve
				for (int i = 1; i <= 10000; i++) {
					ON_3dPoint p = trimCurve->PointAt(dom.ParameterAt((double)(i-1)/10000.0));
		    p = surf->PointAt(p.x,p.y);
					VMOVE(pt1, p);
					p = trimCurve->PointAt(dom.ParameterAt((double)i/10000.0));
		    p = surf->PointAt(p.x,p.y);
					VMOVE(pt2, p);
					RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
					RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
				}
			}
			
		}
    }
    return;
}

void plotsurface(ON_Surface &surf, struct bn_vlblock *vbp,int isocurveres, int gridres) {
	register struct bu_list *vhead;
    double umin, umax;
    double vmin, vmax;
    double pt1[3],pt2[3];
    ON_2dPoint from, to;
	
    vhead = rt_vlblock_find(vbp,YELLOW);
	
	ON_Interval udom = surf.Domain(0);
	ON_Interval vdom = surf.Domain(1);

    for (int u = 0; u <= gridres; u++) {
		for (int v = 1; v <= isocurveres; v++) {
			ON_3dPoint p = surf.PointAt(udom.ParameterAt((double)u/(double)gridres),vdom.ParameterAt((double)(v-1)/(double)isocurveres));
			VMOVE(pt1, p);
			p = surf.PointAt(udom.ParameterAt((double)u/(double)gridres),vdom.ParameterAt((double)v/(double)isocurveres));
			VMOVE(pt2, p);
			RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
			RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	}

	for (int v = 0; v <= gridres; v++) {
		for (int u = 1; u <= isocurveres; u++) {
			ON_3dPoint p = surf.PointAt(udom.ParameterAt((double)(u-1)/(double)isocurveres),vdom.ParameterAt((double)v/(double)gridres));
			VMOVE(pt1, p);
			p = surf.PointAt(udom.ParameterAt((double)u/(double)isocurveres),vdom.ParameterAt((double)v/(double)gridres));
			VMOVE(pt2, p);
			RT_ADD_VLIST(vhead, pt1, BN_VLIST_LINE_MOVE);
			RT_ADD_VLIST(vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	}
    return;
}

void plotcurve(ON_Curve &curve, struct bn_vlblock *vbp, int plotres) {
	register struct bu_list *vhead;
    double umin, umax;
    double pt1[3],pt2[3];
    ON_2dPoint from, to;

    vhead = rt_vlblock_find(vbp, RED);

	if (curve.IsLinear()) {
		/*
		 ON_BrepVertex& v1 = face.Brep()->m_V[trim.m_vi[0]];
		 ON_BrepVertex& v2 = face.Brep()->m_V[trim.m_vi[1]];
		 VMOVE(pt1, v1.Point());
		 VMOVE(pt2, v2.Point());
		 LINE_PLOT(pt1,pt2);
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

void plottrim(const ON_Curve &curve, double from, double to ) {
    point_t pt1,pt2;
    // XXX todo: dynamically sample the curve
    for (int i = 0; i <= 10000; i++) {
	ON_3dPoint p = curve.PointAt(from + (to-from)*(double)i/10000.0);//dom.ParameterAt((double)i/10000.0));
	VMOVE(pt2, p);
	if (i != 0) {
	    LINE_PLOT(pt1,pt2);
	} 
	VMOVE(pt1,p);
    }
}
void plottrim(ON_Curve &curve ) {
    point_t pt1,pt2;
    // XXX todo: dynamically sample the curve
    ON_Interval dom = curve.Domain();
    for (int i = 0; i <= 10000; i++) {
	ON_3dPoint p = curve.PointAt(dom.ParameterAt((double)i/10000.0));
	VMOVE(pt2, p);
	if (i != 0) {
	    LINE_PLOT(pt1,pt2);
	} 
	VMOVE(pt1,p);
    }
}

int
brep_info(struct brep_specific* bs,struct bu_vls *vls)
{
	ON_Brep *brep = bs->brep;
	bu_vls_printf(vls, "surfaces:  %d\n",brep->m_S.Count());
	bu_vls_printf(vls, "3d curve:  %d\n",brep->m_C3.Count());
	bu_vls_printf(vls, "2d curves: %d\n",brep->m_C2.Count());
	bu_vls_printf(vls, "vertices:  %d\n",brep->m_V.Count());
	bu_vls_printf(vls, "edges:     %d\n",brep->m_E.Count());
	bu_vls_printf(vls, "trims:     %d\n",brep->m_T.Count());
	bu_vls_printf(vls, "loops:     %d\n",brep->m_L.Count());
	bu_vls_printf(vls, "faces:     %d\n",brep->m_F.Count());
	
	return 0;
}

int
brep_surface_info(struct brep_specific* bs,struct bu_vls *vls,int si)
{
	ON_Brep *brep = bs->brep;
	if ((si >= 0) && (si < brep->m_S.Count()))	{
		const ON_Surface* srf = brep->m_S[si];

		if ( srf )
		{
			ON_Interval udom = srf->Domain(0);
			ON_Interval vdom = srf->Domain(1);
			const char* s = srf->ClassId()->ClassName();
			if ( !s )
				s = "";
			bu_vls_printf(vls,"surface[%2d]: %s u(%g,%g) v(%g,%g)\n",
					   si, s, 
					   udom[0], udom[1], 
					   vdom[0], vdom[1]
					   );
		}
		else
		{
			bu_vls_printf(vls, "surface[%2d]: NULL\n",si);
		}
	}

	return 0;
}


int
brep_face_info(struct brep_specific* bs,struct bu_vls *vls,int si)
{
	ON_Brep *brep = bs->brep;
	if ((si >= 0) && (si < brep->m_S.Count()))	{
		const ON_Surface* srf = brep->m_S[si];

		if ( srf )
		{
			ON_Interval udom = srf->Domain(0);
			ON_Interval vdom = srf->Domain(1);
			const char* s = srf->ClassId()->ClassName();
			if ( !s )
				s = "";
			bu_vls_printf(vls,"surface[%2d]: %s u(%g,%g) v(%g,%g)\n",
					   si, s, 
					   udom[0], udom[1], 
					   vdom[0], vdom[1]
					   );
		}
		else
		{
			bu_vls_printf(vls, "surface[%2d]: NULL\n",si);
		}
	}

	return 0;
}

int
brep_facetrim_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp,int index)
{
	register struct bu_list *vhead;
	struct bn_vlblock        *surface_leafs_vbp;
	
    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
        bu_log("brep is NOT valid");
        return -1;
    }
    ON_BrepFaceArray& faces = brep->m_F;
   if (index < faces.Count()) {
        ON_BrepFace& face = faces[index];			
		plottrim(face,vbp);
    }
    return 0;
}

int
brep_surface_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp,int index,int plotres)
{
	register struct bu_list *vhead;
	struct bn_vlblock        *surface_leafs_vbp;
	
    ON_TextLog tl(stderr);
    ON_Brep* brep = bs->brep;
    if (brep == NULL || !brep->IsValid(&tl)) {
        bu_log("brep is NOT valid");
        return -1;
    }
    if ( index == -1) {
	for (index = 0; index < brep->m_S.Count(); index++) {
	    ON_Surface *surf = brep->m_S[index];
	    plotsurface(*surf,vbp,plotres,10);
	}
    } else if (index < brep->m_S.Count()) {
	ON_Surface *surf = brep->m_S[index];
/*
	ON_4dPoint pnt;
	ON_NurbsSurface test;

	if ( surf->GetNurbForm(test) > 0) {
	    int u_size = test.m_cv_count[0];
	    int v_size = test.m_cv_count[1];
	    cout << "u,v - " << u_size << "," << v_size << endl;
	    for (int iu = 0; iu < u_size; iu++) {
		for (int iv = 0; iv < v_size; iv++) {
			test.GetCV(iu,iv,pnt);
			cout << "("<< pnt.x << "," << pnt.y << "," << pnt.z << "," << pnt.w << ")" << endl;
		}
	    }
	double *knots = new double [test.SpanCount(0)+1];
	if ( test.GetSpanVector( 0, knots ) ) {
		cout << "knots 0 - (";
		for(int i=0; i < test.SpanCount(0)+1; i++ ) {
			if (i > 0) cout << ",";
			cout << knots[i];
		}
		cout << ")" << endl;
	}
	knots = new double [test.SpanCount(1)+1];
	if ( test.GetSpanVector( 1, knots ) ) {
		cout << "knots 1 - (";
		for(int i=0; i < test.SpanCount(1)+1; i++ ) {
			if (i > 0) cout << ",";
			cout << knots[i];
		}
		cout << ")" << endl;
	}

	}
*/
	plotsurface(*surf,vbp,plotres,10);
    }
	
    return 0;
}

int
brep_edge3d_plot(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp,int index,int plotres)
{
	register struct bu_list *vhead;
	struct bn_vlblock *surface_leafs_vbp;

	ON_TextLog tl(stderr);
	ON_Brep* brep = bs->brep;
	if (brep == NULL || !brep->IsValid(&tl)) {
		bu_log("brep is NOT valid");
		return -1;
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
brep_command(struct bu_vls *vls, struct brep_specific* bs, struct rt_brep_internal* bi,struct bn_vlblock *vbp,int argc, char *argv[])
{
    char *command;
    int  ret = 0;

	if (argc == 2)
		command = "info";
	else
		command = (char *) argv[2];

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
			if (strcmp(part, "S") == 0) {
				if (argc == 6) {
					const char *strres = argv[5];
					plotres = atoi(strres);
				}
				if (argc >= 5) {
					const char *strindex = argv[4];
					index = atoi(strindex);
					bu_vls_printf(vls, "strindex-%s", strindex);
					ret = brep_surface_plot(vls, bs, bi, vbp, index, plotres);
				} else {
					ret = brep_surface_plot(vls, bs, bi, vbp, index, plotres);
				}
			} else if (strcmp(part, "F") == 0) {
				ret = brep_facetrim_plot(vls, bs, bi, vbp, index);
			} else if (strcmp(part, "E") == 0) {
				if (argc == 6) {
					const char *strres = argv[5];
					plotres = atoi(strres);
				}
				if (argc >= 5) {
					const char *strindex = argv[4];
					index = atoi(strindex);
					bu_vls_printf(vls, "strindex-%s", strindex);
					ret = brep_edge3d_plot(vls, bs, bi, vbp, index, plotres);
				} else {
					ret = brep_edge3d_plot(vls, bs, bi, vbp, index, plotres);
				}
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
