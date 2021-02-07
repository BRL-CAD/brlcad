/*              B R E P _ T R I M M E D . C P P
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
/** @file proc-db/brep_trimmed.cpp
 *
 * Generate a single B-rep surface with an interior trim.
 */

#include "common.h"

#include "bu/app.h"
#include "wdb.h"


enum {
    A = 0,
    B = 1,
    C = 2,
    D = 3,
    E = 4,
    F = 5,
    G = 6,
    H_ = 7
};


enum {
    AB =  0,
    BC =  1,
    CD =  2,
    AD =  3,
    EF =  4,
    FG =  5,
    GH =  6,
    EH =  7
};


enum {
    ABCD =  0
};


static ON_Curve* CreateLinearCurve(
    const ON_3dPoint& from,
    const ON_3dPoint& to
    )
{
    /* creates a 3d line segment to be used as a 3d curve in a ON_Brep */
    ON_Curve* c3d = new ON_LineCurve(from, to);
    c3d->SetDomain(0.0, 10.0);
    return c3d;
}


static ON_Surface* CreateNurbsSurface(
    const ON_3dPoint& SW,
    const ON_3dPoint& SE,
    const ON_3dPoint& NE,
    const ON_3dPoint& NW
    )
{
    ON_NurbsSurface* pNurbsSurface = new ON_NurbsSurface(
	3,     /* dimension (>= 1) */
	false, /* not rational */
	2,     /* "u" order (>= 2) */
	2,     /* "v" order (>= 2) */
	2,     /* number of control vertices in "u" dir (>= order) */
	2      /* number of control vertices in "v" dir (>= order) */
	);
    /* corner CVs in counter clockwise order starting in the south west */
    pNurbsSurface->SetCV(0, 0, SW);
    pNurbsSurface->SetCV(1, 0, SE);
    pNurbsSurface->SetCV(1, 1, NE);
    pNurbsSurface->SetCV(0, 1, NW);
    /* "u" knots */
    pNurbsSurface->SetKnot(0, 0, 0.0);
    pNurbsSurface->SetKnot(0, 1, 1.0);
    /* "v" knots */
    pNurbsSurface->SetKnot(1, 0, 0.0);
    pNurbsSurface->SetKnot(1, 1, 1.0);

    return pNurbsSurface;
}


static void CreateOneEdge(
    ON_Brep& brep,
    int vi0, /* index of start vertex */
    int vi1, /* index of end vertex */
    int c3i  /* index of 3d curve */
    )
{
    ON_BrepVertex& v0 = brep.m_V[vi0];
    ON_BrepVertex& v1 = brep.m_V[vi1];
    ON_BrepEdge& edge = brep.NewEdge(v0, v1, c3i);
    edge.m_tolerance = 0.0;  /* this simple example is exact - for *
			      * models with non-exact data, set
			      * tolerance as explained in definition
			      * of ON_BrepEdge.
			      */
}


static void CreateEdges(ON_Brep& brep)
{
    /* In this simple example, the edge indices exactly match the 3d
     * curve indices.  In general, the correspondence between edge and
     * curve indices can be arbitrary.  It is permitted for multiple
     * edges to use different portions of the same 3d curve.  The
     * orientation of the edge always agrees with the natural
     * parametric orientation of the curve.
     */

    /* outer edges */
    CreateOneEdge(brep, A, B, AB);
    CreateOneEdge(brep, B, C, BC);
    CreateOneEdge(brep, C, D, CD);
    CreateOneEdge(brep, A, D, AD);

    /* Inner Edges */
    CreateOneEdge(brep, E, F, EF);
    CreateOneEdge(brep, F, G, FG);
    CreateOneEdge(brep, G, H_, GH);
    CreateOneEdge(brep, E, H_, EH);
}


