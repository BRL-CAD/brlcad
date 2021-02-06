/*               B R E P _ I N V A L I D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
/** @file proc-db/brep_invalid.cpp
 *
 * Creating a single trimmed NURBS surface with an invalid trim
 */

#include "common.h"

#include "bio.h"
#include "bu/app.h"
#include "wdb.h"


static void
Cobb_InnerTrimmingEdge(ON_Brep& brep, ON_BrepFace& face)
{
    double scale = 0.25;

    const ON_Surface& srf = *brep.m_S[face.m_si];

    ON_Interval d0 = srf.Domain(0);
    ON_Interval d1 = srf.Domain(1);

    ON_3dPoint p0 = srf.PointAt(d0.ParameterAt(scale), d1.ParameterAt(scale));
    ON_3dPoint p1 = srf.PointAt(d0.ParameterAt(scale), d1.ParameterAt(1-scale));
    ON_3dPoint p2 = srf.PointAt(d0.ParameterAt(1-scale), d1.ParameterAt(1-scale));
    ON_3dPoint p3 = srf.PointAt(d0.ParameterAt(1-scale), d1.ParameterAt(scale));

    ON_BrepVertex v0; v0.point = p0; v0.m_tolerance = 0.0;
    ON_BrepVertex v1; v1.point = p1; v1.m_tolerance = 0.0;
    ON_BrepVertex v2; v2.point = p2; v2.m_tolerance = 0.0;
    ON_BrepVertex v3; v3.point = p3; v3.m_tolerance = 0.0;

    ON_BrepEdge& e0 = brep.NewEdge(v0, v1, 0); e0.m_tolerance = 0.0;
    ON_BrepEdge& e1 = brep.NewEdge(v1, v2, 1); e1.m_tolerance = 0.0;
    ON_BrepEdge& e2 = brep.NewEdge(v2, v3, 2); e2.m_tolerance = 0.0;
    ON_BrepEdge& e3 = brep.NewEdge(v3, v0, 3); e3.m_tolerance = 0.0;
}


static ON_Curve*
Cobb_InnerTrimmingCurve(const ON_Surface& s,
			int side // 0 = near SE to SW
			// 1 = near SW to NW
			// 2 = near NW to NE
			// 3 = near NE to SE
    )
{
    ON_2dPoint from, to;
    double u0, u1, v0, v1, scale = 0.25;

    s.GetDomain(0, &u0, &u1);
    s.GetDomain(1, &v0, &v1);

    double udis = scale * (u1 - u0);
    double vdis = scale * (v1 - v0);

    u0 += udis;
    u1 -= udis;
    v0 += vdis;
    v1 -= vdis;

    switch(side) {
	case 0:  // near SE to SW
	    from.x = u1; from.y = v0;
	    to.x   = u0; to.y   = v0;
	    break;
	case 1: // near SW to NW
	    from.x = u0; from.y = v0;
	    to.x   = u0; to.y   = v1;
	    break;
	case 2: // near NW to NE
	    from.x = u0; from.y = v1;
	    to.x   = u1; to.y   = v1;
	    break;
	case 3: // near NE to SE
	    from.x = u1; from.y = v1;
	    to.x   = u1; to.y   = v0;
	    break;
	default:
	    return 0;
    }

    ON_Curve* c2d = new ON_LineCurve(from, to);
    c2d->SetDomain(0.0, 1.0);

    return c2d;
}


