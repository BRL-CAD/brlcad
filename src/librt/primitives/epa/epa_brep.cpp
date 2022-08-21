/*                    E P A _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file epa_brep.cpp
 *
 * Convert an Elliptical Paraboloid to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"

void MatMultiply(mat_t& res, mat_t a, mat_t b) {
    for (int i = 0; i < ELEMENTS_PER_PLANE; i++) {
        for (int j = 0; j < ELEMENTS_PER_PLANE; j++) {
            res[i * ELEMENTS_PER_PLANE + j] = 0;
            for (int v = 0; v < ELEMENTS_PER_PLANE; v++) {
                res[i * ELEMENTS_PER_PLANE + j] += a[i * ELEMENTS_PER_PLANE + v] * b[v * ELEMENTS_PER_PLANE + j];
            }
        }
    }
}

extern "C" void
rt_epa_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *)
{
    struct rt_epa_internal *eip;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_epa_internal *)ip->idb_ptr;
    RT_EPA_CK_MAGIC(eip);

    point_t p1_origin;
    ON_3dPoint plane1_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    //  First, find plane in 3 space corresponding to the bottom face of the EPA.

    vect_t x_dir, y_dir;

    VMOVE(x_dir, eip->epa_Au);
    VCROSS(y_dir, eip->epa_Au, eip->epa_H);
    VUNITIZE(y_dir);

    VMOVE(p1_origin, eip->epa_V);
    plane1_origin = ON_3dPoint(p1_origin);
    plane_x_dir = ON_3dVector(x_dir);
    plane_y_dir = ON_3dVector(y_dir);
    const ON_Plane epa_bottom_plane = ON_Plane(plane1_origin, plane_x_dir, plane_y_dir);

    //  Next, create an ellipse in the plane corresponding to the edge of the epa.

    ON_Ellipse ellipse1 = ON_Ellipse(epa_bottom_plane, eip->epa_r1, eip->epa_r2);
    ON_NurbsCurve ellcurve1;
    ellipse1.GetNurbForm(ellcurve1);
    ellcurve1.SetDomain(0.0, 1.0);

    // Generate the bottom cap
    ON_SimpleArray<ON_Curve*> boundary;
    boundary.Append(ON_Curve::Cast(&ellcurve1));
    ON_PlaneSurface* bp = new ON_PlaneSurface();
    bp->m_plane = epa_bottom_plane;
    bp->SetDomain(0, -100.0, 100.0);
    bp->SetDomain(1, -100.0, 100.0);
    bp->SetExtents(0, bp->Domain(0));
    bp->SetExtents(1, bp->Domain(1));
    (*b)->m_S.Append(bp);
    const int bsi = (*b)->m_S.Count() - 1;
    ON_BrepFace& bface = (*b)->NewFace(bsi);
    (*b)->NewPlanarFaceLoop(bface.m_face_index, ON_BrepLoop::outer, boundary, true);
    const ON_BrepLoop* bloop = (*b)->m_L.Last();
    bp->SetDomain(0, bloop->m_pbox.m_min.x, bloop->m_pbox.m_max.x);
    bp->SetDomain(1, bloop->m_pbox.m_min.y, bloop->m_pbox.m_max.y);
    bp->SetExtents(0, bp->Domain(0));
    bp->SetExtents(1, bp->Domain(1));
    (*b)->SetTrimIsoFlags(bface);

    //  Now, the hard part.  Need an elliptical parabolic NURBS surface

    ON_NurbsSurface* epacurvedsurf = ON_NurbsSurface::New(3, true, 3, 3, 9, 3);
    epacurvedsurf->SetKnot(0, 0, 0);
    epacurvedsurf->SetKnot(0, 1, 0);
    epacurvedsurf->SetKnot(0, 2, 1.571);
    epacurvedsurf->SetKnot(0, 3, 1.571);
    epacurvedsurf->SetKnot(0, 4, 3.142);
    epacurvedsurf->SetKnot(0, 5, 3.142);
    epacurvedsurf->SetKnot(0, 6, 4.713);
    epacurvedsurf->SetKnot(0, 7, 4.713);
    epacurvedsurf->SetKnot(0, 8, 6.284);
    epacurvedsurf->SetKnot(0, 9, 6.284);
    epacurvedsurf->SetKnot(1, 0, 0);
    epacurvedsurf->SetKnot(1, 1, 0);
    epacurvedsurf->SetKnot(1, 2, eip->epa_r1*2);
    epacurvedsurf->SetKnot(1, 3, eip->epa_r1*2);

    double h = MAGNITUDE(eip->epa_H);
    double r1 = eip->epa_r1;
    double r2 = eip->epa_r2;

    ON_4dPoint pt01 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(0, 0, pt01);
    ON_4dPoint pt02 = ON_4dPoint(0, r2/2, h, 1);
    epacurvedsurf->SetCV(0, 1, pt02);
    ON_4dPoint pt03 = ON_4dPoint(0, r2, 0, 1);
    epacurvedsurf->SetCV(0, 2, pt03);

    ON_4dPoint pt04 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(1, 0, pt04);
    ON_4dPoint pt05 = ON_4dPoint(r1/2/sqrt(2.), r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(1, 1, pt05);
    ON_4dPoint pt06 = ON_4dPoint(r1/sqrt(2.), r2/sqrt(2.), 0, 1/sqrt(2.));
    epacurvedsurf->SetCV(1, 2, pt06);

    ON_4dPoint pt07 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(2, 0, pt07);
    ON_4dPoint pt08 = ON_4dPoint(r1/2, 0, h, 1);
    epacurvedsurf->SetCV(2, 1, pt08);
    ON_4dPoint pt09 = ON_4dPoint(r1, 0, 0, 1);
    epacurvedsurf->SetCV(2, 2, pt09);

    ON_4dPoint pt10 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(3, 0, pt10);
    ON_4dPoint pt11 = ON_4dPoint(r1/2/sqrt(2.), -r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(3, 1, pt11);
    ON_4dPoint pt12 = ON_4dPoint(r1/sqrt(2.), -r2/sqrt(2.), 0, 1/sqrt(2.));
    epacurvedsurf->SetCV(3, 2, pt12);

    ON_4dPoint pt13 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(4, 0, pt13);
    ON_4dPoint pt14 = ON_4dPoint(0, -r2/2, h, 1);
    epacurvedsurf->SetCV(4, 1, pt14);
    ON_4dPoint pt15 = ON_4dPoint(0, -r2, 0, 1);
    epacurvedsurf->SetCV(4, 2, pt15);

    ON_4dPoint pt16 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(5, 0, pt16);
    ON_4dPoint pt17 = ON_4dPoint(-r1/2/sqrt(2.), -r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(5, 1, pt17);
    ON_4dPoint pt18 = ON_4dPoint(-r1/sqrt(2.), -r2/sqrt(2.), 0, 1/sqrt(2.));
    epacurvedsurf->SetCV(5, 2, pt18);

    ON_4dPoint pt19 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(6, 0, pt19);
    ON_4dPoint pt20 = ON_4dPoint(-r1/2, 0, h, 1);
    epacurvedsurf->SetCV(6, 1, pt20);
    ON_4dPoint pt21 = ON_4dPoint(-r1, 0, 0, 1);
    epacurvedsurf->SetCV(6, 2, pt21);

    ON_4dPoint pt22 = ON_4dPoint(0, 0, h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(7, 0, pt22);
    ON_4dPoint pt23 = ON_4dPoint(-r1/2/sqrt(2.), r2/2/sqrt(2.), h/sqrt(2.), 1/sqrt(2.));
    epacurvedsurf->SetCV(7, 1, pt23);
    ON_4dPoint pt24 = ON_4dPoint(-r1/sqrt(2.), r2/sqrt(2.), 0, 1/sqrt(2.));
    epacurvedsurf->SetCV(7, 2, pt24);

    ON_4dPoint pt25 = ON_4dPoint(0, 0, h, 1);
    epacurvedsurf->SetCV(8, 0, pt25);
    ON_4dPoint pt26 = ON_4dPoint(0, r2/2, h, 1);
    epacurvedsurf->SetCV(8, 1, pt26);
    ON_4dPoint pt27 = ON_4dPoint(0, r2, 0, 1);
    epacurvedsurf->SetCV(8, 2, pt27);

    vect_t origin_H = { 0,0,1 };
    vect_t origin_Au = { 0,1,0 };
    vect_t end_H;
    vect_t end_Au;
    vect_t axis_1;      // axis of the first rotation
    vect_t axis_2;      // axis of the second rotation
    VMOVE(end_H, eip->epa_H);
    VMOVE(end_Au, eip->epa_Au);
    VUNITIZE(end_H);
    VUNITIZE(end_Au);

    VCROSS(axis_1, origin_H, end_H);
    VUNITIZE(axis_1);

    double cos_a = VDOT(origin_H, end_H);
    double sin_a = sqrt(1 - cos_a);
    /*fastf_t rot1[9] = { cos_a ,-sin_a*axis_1[2], sin_a * axis_1[1],
                       sin_a* axis_1[2], cos_a ,-sin_a* axis_1[0],
                       -sin_a* axis_1[1], sin_a* axis_1[0], cos_a  };*/

    // first rotate matrix: plane normal rotation in 3D
    /*
    reference: https://sites.cs.ucsb.edu/~lingqi/teaching/resources/GAMES101_Lecture_04.pdf page 10
    R(n,a) = cos(a)*I + (1-cos(a))*n*n^t + sin(a)* (0    -nz  ny
                                                    nz   0    -nx
                                                    -ny  nx   0   )
    */
    double cos_a_1 = 1 - cos_a;
    mat_t r_1_matrix= {
            cos_a, -sin_a * axis_1[2], sin_a * axis_1[1], 0,
            sin_a * axis_1[2], cos_a, -sin_a * axis_1[0], 0,
            -sin_a * axis_1[1], sin_a * axis_1[0], cos_a , 0,
            0,0,0,1
    };
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r_1_matrix[i * 4 + j] += cos_a_1 * axis_1[i] * axis_1[j];
        }
    }
    // first rotation matrix caculated

    // get end_Au after first rotation
    MAT3X3VEC(end_Au, r_1_matrix, end_Au);

    // second rotation axis
    VCROSS(axis_2, origin_Au, end_Au);
    VUNITIZE(axis_2);

    cos_a = VDOT(origin_H, end_H);
    sin_a = sqrt(1 - cos_a);

    double cos_a_2 = 1 - cos_a;
    mat_t r_2_matrix = {
            cos_a, -sin_a * axis_1[2], sin_a * axis_1[1], 0,
            sin_a * axis_1[2], cos_a, -sin_a * axis_1[0], 0,
            -sin_a * axis_1[1], sin_a * axis_1[0], cos_a , 0,
            0,0,0,1
    };
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r_2_matrix[i * 4 + j] += cos_a_2 * axis_1[i] * axis_1[j];
        }
    }
    // second rotation matrix caculated

    mat_t trans_matrix; 
    MatMultiply(trans_matrix, r_1_matrix, r_2_matrix);
    trans_matrix[ELEMENTS_PER_PLANE - 1] = plane1_origin.x;
    trans_matrix[ELEMENTS_PER_PLANE * 2 - 1] = plane1_origin.y;
    trans_matrix[ELEMENTS_PER_PLANE * 3 - 1] = plane1_origin.z;
    trans_matrix[ELEMENTS_PER_PLANE * 4 - 1] = 1;

    double tran_mat[ELEMENTS_PER_PLANE][ELEMENTS_PER_PLANE] = { 0 };
    for (int m = 0; m < ELEMENTS_PER_MAT; m++)
    {
        int i = m / ELEMENTS_PER_PLANE;
        int j = m % ELEMENTS_PER_PLANE;
        tran_mat[i][j] = trans_matrix[m];

    }
    /*VMOVE(end_Au, eip->epa_Au);
    double cos_a_1 = 1 - cos_a;
    double r_1_matrix[3][3]{
            {cos_a, -sin_a * axis_1[2], sin_a * axis_1[1]},
            {sin_a * axis_1[2], cos_a, -sin_a * axis_1[0]},
            {-sin_a * axis_1[1], sin_a * axis_1[0], cos_a}
    };
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r_1_matrix[i][j] += cos_a_1 * axis_1[i] * axis_1[j];
        }
    }

    double tran_matrix[4][4]{
            {cos_a, -sin_a * axis_1[2], sin_a * axis_1[1], plane1_origin.x},
            {sin_a * axis_1[2], cos_a, -sin_a * axis_1[0], plane1_origin.y},
            {-sin_a * axis_1[1], sin_a * axis_1[0], cos_a , plane1_origin.z},
            {0,0,0,1}
    };*/

    ON_Xform trans(tran_mat);
    epacurvedsurf->Transform(trans);

    (*b)->m_S.Append(epacurvedsurf);
    int surfindex = (*b)->m_S.Count();
    (*b)->NewFace(surfindex - 1);
    int faceindex = (*b)->m_F.Count();
    (*b)->NewOuterLoop(faceindex-1);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
