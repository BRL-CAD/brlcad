/*                 B R E P _ S I M P L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file proc-db/brep_simple.cpp
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

/* without OBJ_BREP, this entire procedural example is disabled */
#ifdef OBJ_BREP

#include "twistedcube.h"

#include "bio.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "vmath.h"		/* BRL-CAD Vector macros */
#include "wdb.h"

#ifdef __cplusplus
}
#endif


/* Prevent enum conflict with vmath.h */
namespace {

enum {
    A, B, C, D, E, F, G, H
};

enum {
    AB, BC, CD, DA, BE, EH, HC, EF, FG, GH, FA, DG
};

enum {
    ABCD, BEHC, EFGH, FADG, FEBA, DCHG
};

void
MakeTwistedCubeEdges(ON_Brep& brep)
{
    MakeTwistedCubeEdge(brep, A, B, AB);
    MakeTwistedCubeEdge(brep, B, C, BC);
    MakeTwistedCubeEdge(brep, C, D, CD);
    MakeTwistedCubeEdge(brep, D, A, DA);
    MakeTwistedCubeEdge(brep, B, E, BE);
    MakeTwistedCubeEdge(brep, E, H, EH);
    MakeTwistedCubeEdge(brep, H, C, HC);
    MakeTwistedCubeEdge(brep, E, F, EF);
    MakeTwistedCubeEdge(brep, F, G, FG);
    MakeTwistedCubeEdge(brep, G, H, GH);
    MakeTwistedCubeEdge(brep, F, A, FA);
    MakeTwistedCubeEdge(brep, D, G, DG);
}

void
MakeTwistedCubeFaces(ON_Brep& brep)
{
    // The direction of each edge was determined when the edge was
    // created as an object (e.g. AD instead of DA).  When using these
    // edges to define Faces, the direction of the edge is overridden
    // by the direction the trimming loop of the face needs that edge to
    // take - in other words, to properly form the loop for BEHC (for example)
    // the Face definition needs edge CB rather than BC.  Instead of creating
    // a new edge, a factor of -1 is supplied indicating that the BC
    // edge data should be inverted from its default orientation for
    // the purposes of loop assembly.

    MakeTwistedCubeFace(brep,
			ABCD, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			A, B, C, D, // indices of vertices listed in order
			AB, +1, // south edge, orientation w.r.t. trimming curve
			BC, +1, // east edge, orientation w.r.t. trimming curve
			CD, +1, // west edge, orientation w.r.t. trimming curve
			DA, +1);// north edge, orientation w.r.t. trimming curve

    MakeTwistedCubeFace(brep,
			BEHC, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			B, E, H, C, // indices of vertices listed in order
			BE, +1, // south edge, orientation w.r.t. trimming curve
			EH, +1, // east edge, orientation w.r.t. trimming curve
			HC, +1, // west edge, orientation w.r.t. trimming curve
			BC, -1);// north edge, orientation w.r.t. trimming curve

    MakeTwistedCubeFace(brep,
			EFGH, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			E, F, G, H, // indices of vertices listed in order
			EF, +1, // south edge, orientation w.r.t. trimming curve
			FG, +1, // east edge, orientation w.r.t. trimming curve
			GH, +1, // west edge, orientation w.r.t. trimming curve
			EH, -1);// north edge, orientation w.r.t. trimming curve

    MakeTwistedCubeFace(brep,
			FADG, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			F, A, D, G, // indices of vertices listed in order
			FA, +1, // south edge, orientation w.r.t. trimming curve
			DA, -1, // east edge, orientation w.r.t. trimming curve
			DG, +1, // west edge, orientation w.r.t. trimming curve
			FG, -1);// north edge, orientation w.r.t. trimming curve

    MakeTwistedCubeFace(brep,
			FEBA, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			F, E, B, A, // indices of vertices listed in order
			EF, -1, // south edge, orientation w.r.t. trimming curve
			BE, -1, // east edge, orientation w.r.t. trimming curve
			AB, -1, // west edge, orientation w.r.t. trimming curve
			FA, -1);// north edge, orientation w.r.t. trimming curve

    MakeTwistedCubeFace(brep,
			DCHG, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			D, C, H, G, // indices of vertices listed in order
			CD, -1, // south edge, orientation w.r.t. trimming curve
			HC, -1, // east edge, orientation w.r.t. trimming curve
			GH, -1, // west edge, orientation w.r.t. trimming curve
			DG, -1);// north edge, orientation w.r.t. trimming curve

}

ON_Brep*
MakeTwistedCube(ON_TextLog& error_log)
{
    ON_3dPoint point[8] = {
	// front
	ON_3dPoint(0.0,  0.0,  1.0), // Point A
	ON_3dPoint(1.0,  0.0,  1.0), // Point B
	ON_3dPoint(1.0,  1.2,  1.0), // Point C
	ON_3dPoint(0.0,  1.0,  1.0), // Point D

	// back
	ON_3dPoint(1.0,  0.0,  0.0), // Point E
	ON_3dPoint(0.0,  0.0,  0.0), // Point F
	ON_3dPoint(0.0,  1.0,  0.0), // Point G
	ON_3dPoint(1.0,  1.0,  0.0), // Point H
    };

    ON_Brep* brep = new ON_Brep();

    // create eight vertices located at the eight points
    for (int i = 0; i < 8; i++) {
	ON_BrepVertex& v = brep->NewVertex(point[i]);
	v.m_tolerance = 0.0;
	// this example uses exact tolerance... reference
	// ON_BrepVertex for definition of non-exact data
    }

    // create 3d curve geometry -
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[A], point[B])); // AB
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[B], point[C])); // BC
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[C], point[D])); // CD
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[D], point[A])); // DA
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[B], point[E])); // BE
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[E], point[H])); // EH
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[H], point[C])); // HC
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[E], point[F])); // EF
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[F], point[G])); // FG
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[G], point[H])); // GH
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[F], point[A])); // FA
    brep->m_C3.Append(TwistedCubeEdgeCurve(point[D], point[G])); // DG

    // create the 12 edges that connect the corners
    MakeTwistedCubeEdges(*brep);

    // create the 3d surface geometry. the orientations are arbitrary so
    // some normals point into the cube and other point out... not sure why
    brep->m_S.Append(TwistedCubeSideSurface(point[A], point[B], point[C], point[D])); //ABCD
    brep->m_S.Append(TwistedCubeSideSurface(point[B], point[E], point[H], point[C])); //BEHC
    brep->m_S.Append(TwistedCubeSideSurface(point[E], point[F], point[G], point[H])); //EFGH
    brep->m_S.Append(TwistedCubeSideSurface(point[F], point[A], point[D], point[G])); //FADG
    brep->m_S.Append(TwistedCubeSideSurface(point[F], point[E], point[B], point[A])); //FEBA
    brep->m_S.Append(TwistedCubeSideSurface(point[D], point[C], point[H], point[G])); //DCHG

    // create the faces
    MakeTwistedCubeFaces(*brep);

    if (brep && !brep->IsValid()) {
	error_log.Print("Twisted cube b-rep is not valid!\n");
	delete brep;
	brep = NULL;
    }

    return brep;
}

} /* end namespace */


