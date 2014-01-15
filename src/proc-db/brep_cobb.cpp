/*                   B R E P _ C O B B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Based off of code from Ayam:
 *
 * This software is copyrighted (c) 1998-2012 by
 * Randolf Schultz (randolf.schultz@gmail.com).
 * All rights reserved.
 *
 * The author hereby grants permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 *
 * IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 * DERIVATIVES THEREOF, EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
 * IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHOR AND DISTRIBUTORS HAVE
 * NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 * MODIFICATIONS.
 */
/** @file proc-db/brep_cobb.cpp
 *
 * Creates a Cobb NURBS sphere with cube topology, per
 *
 * J.E. Cobb, “Tiling the Sphere with Rational Bezier Patches,”
 *    Tech. Report TR UUCS-88-009, Computer Science Dept.,
 *    Univ. of Utah, Salt Lake City, 1988.
 */

#include "common.h"

#include "bio.h"

/* without OBJ_BREP, this entire procedural example is disabled */
#ifdef OBJ_BREP

#ifdef __cplusplus
extern "C" {
#endif

#include "wdb.h"

#ifdef __cplusplus
}
#endif

ON_BezierSurface *
ON_CobbSphereFace(double rotation_x, double rotation_z)
{
    ON_3dVector rotation_x_axis(1, 0, 0);
    ON_3dVector rotation_z_axis(0, 0, 1);
    ON_3dVector rotation_center(0, 0, 0);
    double rot_x = rotation_x * ON_PI/180.0;
    double rot_z = rotation_z * ON_PI/180.0;

    double t = sqrt(3.0);
    double d = sqrt(2.0);
    ON_4dPoint cv;
    ON_BezierSurface *p1 = new ON_BezierSurface(3, true, 5, 5);
    p1->ReserveCVCapacity(100);

    cv.x = 4.0*(1.0-t);
    cv.y = 4.0*(1.0-t);
    cv.z = 4.0*(1.0-t);
    cv.w = 4.0*(3.0-t);
    p1->SetCV(0,0,cv);


    cv.x = -d;
    cv.y = d * (t - 4.0);
    cv.z = d * (t - 4.0);
    cv.w = d * (3.0*t - 2.0);
    p1->SetCV(0,1,cv);

    cv.x = 0.0;
    cv.y = 4.0*(1.0-2.0*t)/3.0;
    cv.z = 4.0*(1.0-2.0*t)/3.0;
    cv.w = 4.0*(5.0-t)/3.0;
    p1->SetCV(0,2,cv);

    cv.x = d;
    cv.y = d * (t - 4.0);
    cv.z = d * (t - 4.0);
    cv.w = d * (3*t - 2.0);
    p1->SetCV(0,3,cv);

    cv.x = 4.0*(t - 1.0);
    cv.y = 4.0*(1.0-t);
    cv.z = 4.0*(1.0-t);
    cv.w = 4.0*(3.0-t);
    p1->SetCV(0,4,cv);

    cv.x = d*(t - 4.0);
    cv.y = -d;
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(1,0,cv);

    cv.x = (2.0-3.0*t)/2.0;
    cv.y = (2.0-3.0*t)/2.0;
    cv.z = -(t+6.0)/2.0;
    cv.w = (t+6.0)/2.0;
    p1->SetCV(1,1,cv);

    cv.x = 0.0;
    cv.y = d*(2.0*t-7.0)/3.0;
    cv.z = -5.0*sqrt(6.0)/3.0;
    cv.w = d*(t+6.0)/3.0;
    p1->SetCV(1,2,cv);

    cv.x = (3.0*t-2.0)/2.0;
    cv.y = (2.0-3.0*t)/2.0;
    cv.z = -(t+6.0)/2.0;
    cv.w = (t+6.0)/2.0;
    p1->SetCV(1,3,cv);

    cv.x = d*(4.0-t);
    cv.y = -d;
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(1,4,cv);

    cv.x = 4.0*(1.0-2*t)/3.0;
    cv.y = 0.0;
    cv.z = 4.0*(1.0-2*t)/3.0;
    cv.w = 4.0*(5.0-t)/3.0;
    p1->SetCV(2,0,cv);

    cv.x = d*(2.0*t-7.0)/3.0;
    cv.y = 0.0;
    cv.z = -5.0*sqrt(6.0)/3.0;
    cv.w = d*(t+6.0)/3.0;
    p1->SetCV(2,1,cv);

    cv.x = 0.0;
    cv.y = 0.0;
    cv.z = 4.0*(t-5.0)/3.0;
    cv.w = 4.0*(5.0*t-1.0)/9.0;
    p1->SetCV(2,2,cv);

    cv.x = -d*(2*t-7.0)/3.0;
    cv.y = 0.0;
    cv.z = -5.0*sqrt(6.0)/3.0;
    cv.w = d*(t+6.0)/3.0;
    p1->SetCV(2,3,cv);

    cv.x = -4.0*(1.0-2.0*t)/3.0;
    cv.y = 0.0;
    cv.z = 4.0*(1.0-2.0*t)/3.0;
    cv.w = 4.0*(5.0-t)/3.0;
    p1->SetCV(2,4,cv);

    cv.x = d*(t-4.0);
    cv.y = d;
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2);
    p1->SetCV(3,0,cv);

    cv.x = (2.0-3.0*t)/2.0;
    cv.y = -(2.0-3.0*t)/2.0;
    cv.z = -(t+6.0)/2.0;
    cv.w = (t+6.0)/2.0;
    p1->SetCV(3,1,cv);

    cv.x = 0.0;
    cv.y = -d*(2.0*t-7.0)/3.0;
    cv.z = -5.0*sqrt(6.0)/3.0;
    cv.w = d*(t+6.0)/3.0;
    p1->SetCV(3,2,cv);

    cv.x = (3.0*t-2.0)/2.0;
    cv.y = -(2.0-3.0*t)/2.0;
    cv.z = -(t+6.0)/2.0;
    cv.w = (t+6.0)/2.0;
    p1->SetCV(3,3,cv);

    cv.x = d*(4.0-t);
    cv.y = d;
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(3,4,cv);

    cv.x = 4.0*(1.0-t);
    cv.y = -4.0*(1.0-t);
    cv.z = 4.0*(1.0-t);
    cv.w = 4.0*(3.0-t);
    p1->SetCV(4,0,cv);

    cv.x = -d;
    cv.y = -d*(t-4.0);
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(4,1,cv);

    cv.x = 0.0;
    cv.y = -4.0*(1.0-2.0*t)/3.0;
    cv.z = 4.0*(1.0-2.0*t)/3.0;
    cv.w = 4.0*(5.0-t)/3.0;
    p1->SetCV(4,2,cv);

    cv.x = d;
    cv.y = -d*(t-4.0);
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(4,3,cv);

    cv.x = 4.0*(t-1.0);
    cv.y = -4.0*(1.0-t);
    cv.z = 4.0*(1.0-t);
    cv.w = 4.0*(3.0-t);
    p1->SetCV(4,4,cv);

    p1->Rotate(rot_x, rotation_x_axis, rotation_center);
    p1->Rotate(rot_z, rotation_z_axis, rotation_center);

    return p1;
}