static int
Cobb_InnerTrimmingLoop(ON_Brep& brep, ON_BrepFace& face,
		       int UNUSED(vSWi),
		       int UNUSED(vSEi),
		       int UNUSED(vNEi),
		       int UNUSED(vNWi),
		       int eSi, int eS_dir,
		       int eEi, int eE_dir,
		       int eNi, int eN_dir,
		       int eWi, int eW_dir)
{
    const ON_Surface& srf = *brep.m_S[face.m_si];
    ON_BrepLoop& loop = brep.NewLoop(ON_BrepLoop::inner, face);

    // start near the south side
    ON_Curve* c2 = 0;
    int c2i, ei = 0, bRev3d = 0;
    ON_Surface::ISO iso = ON_Surface::not_iso;

    for (int side = 0; side < 4; side++) {
	c2 = Cobb_InnerTrimmingCurve(srf, side);
	c2i = brep.m_C2.Count();
	brep.m_C2.Append(c2);

	switch(side) {
	    case 0: // near south
		ei = eSi;
		bRev3d = (eS_dir == -1);
		iso = ON_Surface::y_iso;
		break;
	    case 1: // near west
		ei = eEi;
		bRev3d = (eE_dir == -1);
		iso = ON_Surface::x_iso;
		break;
	    case 2: // near north
		ei = eNi;
		bRev3d = (eN_dir == -1);
		iso = ON_Surface::y_iso;
		break;
	    case 3: // near east
		ei = eWi;
		bRev3d = (eW_dir == -1);
		iso = ON_Surface::x_iso;
		break;
	}

	ON_BrepTrim& trim = brep.NewTrim(brep.m_E[ei], bRev3d, loop, c2i);
	trim.m_iso = iso;
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_tolerance[0] = 0.0;
	trim.m_tolerance[1] = 0.0;
    }

    return loop.m_loop_index;
}


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
    p1->SetCV(0, 0, cv);


    cv.x = -d;
    cv.y = d * (t - 4.0);
    cv.z = d * (t - 4.0);
    cv.w = d * (3.0*t - 2.0);
    p1->SetCV(0, 1, cv);

    cv.x = 0.0;
    cv.y = 4.0*(1.0-2.0*t)/3.0;
    cv.z = 4.0*(1.0-2.0*t)/3.0;
    cv.w = 4.0*(5.0-t)/3.0;
    p1->SetCV(0, 2, cv);

    cv.x = d;
    cv.y = d * (t - 4.0);
    cv.z = d * (t - 4.0);
    cv.w = d * (3*t - 2.0);
    p1->SetCV(0, 3, cv);

    cv.x = 4.0*(t - 1.0);
    cv.y = 4.0*(1.0-t);
    cv.z = 4.0*(1.0-t);
    cv.w = 4.0*(3.0-t);
    p1->SetCV(0, 4, cv);

    cv.x = d*(t - 4.0);
    cv.y = -d;
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(1, 0, cv);

    cv.x = (2.0-3.0*t)/2.0;
    cv.y = (2.0-3.0*t)/2.0;
    cv.z = -(t+6.0)/2.0;
    cv.w = (t+6.0)/2.0;
    p1->SetCV(1, 1, cv);

    cv.x = 0.0;
    cv.y = d*(2.0*t-7.0)/3.0;
    cv.z = -5.0*sqrt(6.0)/3.0;
    cv.w = d*(t+6.0)/3.0;
    p1->SetCV(1, 2, cv);

    cv.x = (3.0*t-2.0)/2.0;
    cv.y = (2.0-3.0*t)/2.0;
    cv.z = -(t+6.0)/2.0;
    cv.w = (t+6.0)/2.0;
    p1->SetCV(1, 3, cv);

    cv.x = d*(4.0-t);
    cv.y = -d;
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(1, 4, cv);

    cv.x = 4.0*(1.0-2*t)/3.0;
    cv.y = 0.0;
    cv.z = 4.0*(1.0-2*t)/3.0;
    cv.w = 4.0*(5.0-t)/3.0;
    p1->SetCV(2, 0, cv);

    cv.x = d*(2.0*t-7.0)/3.0;
    cv.y = 0.0;
    cv.z = -5.0*sqrt(6.0)/3.0;
    cv.w = d*(t+6.0)/3.0;
    p1->SetCV(2, 1, cv);

    cv.x = 0.0;
    cv.y = 0.0;
    cv.z = 4.0*(t-5.0)/3.0;
    cv.w = 4.0*(5.0*t-1.0)/9.0;
    p1->SetCV(2, 2, cv);

    cv.x = -d*(2*t-7.0)/3.0;
    cv.y = 0.0;
    cv.z = -5.0*sqrt(6.0)/3.0;
    cv.w = d*(t+6.0)/3.0;
    p1->SetCV(2, 3, cv);

    cv.x = -4.0*(1.0-2.0*t)/3.0;
    cv.y = 0.0;
    cv.z = 4.0*(1.0-2.0*t)/3.0;
    cv.w = 4.0*(5.0-t)/3.0;
    p1->SetCV(2, 4, cv);

    cv.x = d*(t-4.0);
    cv.y = d;
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2);
    p1->SetCV(3, 0, cv);

    cv.x = (2.0-3.0*t)/2.0;
    cv.y = -(2.0-3.0*t)/2.0;
    cv.z = -(t+6.0)/2.0;
    cv.w = (t+6.0)/2.0;
    p1->SetCV(3, 1, cv);

    cv.x = 0.0;
    cv.y = -d*(2.0*t-7.0)/3.0;
    cv.z = -5.0*sqrt(6.0)/3.0;
    cv.w = d*(t+6.0)/3.0;
    p1->SetCV(3, 2, cv);

    cv.x = (3.0*t-2.0)/2.0;
    cv.y = -(2.0-3.0*t)/2.0;
    cv.z = -(t+6.0)/2.0;
    cv.w = (t+6.0)/2.0;
    p1->SetCV(3, 3, cv);

    cv.x = d*(4.0-t);
    cv.y = d;
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(3, 4, cv);

    cv.x = 4.0*(1.0-t);
    cv.y = -4.0*(1.0-t);
    cv.z = 4.0*(1.0-t);
    cv.w = 4.0*(3.0-t);
    p1->SetCV(4, 0, cv);

    cv.x = -d;
    cv.y = -d*(t-4.0);
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(4, 1, cv);

    cv.x = 0.0;
    cv.y = -4.0*(1.0-2.0*t)/3.0;
    cv.z = 4.0*(1.0-2.0*t)/3.0;
    cv.w = 4.0*(5.0-t)/3.0;
    p1->SetCV(4, 2, cv);

    cv.x = d;
    cv.y = -d*(t-4.0);
    cv.z = d*(t-4.0);
    cv.w = d*(3.0*t-2.0);
    p1->SetCV(4, 3, cv);

    cv.x = 4.0*(t-1.0);
    cv.y = -4.0*(1.0-t);
    cv.z = 4.0*(1.0-t);
    cv.w = 4.0*(3.0-t);
    p1->SetCV(4, 4, cv);

    p1->Rotate(rot_x, rotation_x_axis, rotation_center);
    p1->Rotate(rot_z, rotation_z_axis, rotation_center);

    return p1;
}


