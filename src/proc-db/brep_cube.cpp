/*                           B R E P _ C U B E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file brep_cube.c
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

#include <stdio.h>		/* Direct the output to stdout */

/* without OBJ_BREP, this entire procedural example is disabled */
#ifdef OBJ_BREP

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "machine.h"		/* BRL-CAD specific machine data types */
#include "vmath.h"		/* BRL-CAD Vector macros */
#include "wdb.h"

#ifdef __cplusplus
}
#endif

enum {
    A, B, C, D, E, F, G, H
};

enum {
    AB, BC, CD, AD, EF, FG, GH, EH, AE, BF, CG, DH
};

enum {
    ABCD, BFGC, FEHG, EADH, EFBA, DCGH
};

ON_Curve* 
TwistedCubeEdgeCurve( const ON_3dPoint& from, const ON_3dPoint& to)
{
    // creates a 3d line segment to be used as a 3d curve in an ON_Brep
    ON_Curve* c3d = new ON_LineCurve( from, to );
    c3d->SetDomain( 0.0, 1.0 ); // XXX is this UV bounds?
    return c3d;
}

void 
MakeTwistedCubeEdge(ON_Brep& brep, int from, int to, int curve) 
{
    ON_BrepVertex& v0 = brep.m_V[from];
    ON_BrepVertex& v1 = brep.m_V[to];
    ON_BrepEdge& edge = brep.NewEdge(v0,v1,curve);
    edge.m_tolerance = 0.0; // exact!
}

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

ON_Surface* 
TwistedCubeSideSurface(const ON_3dPoint& SW, const ON_3dPoint& SE, const ON_3dPoint& NE, const ON_3dPoint& NW)
{
    ON_NurbsSurface* pNurbsSurface = new ON_NurbsSurface(3, // dimension
							 FALSE, // not rational
							 2, // u order
							 2, // v order,
							 2, // number of control vertices in u
							 2 // number of control verts in v
							 );
    pNurbsSurface->SetCV(0, 0, SW);
    pNurbsSurface->SetCV(1, 0, SE);
    pNurbsSurface->SetCV(1, 1, NE);
    pNurbsSurface->SetCV(0, 1, NW);
    // u knots
    pNurbsSurface->SetKnot(0, 0, 0.0);
    pNurbsSurface->SetKnot(0, 1, 1.0);
    // v knots
    pNurbsSurface->SetKnot(1, 0, 0.0);
    pNurbsSurface->SetKnot(1, 1, 1.0);
  
    return pNurbsSurface;
}

ON_Curve* 
TwistedCubeTrimmingCurve(const ON_Surface& s,
			 int side // 0 = SW to SE, 1 = SE to NE, 2 = NE to NW, 3 = NW, SW
			 ) 
{
    // a trimming curve is a 2d curve whose image lies in the surface's
    // domain. The "active" portion of the surface is to the left of the
    // trimming curve (looking down the orientation of the curve). An
    // outer trimming loop consists of a simple closed curve running
    // counter-clockwise around the region it trims
  
    ON_2dPoint from, to;
    double u0, u1, v0, v1;
    s.GetDomain(0, &u0, &u1);
    s.GetDomain(1, &v0, &v1);
  
    switch (side) {
	case 0:
	    from.x = u0; from.y = v0;
	    to.x   = u1; to.y   = v0;
	    break;
	case 1:
	    from.x = u1; from.y = v0;
	    to.x   = u1; to.y   = v1;
	    break;
	case 2:
	    from.x = u1; from.y = v1;
	    to.x   = u0; to.y   = v1;
	    break;
	case 3:
	    from.x = u0; from.y = v1;
	    to.x   = u0; to.y   = v0;
	    break;
	default:
	    return NULL;
    }
    ON_Curve* c2d = new ON_LineCurve(from,to);
    c2d->SetDomain(0.0,1.0);
    return c2d;
}
			

int // return value not used?
MakeTwistedCubeTrimmingLoop(ON_Brep& brep,
			    ON_BrepFace& face,
			    int v0, int v1, int v2, int v3, // indices of corner vertices
			    int e0, int eo0, // edge index + orientation w.r.t surface trim
			    int e1, int eo1,
			    int e2, int eo2,
			    int e3, int eo3)
{
    // get a reference to the surface
    const ON_Surface& srf = *brep.m_S[face.m_si];
  
    ON_BrepLoop& loop = brep.NewLoop(ON_BrepLoop::outer, face);
  
    // create the trimming curves running counter-clockwise around the
    // surface's domain, start at the south side
    ON_Curve* c2;
    int c2i, ei = 0, bRev3d = 0;
    ON_2dPoint q;
  
    // flags for isoparametric curves
    ON_Surface::ISO iso = ON_Surface::not_iso;
  
    for (int side = 0; side < 4; side++) {
	// side: 0=south, 1=east, 2=north, 3=west
	c2 = TwistedCubeTrimmingCurve( srf, side );
	c2i = brep.m_C2.Count();
	brep.m_C2.Append(c2);
    
	switch (side) {
	    case 0:
		ei = e0;
		bRev3d = (eo0 == -1);
		iso = ON_Surface::S_iso;
		break;
	    case 1:
		ei = e1;
		bRev3d = (eo1 == -1);
		iso = ON_Surface::E_iso;
		break;
	    case 2:
		ei = e2;
		bRev3d = (eo2 == -1);
		iso = ON_Surface::N_iso;
		break;
	    case 3:
		ei = e3;
		bRev3d = (eo3 == -1);
		iso = ON_Surface::W_iso;
		break;
	}
    
	ON_BrepTrim& trim = brep.NewTrim(brep.m_E[ei], bRev3d, loop, c2i);
	// not sure these two lines are needed!
	q = c2->PointAtStart();
	q = c2->PointAtEnd();
	trim.m_iso = iso;
    
	// the type gives metadata on the trim type in this case, "mated"
	// means the trim is connected to an edge, is part of an
	// outer/inner/slit loop, no other trim from the same edge is
	// connected to the edge, and at least one trim from a different
	// loop is connected to the edge
	trim.m_type = ON_BrepTrim::mated; // i.e. this b-rep is closed, so
	// all trims have mates

	// not convinced these shouldn't be set with a member function
	trim.m_tolerance[0] = 0.0; // exact
	trim.m_tolerance[1] = 0.0; //
    }
    return loop.m_loop_index;
}

