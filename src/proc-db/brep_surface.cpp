/*                B R E P _ S U R F A C E . C P P
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
/** @file proc-db/brep_surface.cpp
 *
 * Create a single plate-mode NURBS surface with a thickness.
 */

#include "common.h"

#include "bio.h"

#include "bu/app.h"
#include "wdb.h"


enum {
    A, B, C, D
};


/*
 * This example is used to generate a single NURBS surface w/o thickness.
 */
ON_Brep *
brep_single_surf(fastf_t thickness = 0.0)
{
    /*
     * Four vertices for the NURBS surface
     *
     *          D ---------- C
     *          |            |
     *          |            |
     *          |            |
     *          A ---------- B
     */

    // control points
    ON_3dPoint point[4] = {
	ON_3dPoint(0.0, 0.0, 11.0), // Point A
	ON_3dPoint(10.0, 0.0, 12.0), // Point B
	ON_3dPoint(10.0, 8.0, 13.0), // Point C
	ON_3dPoint(0.0, 6.0, 12.0), // Point D
    };

    ON_Brep* b = ON_Brep::New();
    ON_TextLog error_log;

    // create vertices
    for (int i = 0; i < 4; i++) {
	ON_BrepVertex& v = b->NewVertex(point[i]);
	v.m_tolerance = 0.0;
    }

    // create 3D lines
    ON_Curve* c3d_ab = new ON_LineCurve(point[A], point[B]);
    c3d_ab->SetDomain(0.0, 1.0);
    b->m_C3.Append(c3d_ab);

    ON_Curve* c3d_bc = new ON_LineCurve(point[B], point[C]);
    c3d_bc->SetDomain(0.0, 1.0);
    b->m_C3.Append(c3d_bc);

    ON_Curve* c3d_cd = new ON_LineCurve(point[C], point[D]);
    c3d_cd->SetDomain(0.0, 1.0);
    b->m_C3.Append(c3d_cd);

    ON_Curve* c3d_da = new ON_LineCurve(point[D], point[A]);
    c3d_da->SetDomain(0.0, 1.0);
    b->m_C3.Append(c3d_da);

    // create edges
    ON_BrepEdge& e_ab = b->NewEdge(b->m_V[A], b->m_V[B], 0);
    e_ab.m_tolerance = 0.0;

    ON_BrepEdge& e_bc = b->NewEdge(b->m_V[B], b->m_V[C], 1);
    e_bc.m_tolerance = 0.0;

    ON_BrepEdge& e_cd = b->NewEdge(b->m_V[C], b->m_V[D], 2);
    e_cd.m_tolerance = 0.0;

    ON_BrepEdge& e_da = b->NewEdge(b->m_V[D], b->m_V[A], 3);
    e_da.m_tolerance = 0.0;

    // create the single surface
    ON_NurbsSurface* s = new ON_NurbsSurface(3, 0, 2, 2, 2, 2);
    s->SetCV(0, 0, point[A]);
    s->SetCV(1, 0, point[B]);
    s->SetCV(1, 1, point[C]);
    s->SetCV(0, 1, point[D]);
    s->SetKnot(0, 0, 0.0);
    s->SetKnot(0, 1, 1.0);
    s->SetKnot(1, 0, 0.0);
    s->SetKnot(1, 1, 1.0);
    b->m_S.Append(s);

    // create the single face
    ON_BrepFace& f = b->NewFace(0);
    ON_BrepLoop& l = b->NewLoop(ON_BrepLoop::outer, f);

    int c2i = 0;
    ON_Surface::ISO iso = ON_Surface::not_iso;

    for (int side = 0; side < 4; side++) {
	ON_2dPoint from, to;
	double u0, u1, v0, v1;
	s->GetDomain(0, &u0, &u1);
	s->GetDomain(1, &v0, &v1);

	switch (side) {
	case 0:
	    from.x = u0; from.y = v0;
	    to.x   = u1; to.y   = v0;
	    iso = ON_Surface::S_iso;
	    break;
	case 1:
	    from.x = u1; from.y = v0;
	    to.x   = u1; to.y   = v1;
	    iso = ON_Surface::E_iso;
	    break;
	case 2:
	    from.x = u1; from.y = v1;
	    to.x   = u0; to.y   = v1;
	    iso = ON_Surface::N_iso;
	    break;
	case 3:
	    from.x = u0; from.y = v1;
	    to.x   = u0; to.y   = v0;
	    iso = ON_Surface::W_iso;
	    break;
	}

	ON_Curve* c2d = new ON_LineCurve(from, to);
	c2d->SetDomain(0.0, 1.0);
	c2i = b->m_C2.Count();
	b->m_C2.Append(c2d);

	ON_BrepTrim& t = b->NewTrim(b->m_E[side], 0, l, c2i);
	t.m_iso = iso;
	t.m_type = ON_BrepTrim::boundary;
	t.m_tolerance[0] = 0.0;
	t.m_tolerance[1] = 0.0;
    }

    /* if brep is a solid model, set the thickness to 0 */
    if (b->IsSolid())
	thickness = 0.0;

    /* assign the value to the whole brep */
    if (!NEAR_ZERO(thickness, 0.001) && b->m_F.Count()) {
	ON_U u;
	u.d = thickness;
	b->m_brep_user = u;

	for (int fi = 0; fi < b->m_F.Count(); fi++)
	    b->m_F[fi].m_face_user = u;
    }

    b->Standardize();
    b->Compact();

    if (b && !b->IsValid(&error_log)) {
	error_log.Print("Invalid single NURBS surface");
	delete b;
	b = NULL;
    }

    return b;
}


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
    const char* id_name = "B-Rep Example";
    const char* file_name = "brep_surface.g";
    const char* regn_name = "brep_surface.r";
    const char* geom_name = "brep_surface.brep";
    fastf_t thickness = 0.0;

    bu_setprogname(argv[0]);

    /* parse the arguments */
    if (argc > 1) {
	if (BU_STR_EQUAL(argv[1], "-H")) {
	    if (argc == 3)
		thickness = (fastf_t)atof(argv[2]);
	    else {
		usage(argv[0]);
		bu_exit(1, "ERROR: unable to parse the arguments");
	    }
	}
    }

    ON::Begin();
    ON_Brep* brep;

    /* export brep to file */

    bu_log("Writing a single surface to [%s]...\n", file_name);
    outfp = wdb_fopen(file_name);
    mk_id(outfp, id_name);

    brep = brep_single_surf(thickness);
    if (!brep)
	bu_exit(1, "ERROR: unable to make the surface\n");

    mk_brep(outfp, geom_name, brep);

    unsigned char rgb[] = {255, 255, 255};
    mk_region1(outfp, regn_name, geom_name, "plastic", "", rgb);

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
