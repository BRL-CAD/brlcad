/*                    D S P _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file dsp_brep.cpp
 *
 * Convert a Displacement Map to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"

/* private header */
#include "./dsp.h"

#define MOVEPT(_p) MAT4X3PNT(_p, dsp_ip->dsp_stom, p_temp)
#define MAX_CNT 256

fastf_t C[2*MAX_CNT+1][2*MAX_CNT+1]; // Combination. Assume xcnt & ycnt are not greater than 256

int DegreeReduction(int n, ON_3dPointArray &bezcurv, fastf_t tol, fastf_t &maxerr)
{
    /* This function reduces the degree of a Bezier curve by one.
     * For details about the algorithm, see:
     * Dave Morgan, Degree Reduction of Bezier Curves.
     * http://www.yamnuska.ca/geek/degreeReductionBezier.pdf
     * NOTE: This may cause quite inaccurate results, especially in the middle
     * of the curve when n is big.
     * TODO: Find a better algorithm unless this one is good enough.
     */

    if (tol < 0.0)
	return -1;
    if (bezcurv.Count() != n + 1) {
	bu_log("DegreeReduction(): The Bezier curve should has %d control points!\n", n + 1);
	return -2;
    }
    if (n < 1)
	return -2;

    ON_3dPointArray left(n), right(n), tmpcurve(n);
    left[0] = bezcurv[0];
    for (int i = 1; i < n; i++) {
	left[i] = bezcurv[i] + (double)i / (n - i) * (bezcurv[i] - left[i - 1]);
    }
    right[n - 1] = bezcurv[n];
    for (int i = n - 1; i > 0; i--) {
	right[i - 1] = bezcurv[i] + (double)(n - i) / i * (bezcurv[i] - right[i]);
    }

    fastf_t sum = 0.0;
    for (int i = 0; i < n; i++) {
	ON_3dPoint newpt;
	fastf_t lambda = pow(2.0, 1 - 2*n);
	sum += C[2*n][2*i];
	lambda *= sum;
	if (NEAR_ZERO(lambda, 1e-6))
	    lambda = 0.0;
	if (NEAR_EQUAL(lambda, 1.0, 1e-6))
	    lambda = 1.0;
	newpt = (1 - lambda) * left[i] + lambda * right[i];
	tmpcurve.Append(newpt);
    }
    // Calculate the deviation
    int r = (n - 1) / 2;
    if (n % 2 == 0) {
	ON_3dVector delta;
	delta = bezcurv[r + 1] - (tmpcurve[r] + tmpcurve[r + 1]) / 2;
	maxerr += delta.Length();
    } else {
	ON_3dVector delta;
	delta = left[r] - right[r];
	maxerr += delta.Length();
    }

    if (maxerr > tol)
	return -1;

    bezcurv.Empty();
    bezcurv.Append(tmpcurve.Count(), tmpcurve.Array());
    return 0;
}

