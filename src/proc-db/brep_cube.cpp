/*                   B R E P _ C U B E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file proc-db/brep_cube.cpp
 *
 * Creates a brep with the following topology making direct use of
 * the openNURBS API:
 *
 *             H-------e6-------G
 *            /                /|
 *           / |              / |
 *          /  e7            /  e5
 *         /   |            /   |
 *        /                e10  |
 *       /     |          /     |
 *      e11    E- - e4- -/- - - F
 *     /                /      /
 *    /      /         /      /
 *   D---------e2-----C      e9
 *   |     /          |     /
 *   |    e8          |    /
 *   e3  /            e1  /
 *   |                |  /
 *   | /              | /
 *   |                |/
 *   A-------e0-------B
 *
 * Copied almost verbatim from the file
 * src/other/openNURBS/example_brep.cpp in order to explore the API
 * for creating B-Reps. There is a slight bit more relevant
 * documentation in this version than the other. In addition, this
 * version uses the wdb interface to add the B-Rep to a BRL-CAD
 * geometry file.
 */

#include "common.h"

#include "bio.h"

/* without OBJ_BREP, this entire procedural example is disabled */
#ifdef OBJ_BREP

#include "twistedcube.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "vmath.h"		/* BRL-CAD Vector macros */
#include "wdb.h"

#ifdef __cplusplus
}
#endif

/* Prevent enum conflict with vmath.h */
namespace  {

enum {
    A, B, C, D, E, F, G, H
};

enum {
    AB, BC, CD, AD, EF, FG, GH, EH, AE, BF, CG, DH
};

enum {
    ABCD, BFGC, FEHG, EADH, EFBA, DCGH
};

void
MakeTwistedCubeEdges(ON_Brep& brep)
{
    MakeTwistedCubeEdge(brep, A, B, AB);
    MakeTwistedCubeEdge(brep, B, C, BC);
    MakeTwistedCubeEdge(brep, C, D, CD);
    MakeTwistedCubeEdge(brep, A, D, AD);
    MakeTwistedCubeEdge(brep, E, F, EF);
    MakeTwistedCubeEdge(brep, F, G, FG);
    MakeTwistedCubeEdge(brep, G, H, GH);
    MakeTwistedCubeEdge(brep, E, H, EH);
    MakeTwistedCubeEdge(brep, A, E, AE);
    MakeTwistedCubeEdge(brep, B, F, BF);
    MakeTwistedCubeEdge(brep, C, G, CG);
    MakeTwistedCubeEdge(brep, D, H, DH);
}

void
MakeTwistedCubeFaces(ON_Brep& brep)
{
    MakeTwistedCubeFace(brep,
			ABCD, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			A, B, C, D, // indices of vertices listed in order
			AB, +1, // south edge, orientation w.r.t. trimming curve?
			BC, +1, // east edge, orientation w.r.t. trimming curve?
			CD, +1,
			AD, -1);

    MakeTwistedCubeFace(brep,
			BFGC, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			B, F, G, C, // indices of vertices listed in order
			BF, +1, // south edge, orientation w.r.t. trimming curve?
			FG, +1, // east edge, orientation w.r.t. trimming curve?
			CG, -1,
			BC, -1);

    // ok, i think I understand the trimming curve orientation
    // thingie. maybe.  since the edge "directions" are arbitrary
    // (e.g. AD instead of DA (which would be more appropriate)) one
    // must indicate that the direction with relation to the trimming
    // curve (which only goes in ONE direction) - 1="in the same
    // direction" and -1="in the opposite direction"

    MakeTwistedCubeFace(brep,
			FEHG, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			F, E, H, G, // indices of vertices listed in order
			EF, -1, // south edge, orientation w.r.t. trimming curve?
			EH, +1, // east edge, orientation w.r.t. trimming curve?
			GH, -1,
			FG, -1);

    MakeTwistedCubeFace(brep,
			EADH, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			E, A, D, H, // indices of vertices listed in order
			AE, -1, // south edge, orientation w.r.t. trimming curve?
			AD, +1, // east edge, orientation w.r.t. trimming curve?
			DH, +1,
			EH, -1);

    MakeTwistedCubeFace(brep,
			EFBA, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			E, F, B, A, // indices of vertices listed in order
			EF, +1, // south edge, orientation w.r.t. trimming curve?
			BF, -1, // east edge, orientation w.r.t. trimming curve?
			AB, -1,
			AE, +1);

    MakeTwistedCubeFace(brep,
			DCGH, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			D, C, G, H, // indices of vertices listed in order
			CD, -1, // south edge, orientation w.r.t. trimming curve?
			CG, +1, // east edge, orientation w.r.t. trimming curve?
			GH, +1,
			DH, -1);

}

ON_Brep*
MakeTwistedCube(ON_TextLog& error_log)
{
    ON_3dPoint point[8] = {
	ON_3dPoint(0.0,  0.0, 11.0), // Point A
	ON_3dPoint(10.0,  0.0, 12.0), // Point B
	ON_3dPoint(10.0,  8.0, 13.0), // Point C
	ON_3dPoint(0.0,  6.0, 12.0), // Point D
	ON_3dPoint(1.0,  2.0,  0.0), // Point E
	ON_3dPoint(10.0,  0.0,  0.0), // Point F
	ON_3dPoint(10.0,  7.0, -1.0), // Point G
	ON_3dPoint(0.0,  6.0,  0.0), // Point H
    };

    ON_Brep* brep = new ON_Brep();

    // create eight vertices located at the eight points
    for (int i = 0; i < 8; i++) {
	ON_BrepVertex& v = brep->NewVertex(point[i]);
	v.m_tolerance = 0.0;
	// this example uses exact tolerance... reference
	// ON_BrepVertex for definition of non-exact data
    }

    // create 3d curve geometry - the orientations are arbitrarily
    // chosen so that the end vertices are in alphabetical order
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[A], point[B])); // AB
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[B], point[C])); // BC
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[C], point[D])); // CD
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[A], point[D])); // AD
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[E], point[F])); // EF
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[F], point[G])); // FG
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[G], point[H])); // GH
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[E], point[H])); // EH
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[A], point[E])); // AE
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[B], point[F])); // BF
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[C], point[G])); // CG
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[D], point[H])); // DH

    // create the 12 edges the connect the corners
    MakeTwistedCubeEdges(*brep);

    // create the 3d surface geometry. the orientations are arbitrary so
    // some normals point into the cube and other point out... not sure why
    brep->m_S.Append(TwistedCubeSideSurface(point[A], point[B], point[C], point[D]));
    brep->m_S.Append(TwistedCubeSideSurface(point[B], point[F], point[G], point[C]));
    brep->m_S.Append(TwistedCubeSideSurface(point[F], point[E], point[H], point[G]));
    brep->m_S.Append(TwistedCubeSideSurface(point[E], point[A], point[D], point[H]));
    brep->m_S.Append(TwistedCubeSideSurface(point[E], point[F], point[B], point[A]));
    brep->m_S.Append(TwistedCubeSideSurface(point[D], point[C], point[G], point[H]));

    // create the faces
    MakeTwistedCubeFaces(*brep);

    if (brep && !brep->IsValid()) {
	error_log.Print("Twisted cube b-rep is not valid!\n");
	delete brep;
	brep = NULL;
    }

    return brep;
}

}