void
MakeTwistedCubeFace(ON_Brep& brep, 
		    int surf, 
		    int orientation, 
		    int v0, int v1, int v2, int v3, // the indices of corner vertices
		    int e0, int eo0, // edge index + orientation
		    int e1, int eo1,
		    int e2, int eo2,
		    int e3, int eo3) 
{
    ON_BrepFace& face = brep.NewFace(surf);
    MakeTwistedCubeTrimmingLoop(brep,
				face,
				v0, v1, v2, v3,
				e0, eo0,
				e1, eo1,
				e2, eo2,
				e3, eo3);
    // should the normal be reversed?
    face.m_bRev = (orientation == -1);
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
			AD, -1); // XXX ????

    MakeTwistedCubeFace(brep, 
			BFGC, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			B, F, G, C, // indices of vertices listed in order
			BF, +1, // south edge, orientation w.r.t. trimming curve?
			FG, +1, // east edge, orientation w.r.t. trimming curve?
			CG, -1,
			BC, -1); // XXX ????

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
			FG, -1); // XXX ????

    MakeTwistedCubeFace(brep, 
			EADH, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			E, A, D, H, // indices of vertices listed in order
			AE, -1, // south edge, orientation w.r.t. trimming curve?
			AD, +1, // east edge, orientation w.r.t. trimming curve?
			DH, +1,
			EH, -1); // XXX ????

    MakeTwistedCubeFace(brep,
			EFBA, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			E, F, B, A, // indices of vertices listed in order
			EF, +1, // south edge, orientation w.r.t. trimming curve?
			BF, -1, // east edge, orientation w.r.t. trimming curve?
			AB, -1,
			AE, +1); // XXX ????

    MakeTwistedCubeFace(brep, 
			DCGH, // index of surface geometry
			+1,   // orientation of surface w.r.t. brep
			D, C, G, H, // indices of vertices listed in order
			CD, -1, // south edge, orientation w.r.t. trimming curve?
			CG, +1, // east edge, orientation w.r.t. trimming curve?
			GH, +1,
			DH, -1); // XXX ????
  
}

ON_Brep* 
MakeTwistedCube(ON_TextLog& error_log)
{
    ON_3dPoint point[8] = {
	ON_3dPoint( 0.0,  0.0, 11.0), // Point A
	ON_3dPoint(10.0,  0.0, 12.0), // Point B
	ON_3dPoint(10.0,  8.0, 13.0), // Point C
	ON_3dPoint( 0.0,  6.0, 12.0), // Point D
	ON_3dPoint( 1.0,  2.0,  0.0), // Point E
	ON_3dPoint(10.0,  0.0,  0.0), // Point F
	ON_3dPoint(10.0,  7.0, -1.0), // Point G
	ON_3dPoint( 0.0,  6.0,  0.0), // Point H
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
    MakeTwistedCubeEdges( *brep );

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

    if (!brep->IsValid()) {
	error_log.Print("Twisted cube b-rep is not valid!\n");
	delete brep;
	brep = NULL;
    }

    return brep;
}

void
printPoints(struct rt_brep_internal* bi)
{
    ON_TextLog tl(stdout);
    ON_Brep* brep = bi->brep;
    if (brep) {
	const int count = brep->m_V.Count();
	for (int i = 0; i < count; i++) {
	    ON_BrepVertex& bv = brep->m_V[i];
	    bv.Dump(tl);
	}
    } else {
	fprintf(stderr, "brep was NULL!\n");
    }
}

int 
main(int argc, char** argv)
{
    struct rt_wdb* outfp;
    ON_Brep* brep;
    ON_TextLog error_log;
    char* id_name = "B-Rep Example";
    char* geom_name = "cube.s";

    ON::Begin();
  
    if (argc > 1) {
	printf("Writing a twisted cube b-rep...\n");
	outfp = wdb_fopen("brep_cube1.g");
	mk_id(outfp, id_name);
	brep = MakeTwistedCube(error_log);
	mk_brep(outfp, geom_name, brep);

	//mk_comb1(outfp, "cube.r", geom_name, 1);
	unsigned char rgb[] = {255,255,255};
	mk_region1(outfp, "cube.r", geom_name, "flat", "", rgb);
	
	wdb_close(outfp);	
    }
    
    printf("Reading a twisted cube b-rep...\n");
    struct db_i* dbip = db_open("brep_cube1.g", "r");
    db_dirbuild(dbip);
    struct directory* dirp;
    if (dirp = db_lookup(dbip, "cube.s", 0)) {
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
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