static ON_Curve* CreateOuterTrimmingCurve(
    const ON_Surface& s,
    int side /* 0 = SW to SE */
    /* 1 = SE to NE */
    /* 2 = NE to NW */
    /* 3 = NW to SW */
    )
{
    /* A trimming curve is a 2d curve whose image lies in the
     * surface's domain.  The "active" portion of the surface is to
     * the left of the trimming curve.  An outer trimming loop
     * consists of a simple closed curve running counter-clockwise
     * around the region it trims.
     */

    /* In cases when trim curve is not easily defined in surface
     * domain, use ON_Surface::Pullback only be careful about curve
     * direction to ensure loop trims run anti-clockwise for outer
     * loop and clockwise for inner loop.
     */

    /* In this case, trim curves lie on the four edges of the
     * surface.
     */
    ON_2dPoint from, to;
    double u0, u1, v0, v1;

    s.GetDomain(0, &u0, &u1);
    s.GetDomain(1, &v0, &v1);

    switch (side) {
	case 0:  /* SW to SE */
	    from.x = u0; from.y = v0;
	    to.x   = u1; to.y   = v0;
	    break;
	case 1: /* SE to NE */
	    from.x = u1; from.y = v0;
	    to.x   = u1; to.y   = v1;
	    break;
	case 2: /* NE to NW */
	    from.x = u1; from.y = v1;
	    to.x   = u0; to.y   = v1;
	    break;
	case 3: /* NW to SW */
	    from.x = u0; from.y = v1;
	    to.x   = u0; to.y   = v0;
	    break;
	default:
	    return 0;
    }

    ON_Curve* c2d = new ON_LineCurve(from, to);
    c2d->SetDomain(0.0, 1.0);

    return c2d;
}


static int MakeOuterTrimmingLoop(
    ON_Brep& brep, /* returns index of loop */
    ON_BrepFace& face,  /* face loop is on */
    int UNUSED(vSWi),
    int UNUSED(vSEi),
    int UNUSED(vNEi),
    int UNUSED(vNWi), /* Indices of corner vertices listed in SW, SE,
		       * NW, NE order
		       */
    int eSi,     /* index of edge on south side of surface */
    int eS_dir,  /* orientation of edge with respect to surface trim */
    int eEi,     /* index of edge on south side of surface */
    int eE_dir,  /* orientation of edge with respect to surface trim */
    int eNi,     /* index of edge on south side of surface */
    int eN_dir,  /* orientation of edge with respect to surface trim */
    int eWi,     /* index of edge on south side of surface */
    int eW_dir   /* orientation of edge with respect to surface trim */
    )
{
    const ON_Surface& srf = *brep.m_S[face.m_si];

    /* Create new loop */
    ON_BrepLoop& loop = brep.NewLoop(ON_BrepLoop::outer, face);

    /* Create trimming curves running counter clockwise around the
     * surface's domain.  Note that trims of outer loops run counter
     * clockwise while trims of inner loops (holes) run
     * anti-clockwise.  Also note that when trims locate on surface N,
     * S, E or W ends, then trim_iso becomes N_iso, S_iso, E_iso and
     * W_iso respectfully.  While if trim is parallel to surface N, S
     * or E, W, then trim is becomes y_iso and x_iso respectfully.
     */

    /* Start at the south side */
    ON_Curve* c2 = 0;
    int c2i, ei=0, bRev3d=0;
    ON_Surface::ISO iso = ON_Surface::not_iso;

    for (int side = 0; side < 4; side++) {
	/* side: 0=south, 1=east, 2=north, 3=west */

	c2 = CreateOuterTrimmingCurve(srf, side);

	/* Add trimming curve to brep trimming curves array */
	c2i = brep.m_C2.Count();
	brep.m_C2.Append(c2);

	switch (side) {
	    case 0: /* south */
		ei = eSi;
		bRev3d = (eS_dir == -1);
		iso = ON_Surface::S_iso;
		break;
	    case 1: /* east */
		ei = eEi;
		bRev3d = (eE_dir == -1);
		iso = ON_Surface::E_iso;
		break;
	    case 2: /* north */
		ei = eNi;
		bRev3d = (eN_dir == -1);
		iso = ON_Surface::N_iso;
		break;
	    case 3: /* west */
		ei = eWi;
		bRev3d = (eW_dir == -1);
		iso = ON_Surface::W_iso;
		break;
	}

	/* Create new trim topology that references edge, direction
	 * relative to edge, loop and trim curve geometry
	 */
	ON_BrepTrim& trim = brep.NewTrim(brep.m_E[ei], bRev3d, loop, c2i);
	trim.m_iso = iso;
	trim.m_type = ON_BrepTrim::boundary; /* This one b-rep face,
					      * so all trims are
					      * boundary ones.
					      */
	trim.m_tolerance[0] = 0.0; /* This simple example is exact -
				    * for models with non-exact
				    */
	trim.m_tolerance[1] = 0.0; /* data, set tolerance as explained
				    * in definition of ON_BrepTrim.
				    */
    }

    return loop.m_loop_index;
}