static void
printusage(void) {
    fprintf(stderr,"Usage: brep_simple (takes no arguments)\n");
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
    	fprintf(stderr,"       Program continues running (will create file brep_simple.g):\n");
    }

    ON::Begin();

    printf("Writing a twisted cube b-rep...\n");
    outfp = wdb_fopen("brep_simple.g");
    mk_id(outfp, id_name);

    brep = MakeTwistedCube(error_log);
    if (!brep) {
	bu_exit(1, "ERROR: unable to make the twisted cube\n");
    }
    mk_brep(outfp, geom_name, brep);

    unsigned char rgb[] = {255, 255, 255};
    mk_region1(outfp, "cube.r", geom_name, "plastic", "", rgb);

    wdb_close(outfp);
    delete brep;

    printf("Reading a twisted cube b-rep...\n");
    struct db_i* dbip = db_open("brep_simple.g", DB_OPEN_READONLY);
    if (!dbip) {
	bu_exit(1, "Unable to open brep_simple.g geometry database file\n");
    }
    db_dirbuild(dbip);
    struct directory* dirp;
    if ((dirp = db_lookup(dbip, "cube.s", 0)) != RT_DIR_NULL) {
	printf("\tfound cube.s\n");
	struct rt_db_internal ip;
	mat_t mat;
	MAT_IDN(mat);
	if (rt_db_get_internal(&ip, dirp, dbip, mat, &rt_uniresource) >= 0) {
	    printPoints((struct rt_brep_internal*)ip.idb_ptr);
	} else {
	    fprintf(stderr, "problem getting internal object rep\n");
	}
    }
    db_close(dbip);

    ON::End();

    return 0;
}


#else /* !OBJ_BREP */

int
main(int argc, char *argv[])
{
    printf("ERROR: Boundary Representation object support is not available with\n"
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