static void
printusage(void) {
    fprintf(stderr,"Usage: brep_cube (takes no arguments)\n");
}


int
main(int argc, char** argv)
{
    struct rt_wdb* outfp;
    ON_Brep* brep;
    ON_TextLog error_log;
    const char* id_name = "B-Rep Example";
    const char* geom_name = "cube.s";

    if (BU_STR_EQUAL(argv[1], "-h") || BU_STR_EQUAL(argv[1], "-?")) {
    	printusage();
    	return 0;
    }
    if (argc >= 1) {
    	printusage();
    	fprintf(stderr,"       Program continues running (will create file brep_cube.g):\n");
    }

    ON::Begin();

    /* export brep to file */
    bu_log("Writing a twisted cube b-rep...\n");
    outfp = wdb_fopen("brep_cube.g");
    mk_id(outfp, id_name);

    brep = MakeTwistedCube(error_log);
    if (!brep)
	bu_exit(1, "ERROR: unable to make the cube\n");

    mk_brep(outfp, geom_name, brep);

    //mk_comb1(outfp, "cube.r", geom_name, 1);
    unsigned char rgb[] = {255, 255, 255};
    mk_region1(outfp, "cube.r", geom_name, "plastic", "", rgb);

    wdb_close(outfp);
    delete brep;

    /* reread from file to make sure brep import is working okay */
    bu_log("Reading a twisted cube b-rep...\n");
    struct db_i* dbip = db_open("brep_cube.g", DB_OPEN_READONLY);
    if (!dbip) {
	bu_exit(1, "Unable to find brep_cube.g geometry database file.");
    }
    db_dirbuild(dbip);
    struct directory* dirp;
    if ((dirp = db_lookup(dbip, "cube.s", 0)) != RT_DIR_NULL) {
	bu_log("\tfound cube.s\n");
	struct rt_db_internal ip;
	mat_t mat;
	MAT_IDN(mat);

	if (rt_db_get_internal(&ip, dirp, dbip, mat, &rt_uniresource) >= 0)
	    printPoints((struct rt_brep_internal*)ip.idb_ptr);
	else
	    bu_log("problem getting internal object rep\n");

    }
    db_close(dbip);

    ON::End();

    return 0;
}

#else /* !OBJ_BREP */

#include "bu.h"

int
main(int UNUSED(argc), char *UNUSED(argv[]))
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