extern "C" void
rt_dsp_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_dsp_internal *dsp_ip;

    if (!b)
	return;

    RT_CK_DB_INTERNAL(ip);
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);

    /* A DSP brep is broken down into faces as follows:
     *
     * 1.  The bottom face, a simple planar surface
     *
     * 2.  The four sides, again planar faces but with one edge being
     * trimmed by a curve describing the cross section of the DSP
     * surface with the plane of the face.
     *
     * 3.  The top surface - a generalized Bezier surface with
     * boundaries equal to those of the four side surface intersection
     * curves. Surface topology is deduced as a function of the height
     * values.
     */

    // Step 1 - create the bottom face.

    point_t p_origin, p2, p3;
    point_t p_temp;
    ON_3dPoint plane_origin, plane_x_dir, plane_y_dir, pt2, pt3, pt4;

    VSETALL(p_temp, 0.0);
    MOVEPT(p_origin);
    plane_origin = ON_3dPoint(p_origin);

    VSET(p_temp, dsp_ip->dsp_xcnt-1, 0, 0);
    MOVEPT(p2);
    plane_x_dir = ON_3dPoint(p2);

    VSET(p_temp, 0, dsp_ip->dsp_ycnt-1, 0);
    MOVEPT(p3);
    plane_y_dir = ON_3dPoint(p3);

    ON_Plane bottom_plane(plane_origin, plane_x_dir, plane_y_dir);
    ON_PlaneSurface bottom_surf(bottom_plane);
    bottom_surf.SetDomain(0, 0.0, DIST_PT_PT(p_origin, p2));
    bottom_surf.SetDomain(1, 0.0, DIST_PT_PT(p_origin, p3));
    bottom_surf.SetExtents(0, bottom_surf.Domain(0));
    bottom_surf.SetExtents(1, bottom_surf.Domain(1));
    ON_BrepFace *bottomface = (*b)->NewFace(bottom_surf);
    (*b)->FlipFace(*bottomface);

    // Second step, the "walls"

    ON_SimpleArray<ON_Curve *> boundary;

    // side 1

    point_t s1p1, s1p2, s1p3, s1p4;
    ON_3dPoint s1pt1, s1pt2, s1pt3, s1pt4;
    ON_3dPointArray bezpoints1(256);

    VSET(p_temp, 0, 0, 0);
    MOVEPT(s1p1);
    VSET(p_temp, dsp_ip->dsp_xcnt - 1, 0, 0);
    MOVEPT(s1p2);
    VSET(p_temp, dsp_ip->dsp_xcnt - 1, 0, DSP(dsp_ip, dsp_ip->dsp_xcnt-1, 0));
    MOVEPT(s1p3);
    VSET(p_temp, 0, 0, DSP(dsp_ip, 0 , 0));
    MOVEPT(s1p4);
    s1pt1 = ON_3dPoint(s1p1);
    s1pt2 = ON_3dPoint(s1p2);
    s1pt3 = ON_3dPoint(s1p3);
    s1pt4 = ON_3dPoint(s1p4);
    ON_Plane s1_plane(s1pt1, s1pt2, s1pt4);
    ON_PlaneSurface *s1_surf = new ON_PlaneSurface(s1_plane);
    // Need 3 linear curves and a spline curve
    ON_Curve *s1c1 = new ON_LineCurve(s1pt1, s1pt2);
    boundary.Append(s1c1);
    ON_Curve *s1c2 = new ON_LineCurve(s1pt2, s1pt3);
    boundary.Append(s1c2);
    for (int x=(dsp_ip->dsp_xcnt - 1); x >= 0; x--) {
	point_t p_ctrl;
	VSET(p_temp, x, 0, DSP(dsp_ip, x, 0));
	MOVEPT(p_ctrl);
	ON_3dPoint *ctrlpt = new ON_3dPoint(p_ctrl);
	bezpoints1.Append(*ctrlpt);
    }
    ON_BezierCurve s1_bez3d((const ON_3dPointArray)bezpoints1);
    ON_NurbsCurve* s1_beznurb3d = ON_NurbsCurve::New();
    s1_bez3d.GetNurbForm(*s1_beznurb3d);
    s1_beznurb3d->SetDomain(0.0, 1.0);
    boundary.Append(s1_beznurb3d);
    ON_Curve *s1c4 = new ON_LineCurve(s1pt4, s1pt1);
    boundary.Append(s1c4);
    const int bsi1 = (*b)->AddSurface(s1_surf);
    ON_BrepFace& s1f = (*b)->NewFace(bsi1);
    (*b)->NewPlanarFaceLoop(s1f.m_face_index, ON_BrepLoop::outer, boundary, true);
    const ON_BrepLoop* bloop1 = (*b)->m_L.Last();
    s1_surf->SetDomain(0, bloop1->m_pbox.m_min.x, bloop1->m_pbox.m_max.x);
    s1_surf->SetDomain(1, bloop1->m_pbox.m_min.y, bloop1->m_pbox.m_max.y);
    s1_surf->SetExtents(0, s1_surf->Domain(0));
    s1_surf->SetExtents(1, s1_surf->Domain(1));
    (*b)->SetTrimIsoFlags(s1f);

    // side 2

    point_t s2p1, s2p2, s2p3, s2p4;
    ON_3dPoint s2pt1, s2pt2, s2pt3, s2pt4;
    ON_3dPointArray bezpoints2(256);

    boundary.Empty();
    VSET(p_temp, 0, 0, 0);
    MOVEPT(s2p1);
    VSET(p_temp, 0, dsp_ip->dsp_ycnt - 1, 0);
    MOVEPT(s2p2);
    VSET(p_temp, 0, dsp_ip->dsp_ycnt - 1, DSP(dsp_ip, 0, dsp_ip->dsp_ycnt-1));
    MOVEPT(s2p3);
    VSET(p_temp, 0, 0, DSP(dsp_ip, 0 , 0));
    MOVEPT(s2p4);
    s2pt1 = ON_3dPoint(s2p1);
    s2pt2 = ON_3dPoint(s2p2);
    s2pt3 = ON_3dPoint(s2p3);
    s2pt4 = ON_3dPoint(s2p4);
    ON_Plane s2_plane(s2pt1, s2pt2, s2pt4);
    ON_PlaneSurface *s2_surf = new ON_PlaneSurface(s2_plane);
    // Need 3 linear curves and a spline curve
    ON_Curve *s2c1 = new ON_LineCurve(s2pt1, s2pt2);
    boundary.Append(s2c1);
    ON_Curve *s2c2 = new ON_LineCurve(s2pt2, s2pt3);
    boundary.Append(s2c2);
    for (int y=(dsp_ip->dsp_ycnt - 1); y >= 0; y--) {
	point_t p_ctrl;
	VSET(p_temp, 0, y, DSP(dsp_ip, 0, y));
	MOVEPT(p_ctrl);
	ON_3dPoint ctrlpt(p_ctrl);
	bezpoints2.Append(ctrlpt);
    }
    ON_BezierCurve s2_bez3d((const ON_3dPointArray)bezpoints2);
    ON_NurbsCurve* s2_beznurb3d = ON_NurbsCurve::New();
    s2_bez3d.GetNurbForm(*s2_beznurb3d);
    s2_beznurb3d->SetDomain(0.0, 1.0);
    boundary.Append(s2_beznurb3d);
    ON_Curve *s2c4 = new ON_LineCurve(s2pt4, s2pt1);
    boundary.Append(s2c4);
    const int bsi2 = (*b)->AddSurface(s2_surf);
    ON_BrepFace& s2f = (*b)->NewFace(bsi2);
    (*b)->NewPlanarFaceLoop(s2f.m_face_index, ON_BrepLoop::outer, boundary, true);
    const ON_BrepLoop* bloop2 = (*b)->m_L.Last();
    s2_surf->SetDomain(0, bloop2->m_pbox.m_min.x, bloop2->m_pbox.m_max.x);
    s2_surf->SetDomain(1, bloop2->m_pbox.m_min.y, bloop2->m_pbox.m_max.y);
    s2_surf->SetExtents(0, s2_surf->Domain(0));
    s2_surf->SetExtents(1, s2_surf->Domain(1));
    (*b)->SetTrimIsoFlags(s2f);
    (*b)->FlipFace(s2f);

    // side 3

    point_t s3p1, s3p2, s3p3, s3p4;
    ON_3dPoint s3pt1, s3pt2, s3pt3, s3pt4;
    ON_3dPointArray bezpoints3(256);

    boundary.Empty();
    VSET(p_temp, dsp_ip->dsp_xcnt - 1, dsp_ip->dsp_ycnt - 1, 0);
    MOVEPT(s3p1);
    VSET(p_temp, 0, dsp_ip->dsp_ycnt - 1, 0);
    MOVEPT(s3p2);
    VSET(p_temp, 0, dsp_ip->dsp_ycnt - 1, DSP(dsp_ip, 0, dsp_ip->dsp_ycnt-1));
    MOVEPT(s3p3);
    VSET(p_temp, dsp_ip->dsp_xcnt - 1, dsp_ip->dsp_ycnt - 1, DSP(dsp_ip, dsp_ip->dsp_xcnt-1, dsp_ip->dsp_ycnt-1));
    MOVEPT(s3p4);
    s3pt1 = ON_3dPoint(s3p1);
    s3pt2 = ON_3dPoint(s3p2);
    s3pt3 = ON_3dPoint(s3p3);
    s3pt4 = ON_3dPoint(s3p4);
    ON_Plane s3_plane(s3pt1, s3pt2, s3pt4);
    ON_PlaneSurface *s3_surf = new ON_PlaneSurface(s3_plane);
    // Need 3 linear curves and a spline curve
    ON_Curve *s3c1 = new ON_LineCurve(s3pt1, s3pt2);
    boundary.Append(s3c1);
    ON_Curve *s3c2 = new ON_LineCurve(s3pt2, s3pt3);
    boundary.Append(s3c2);
    for (unsigned int x = 0; x < (dsp_ip->dsp_xcnt); x++) {
	point_t p_ctrl;
	VSET(p_temp, x, dsp_ip->dsp_ycnt - 1, DSP(dsp_ip, x, dsp_ip->dsp_ycnt - 1));
	MOVEPT(p_ctrl);
	ON_3dPoint ctrlpt(p_ctrl);
	bezpoints3.Append(ctrlpt);
    }
    ON_BezierCurve s3_bez3d((const ON_3dPointArray)bezpoints3);
    ON_NurbsCurve* s3_beznurb3d = ON_NurbsCurve::New();
    s3_bez3d.GetNurbForm(*s3_beznurb3d);
    s3_beznurb3d->SetDomain(0.0, 1.0);
    boundary.Append(s3_beznurb3d);
    ON_Curve *s3c4 = new ON_LineCurve(s3pt4, s3pt1);
    boundary.Append(s3c4);
    const int bsi3 = (*b)->AddSurface(s3_surf);
    ON_BrepFace& s3f = (*b)->NewFace(bsi3);
    (*b)->NewPlanarFaceLoop(s3f.m_face_index, ON_BrepLoop::outer, boundary, true);
    const ON_BrepLoop* bloop3 = (*b)->m_L.Last();
    s3_surf->SetDomain(0, bloop3->m_pbox.m_min.x, bloop3->m_pbox.m_max.x);
    s3_surf->SetDomain(1, bloop3->m_pbox.m_min.y, bloop3->m_pbox.m_max.y);
    s3_surf->SetExtents(0, s3_surf->Domain(0));
    s3_surf->SetExtents(1, s3_surf->Domain(1));
    (*b)->SetTrimIsoFlags(s3f);

    // side 4

    point_t s4p1, s4p2, s4p3, s4p4;
    ON_3dPoint s4pt1, s4pt2, s4pt3, s4pt4;
    ON_3dPointArray bezpoints4(256);

    boundary.Empty();
    VSET(p_temp, dsp_ip->dsp_xcnt - 1, dsp_ip->dsp_ycnt - 1, 0);
    MOVEPT(s4p1);
    VSET(p_temp, dsp_ip->dsp_xcnt - 1, 0, 0);
    MOVEPT(s4p2);
    VSET(p_temp, dsp_ip->dsp_xcnt - 1, 0, DSP(dsp_ip, dsp_ip->dsp_xcnt-1, 0));
    MOVEPT(s4p3);
    VSET(p_temp, dsp_ip->dsp_xcnt - 1, dsp_ip->dsp_ycnt - 1, DSP(dsp_ip, dsp_ip->dsp_xcnt-1, dsp_ip->dsp_ycnt-1));
    MOVEPT(s4p4);
    s4pt1 = ON_3dPoint(s4p1);
    s4pt2 = ON_3dPoint(s4p2);
    s4pt3 = ON_3dPoint(s4p3);
    s4pt4 = ON_3dPoint(s4p4);
    ON_Plane s4_plane(s4pt1, s4pt2, s4pt4);
    ON_PlaneSurface *s4_surf = new ON_PlaneSurface(s4_plane);
    // Need 3 linear curves and a spline curve
    ON_Curve *s4c1 = new ON_LineCurve(s4pt1, s4pt2);
    boundary.Append(s4c1);
    ON_Curve *s4c2 = new ON_LineCurve(s4pt2, s4pt3);
    boundary.Append(s4c2);
    for (unsigned int y=0; y < (dsp_ip->dsp_ycnt); y++) {
	point_t p_ctrl;
	VSET(p_temp, dsp_ip->dsp_xcnt - 1, y, DSP(dsp_ip, dsp_ip->dsp_xcnt - 1, y));
	MOVEPT(p_ctrl);
	ON_3dPoint ctrlpt(p_ctrl);
	bezpoints4.Append(ctrlpt);
    }
    ON_BezierCurve s4_bez3d((const ON_3dPointArray)bezpoints4);
    ON_NurbsCurve* s4_beznurb3d = ON_NurbsCurve::New();
    s4_bez3d.GetNurbForm(*s4_beznurb3d);
    s4_beznurb3d->SetDomain(0.0, 1.0);
    boundary.Append(s4_beznurb3d);
    ON_Curve *s4c4 = new ON_LineCurve(s4pt4, s4pt1);
    boundary.Append(s4c4);
    const int bsi4 = (*b)->AddSurface(s4_surf);
    ON_BrepFace& s4f = (*b)->NewFace(bsi4);
    (*b)->NewPlanarFaceLoop(s4f.m_face_index, ON_BrepLoop::outer, boundary, true);
    const ON_BrepLoop* bloop4 = (*b)->m_L.Last();
    s4_surf->SetDomain(0, bloop4->m_pbox.m_min.x, bloop4->m_pbox.m_max.x);
    s4_surf->SetDomain(1, bloop4->m_pbox.m_min.y, bloop4->m_pbox.m_max.y);
    s4_surf->SetExtents(0, s4_surf->Domain(0));
    s4_surf->SetExtents(1, s4_surf->Domain(1));
    (*b)->SetTrimIsoFlags(s4f);
    (*b)->FlipFace(s4f);

    // Next, define the top face with full resolution.

    ON_BezierSurface *bezsurf = new ON_BezierSurface(3, false, dsp_ip->dsp_xcnt, dsp_ip->dsp_ycnt);

    for (unsigned int y=0; y < (dsp_ip->dsp_ycnt); y++) {
	for (unsigned int x=0; x < (dsp_ip->dsp_xcnt); x++) {
	    point_t p_ctrl;
	    VSET(p_temp, x, y, DSP(dsp_ip, x, y));
	    MOVEPT(p_ctrl);
	    ON_3dPoint *ctrlpt = new ON_3dPoint(p_ctrl);
	    bezsurf->SetCV(x, y, *ctrlpt);
	}
    }

    // Reduce the top surface
    fastf_t min = DSP(dsp_ip, 0, 0), max = DSP(dsp_ip, 0, 0);
    for (unsigned int x = 0; x < dsp_ip->dsp_xcnt; x++) {
	for (unsigned int y = 0; y < dsp_ip->dsp_ycnt; y++) {
	    fastf_t data = DSP(dsp_ip, x, y);
	    if (data > max)
		max = data;
	    if (data < min)
		min = data;
	}
    }
    fastf_t max_delta = max - min;
    fastf_t tol = max_delta * 5.0 * dsp_ip->dsp_ycnt;
    fastf_t maxerr = 0.0;
    // Calculate the combinations
    C[0][0] = 1;
    for (int i = 1; i <= MAX_CNT*2; i++) {
	C[i][0] = C[i][i] = 1;
	for (int j = 1; j < i; j++) {
	    C[i][j] = C[i - 1][j - 1] + C[i - 1][j];
	}
    }

    ON_BezierSurface *reducedsurf = new ON_BezierSurface(3, false, dsp_ip->dsp_xcnt, dsp_ip->dsp_ycnt);
    *reducedsurf = *bezsurf;
    unsigned int currentdegree = dsp_ip->dsp_xcnt;
    bool exceedtol = false;
    ON_SimpleArray<ON_3dPointArray *> bezcurvarray(dsp_ip->dsp_ycnt);
    for (unsigned int y = 0; y < dsp_ip->dsp_ycnt; y++) {
	bezcurvarray[(const int)y] = new ON_3dPointArray(dsp_ip->dsp_xcnt);
	for (unsigned int x = 0; x < dsp_ip->dsp_xcnt; x++) {
	    bezcurvarray[(const int)y]->Append(bezsurf->CV(x, y));
	}
    }
    while (currentdegree && !exceedtol) {
	for (unsigned int y = 0; y < dsp_ip->dsp_ycnt; y++) {
	    if (DegreeReduction(currentdegree - 1, *bezcurvarray[(const int)y], tol, maxerr) == -1) {
		exceedtol = true;
		break;
	    }
	}
	if (!exceedtol) {
	    currentdegree--;
	    for (unsigned int y = 0; y < dsp_ip->dsp_ycnt; y++) {
		for (unsigned int x = 0; x <= currentdegree; x++) {
		    reducedsurf->SetCV(x, y, (*bezcurvarray[(const int)y])[(const int)x]);
		}
	    }
	}
    }
    reducedsurf->m_order[0] = currentdegree;

    exceedtol = false;
    tol = max_delta * 5.0 * currentdegree;
    maxerr = 0.0;
    currentdegree = dsp_ip->dsp_ycnt;
    for (int i = 0; i < bezcurvarray.Count(); i++) {
	delete bezcurvarray[i];
    }
    bezcurvarray.Empty();
    bezcurvarray.SetCount(reducedsurf->m_order[0]);
    for (int x = 0; x < reducedsurf->m_order[0]; x++) {
	bezcurvarray[x] = new ON_3dPointArray(dsp_ip->dsp_ycnt);
	for (unsigned int y = 0; y < dsp_ip->dsp_ycnt; y++) {
	    bezcurvarray[x]->Append(reducedsurf->CV(x, y));
	}
    }
    while (currentdegree && !exceedtol) {
	for (int x = 0; x < reducedsurf->m_order[0]; x++) {
	    if (DegreeReduction(currentdegree - 1, *bezcurvarray[x], tol, maxerr) == -1) {
		exceedtol = true;
		break;
	    }
	}
	if (!exceedtol) {
	    currentdegree--;
	    for (int x = 0; x < reducedsurf->m_order[0]; x++) {
		for (unsigned int y = 0; y <= currentdegree; y++) {
		    reducedsurf->SetCV(x, y, (*bezcurvarray[(const int)x])[(const int)y]);
		}
	    }
	}
    }
    reducedsurf->m_order[1] = currentdegree;
    for (int i = 0; i < bezcurvarray.Count(); i++) {
	delete bezcurvarray[i];
    }

    ON_NurbsSurface *tnurbssurf = ON_NurbsSurface::New();
    reducedsurf->GetNurbForm(*tnurbssurf);
    (*b)->NewFace(*tnurbssurf);

    delete bezsurf;
    delete reducedsurf;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