/* Trim curves must run is clockwise direction */
static ON_Curve* CreateInnerTrimmingCurve(
    const ON_Surface& s,
    int side
    /* 0 = near SE to SW */
    /* 1 = near SW to NW */
    /* 2 = near NW to NE */
    /* 3 = near NE to SE */
    )
{
    /* A trimming curve is a 2d curve whose image lies in the
     * surface's domain.  The "active" portion of the surface is to
     * the left of the trimming curve.  An inner trimming loop
     * consists of a simple closed curve running clockwise around the
     * region the hole.
     */

    /* In this case, trim curves lie with 0.2 domain distance from
     * surface edge
     */
    ON_2dPoint from, to;
    double u0, u1, v0, v1;

    s.GetDomain(0, &u0, &u1);
    s.GetDomain(1, &v0, &v1);

    double udis = 0.2*(u1-u0);
    double vdis = 0.2*(v1-v0);

    u0 += udis;
    u1-= udis;
    v0 += vdis;
    v1 -= vdis;

    switch(side) {
	case 0:  /* near SE to SW */
	    from.x = u1; from.y = v0;
	    to.x   = u0; to.y   = v0;
	    break;
	case 1: /* near SW to NW */
	    from.x = u0; from.y = v0;
	    to.x   = u0; to.y   = v1;
	    break;
	case 2: /* near NW to NE */
	    from.x = u0; from.y = v1;
	    to.x   = u1; to.y   = v1;
	    break;
	case 3: /* near NE to SE */
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


static int MakeInnerTrimmingLoop(
    ON_Brep& brep, /* returns index of loop */
    ON_BrepFace& face,  /* face loop is on */
    int UNUSED(vSWi),
    int UNUSED(vSEi),
    int UNUSED(vNEi),
    int UNUSED(vNWi), /* Indices of corner hole vertices listed in SW,
		       * SE, NW, NE order
		       */
    int eSi,     /* index of edge close to south side of surface */
    int eS_dir,  /* orientation of edge with respect to surface trim */
    int eEi,     /* index of edge close to east side of surface */
    int eE_dir,  /* orientation of edge with respect to surface trim */
    int eNi,     /* index of edge close to north side of surface */
    int eN_dir,  /* orientation of edge with respect to surface trim */
    int eWi,     /* index of edge close to west side of surface */
    int eW_dir   /* orientation of edge with respect to surface trim */
    )
{
    const ON_Surface& srf = *brep.m_S[face.m_si];

    /* Create new inner loop */
    ON_BrepLoop& loop = brep.NewLoop(ON_BrepLoop::inner, face);

    /* Create trimming curves running counter clockwise around the
     * surface's domain.  Note that trims of outer loops run counter
     * clockwise while trims of inner loops (holes) run clockwise.
     * Also note that when trims locate on surface N, S, E or W ends,
     * then trim_iso becomes N_iso, S_iso, E_iso and W_iso
     * respectfully.  While if trim is parallel to surface N, S or E,
     * W, then trim iso becomes y_iso and x_iso respectfully.  All
     * other cases, iso is set to not_iso
     */

    /* Start near the south side */
    ON_Curve* c2 = 0;
    int c2i, ei = 0, bRev3d = 0;
    ON_Surface::ISO iso = ON_Surface::not_iso;

    for(int side = 0; side < 4; side++) {
	/* side: 0=near south(y_iso), 1=near west(x_iso), 2=near north(y_iso), 3=near east(x_iso) */

	/* Create trim 2d curve */
	c2 = CreateInnerTrimmingCurve(srf, side);

	/* Add trimming curve to brep trimming curves array */
	c2i = brep.m_C2.Count();
	brep.m_C2.Append(c2);

	switch(side) {
	    case 0: /* near south */
		ei = eSi;
		bRev3d = (eS_dir == -1);
		iso = ON_Surface::y_iso;
		break;
	    case 1: /* near west */
		ei = eEi;
		bRev3d = (eE_dir == -1);
		iso = ON_Surface::x_iso;
		break;
	    case 2: /* near north */
		ei = eNi;
		bRev3d = (eN_dir == -1);
		iso = ON_Surface::y_iso;
		break;
	    case 3: /* near east */
		ei = eWi;
		bRev3d = (eW_dir == -1);
		iso = ON_Surface::x_iso;
		break;
	}

	/* Create new trim topology that references edge, direction
	 * reletive to edge, loop and trim curve geometry
	 */
	ON_BrepTrim& trim = brep.NewTrim(brep.m_E[ei], bRev3d, loop, c2i);
	trim.m_iso = iso;
	trim.m_type = ON_BrepTrim::boundary; /* This one b-rep face,
					      * so all trims are
					      * boundary ones.
					      */
	trim.m_tolerance[0] = 0.0; /* This simple example is exact -
				    * for models with non-exact
				    */
	trim.m_tolerance[1] = 0.0; /* data, set tolerance as explained
				    * in definition of ON_BrepTrim.
				    */
    }

    return loop.m_loop_index;
}


static void CreateFace(ON_Brep& brep, int si)
{
    /* Add new face to brep */
    ON_BrepFace& face = brep.NewFace(si);

    /* Create outer loop and trims for the face */
    MakeOuterTrimmingLoop(brep, face,
			  A, B, C, D, /* Indices of vertices listed in
				       * SW, SE, NW, NE order
				       */
			  AB, +1,     /* South side edge and its
				       * orientation with respect to
				       * to the trimming curve.  (AB)
				       */
			  BC, +1,     /* East side edge and its
				       * orientation with respect to
				       * to the trimming curve.  (BC)
				       */
			  CD, +1,     /* North side edge and its
				       * orientation with respect to
				       * to the trimming curve
				       * (CD)
				       */
			  AD, -1      /* West side edge and its
				       * orientation with respect to
				       * to the trimming curve
				       * (AD)
				       */
	);


    /* Create loop and trims for the face */
    MakeInnerTrimmingLoop(brep, face,
			  E, F, G, H_, /* Indices of hole vertices  */
			  EF, +1,     /* Parallel to south side edge
				       * and its orientation with
				       * respect to to the trimming
				       * curve.  (EF)
				       */
			  FG, +1,     /* Parallel to east side edge
				       * and its orientation with
				       * respect to to the trimming
				       * curve.  (FG)
				       */
			  GH, +1,     /* Parallel to north side edge
				       * and its orientation with
				       * respect to to the trimming
				       * curve (GH)
				       */
			  EH, -1      /* Parallel to west side edge
				       * and its orientation with
				       * respect to to the trimming
				       * curve (EH)
				       */
	);

    /* Set face direction relative to surface direction */
    face.m_bRev = false;
}


static ON_Brep* MakeBrepFace(double thickness = 0.0)
{
    /* This example demonstrates how to construct a ON_Brep with the
     * topology shown below.
     *
     *
     *   D---------e2-----C
     *   |                |
     *   |  G----e6---H_  |
     *   |  |         |   |
     *   e3 e5        e7  |
     *   |  |         |   |
     *   |  F<---e4---E   |
     *   |                |
     *   A-------e0------>B

     * Things need to be defined in a valid brep:
     *  1- Vertices
     *  2- 3D Curves (geometry)
     *  3- Edges (topology - reference curve geometry)
     *  4- Surface (geometry)
     *  5- Faces (topology - reference surface geometry)
     *  6- Loops (2D parameter space of faces)
     *  4- Trims and 2D curves (2D parameter space of edges)
     */

    /* Vertex points */
    ON_3dPoint point[8] = {
	ON_3dPoint(0.0,  0.0,  0.0),  /* point A = geometry for vertex 0 */
	ON_3dPoint(10.0,  0.0,  0.0), /* point B = geometry for vertex 1 */
	ON_3dPoint(10.0,  10.0, 0.0), /* point C = geometry for vertex 2 */
	ON_3dPoint(0.0,  10.0,  0.0), /* point D = geometry for vertex 3 */

	ON_3dPoint(8.0,  2.0,  0.0),  /* point E = geometry for vertex 4 */
	ON_3dPoint(2.0,  2.0,  0.0),  /* point F = geometry for vertex 5 */
	ON_3dPoint(2.0,  8.0, 0.0),   /* point G = geometry for vertex 6 */
	ON_3dPoint(8.0,  8.0,  0.0),  /* point H_ = geometry for vertex 7 */
    };

    ON_Brep* brep = new ON_Brep();

    /* create four vertices of the outer edges */
    int vi;
    for(vi = 0; vi < 8; vi++) {
	ON_BrepVertex& v = brep->NewVertex(point[vi]);
	v.m_tolerance = 0.0; /* this simple example is exact - for models with */
	/* non-exact data, set tolerance as explained in */
	/* definition of ON_BrepVertex. */
    }

    /* Create 3d curve geometry of the outer boundary  */
    /* The orientations are arbitrarily chosen */
    /* so that the end vertices are in alphabetical order. */
    brep->m_C3.Append(CreateLinearCurve(point[A], point[B])); /* line AB */
    brep->m_C3.Append(CreateLinearCurve(point[B], point[C])); /* line BC */
    brep->m_C3.Append(CreateLinearCurve(point[C], point[D])); /* line CD */
    brep->m_C3.Append(CreateLinearCurve(point[A], point[D])); /* line AD */

    /* Create 3d curve geometry of the hole  */
    brep->m_C3.Append(CreateLinearCurve(point[E], point[F])); /* line EF */
    brep->m_C3.Append(CreateLinearCurve(point[F], point[G])); /* line GH */
    brep->m_C3.Append(CreateLinearCurve(point[G], point[H_])); /* line HI */
    brep->m_C3.Append(CreateLinearCurve(point[E], point[H_])); /* line EI */

    /* Create edge topology for each curve in the brep. */
    CreateEdges(*brep);

    /* Create 3d surface geometry - the orientations are arbitrarily
     * chosen so that some normals point into the cube and others
     * point out of the cube.
     */
    brep->m_S.Append(CreateNurbsSurface(point[A], point[B], point[C], point[D])); /* ABCD */

    /* Create face topology and 2d parameter space loops and trims. */
    CreateFace(*brep, ABCD);

    /* assign thickness value to the whole brep */
    if (!NEAR_ZERO(thickness, 0.001) && brep->m_F.Count() && !brep->IsSolid()) {
	ON_U u;
	u.d = thickness;
	brep->m_brep_user = u;

	//for (int fi = 0; fi < b->m_F.Count(); fi++)
	//    b->m_F[fi].m_face_user = u;
    }

    /* Make sure b-rep is valid */
    if(!brep->IsValid()) {
	bu_log("B-rep face is not valid.\n");
	delete brep;
	brep = 0;
    }
    return brep;
}


////////////////////////////////////////////////////////////////

static void
usage(const char *argv0)
{
    bu_log("Usage: %s [options]\n\n", argv0);

    bu_log("    -H plate_thickness\n"
	   "\t\tThickness (mm) used when a NURBS is not a closed volume and it's\n"
	   "\t\tconverted as a plate mode NURBS\n");
}


int main(int argc, char *argv[])
{
    struct rt_wdb* outfp;
    ON_Brep* brep;

    const char* id_name = "B-Rep Surface with interior trim";
    const char* geom_name = "brep_trimmed.s";
    fastf_t thickness = 0.0;

    bu_setprogname(argv[0]);

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

    /* export brep to file */
    bu_log("Writing a b-rep surface...\n");
    outfp = wdb_fopen("brep_trimmed.g");
    mk_id(outfp, id_name);

    brep = MakeBrepFace(thickness);
    mk_brep(outfp, geom_name, brep);

    unsigned char rgb[] = {50, 255, 50};
    mk_region1(outfp, "brep_trimmed.r", geom_name, "plastic", "", rgb);

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