/* TODO - Need to find a more compact, efficient way to
 * do this - shouldn't need 24 3d curves... */
ON_Brep *
Cobb_Sphere(double UNUSED(radius), ON_3dPoint *UNUSED(origin), fastf_t thickness = 0.0)
{
    ON_Brep *b = ON_Brep::New();

    ON_BezierSurface *b1 = ON_CobbSphereFace(0, 0);
    ON_NurbsSurface *p1_nurb = ON_NurbsSurface::New();
    b1->GetNurbForm(*p1_nurb);
    b->NewFace(*p1_nurb);

    /* create inner loop and trim cause the face defined above already contains outer loop and trim*/
    Cobb_InnerTrimmingEdge(*b, b->m_F[0]);
    Cobb_InnerTrimmingLoop(*b, b->m_F[0], 0, 1, 2, 3,
			   4, 1,
			   5, 1,
			   6, 1,
			   7, 1);

    /* assign thickness value to the whole brep */
    if (!NEAR_ZERO(thickness, 0.001) && b->m_F.Count() && !b->IsSolid()) {
	ON_U u;
	u.d = thickness;
	b->m_brep_user = u;

	//for (int fi = 0; fi < b->m_F.Count(); fi++)
	//    b->m_F[fi].m_face_user = u;
    }

    b->Standardize();
    b->Compact();

    return b;
}


//////////////////////////////////////////////////////////////////////////

static void
usage(const char *argv0)
{
    bu_log("Usage: %s [options]\n\n", argv0);

    bu_log("    -H plate_thickness\n"
	   "\t\tThickness (mm) used when a NURBS is not a closed volume and it's\n"
	   "\t\tconverted as a plate mode NURBS\n");
}


int
main(int argc, char** argv)
{
    struct rt_wdb* outfp;
    ON_Brep* brep;
    ON_TextLog error_log;
    ON_3dPoint origin(0.0, 0.0, 0.0);
    const char* id_name = "B-Rep Surface with Invalid Trim";
    const char* geom_name = "brep_invalid.s";
    fastf_t thickness = 0.0;

    bu_setprogname(argv[0]);

    if (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-H")) {
	    if (argc == 3) {
		thickness = (fastf_t)atof(argv[2]);
	    } else {
		usage(argv[0]);
		bu_exit(1, "ERROR: unable to parse the arguments");
	    }
	}
    }

    ON::Begin();

    /* export brep to file */
    bu_log("Writing a b-rep surface...\n");
    outfp = wdb_fopen("brep_invalid.g");
    mk_id(outfp, id_name);

    brep = Cobb_Sphere(1, &origin, thickness);

    mk_brep(outfp, geom_name, brep);

    unsigned char rgb[] = {50, 255, 50};
    mk_region1(outfp, "brep_invalid.r", geom_name, "plastic", "", rgb);

    wdb_close(outfp);
    delete brep;

    ON::End();

    return 0;
}


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