/* TODO - Need to find a more compact, efficient way to
 * do this - shouldn't need 24 3d curves... */
ON_Brep *
Cobb_Sphere(double UNUSED(radius), ON_3dPoint *UNUSED(origin)){
    ON_Brep *b = ON_Brep::New();

    // Patch 1 of 6
    ON_BezierSurface *b1 = ON_CobbSphereFace(0, 0);
    ON_NurbsSurface *p1_nurb = ON_NurbsSurface::New();
    b1->GetNurbForm(*p1_nurb);
    b->NewFace(*p1_nurb);

    // Patch 2 of 6
    ON_BezierSurface *b2 = ON_CobbSphereFace(90, 0);
    ON_NurbsSurface *p2_nurb = ON_NurbsSurface::New();
    b2->GetNurbForm(*p2_nurb);
    b->NewFace(*p2_nurb);

    // Patch 3 of 6
    ON_BezierSurface *b3 = ON_CobbSphereFace(180, 0);
    ON_NurbsSurface *p3_nurb = ON_NurbsSurface::New();
    b3->GetNurbForm(*p3_nurb);
    b->NewFace(*p3_nurb);

    // Patch 4 of 6
    ON_BezierSurface *b4 = ON_CobbSphereFace(270, 0);
    ON_NurbsSurface *p4_nurb = ON_NurbsSurface::New();
    b4->GetNurbForm(*p4_nurb);
    b->NewFace(*p4_nurb);

    // Patch 5 of 6
    ON_BezierSurface *b5 = ON_CobbSphereFace(90, 90);
    ON_NurbsSurface *p5_nurb = ON_NurbsSurface::New();
    b5->GetNurbForm(*p5_nurb);
    b->NewFace(*p5_nurb);

    // Patch 6 of 6
    ON_BezierSurface *b6 = ON_CobbSphereFace(90, 270);
    ON_NurbsSurface *p6_nurb = ON_NurbsSurface::New();
    b6->GetNurbForm(*p6_nurb);
    b->NewFace(*p6_nurb);


    b->Standardize();
    b->Compact();

    return b;
}

int
main(int argc, char** argv)
{
    struct rt_wdb* outfp;
    ON_Brep* brep;
    ON_TextLog error_log;
    ON_3dPoint origin(0.0,0.0,0.0);
    const char* id_name = "B-Rep Cobb Sphere";
    const char* geom_name = "cobb.s";

    if (argc > 1 || BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?")) {
	return 0;
    }

    ON::Begin();

    /* export brep to file */
    bu_log("Writing a Cobb unit sphere b-rep...\n");
    outfp = wdb_fopen("brep_cobb.g");
    mk_id(outfp, id_name);


    brep = Cobb_Sphere(1, &origin);
    mk_brep(outfp, geom_name, brep);

    //mk_comb1(outfp, "cube.r", geom_name, 1);
    unsigned char rgb[] = {50, 255, 50};
    mk_region1(outfp, "cobb.r", geom_name, "plastic", "", rgb);

    wdb_close(outfp);
    delete brep;

    ON::End();

    return 0;
}


#else /* !OBJ_BREP */

    int
main(int argc, char *argv[])
{
    bu_log("ERROR: Boundary Representation object support is not available with\n"
	    "       this compilation of BRL-CAD.\n");
    return 1;
}


#endif /* OBJ_BREP */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
