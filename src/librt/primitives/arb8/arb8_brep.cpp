/*                    A R B 8 _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
/** @file arb8_brep.cpp
 *
 * Convert a Generalized Ellipsoid to b-rep form
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "brep.h"

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
		   

 ON_Curve*
 arb_nurbs_edge_curve(const ON_3dPoint& from, const ON_3dPoint& to){
    ON_Curve* c3d = new ON_LineCurve( from, to );
    c3d->SetDomain( 0.0, 1.0 );
    return c3d;
 }

 void
 arb_nurbs_edge(ON_Brep* brep, int from, int to, int curve){
     ON_BrepVertex& v0 = brep->m_V[from];
     ON_BrepVertex& v1 = brep->m_V[to];
     ON_BrepEdge& edge = brep->NewEdge(v0,v1,curve);
     edge.m_tolerance = 0.0;
 }

 ON_Curve*
 arb_nurbs_trimming_curve(const ON_Surface& s, int side){
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
     ON_Curve* c2d = new ON_LineCurve(from, to);
     c2d->SetDomain(0.0, 1.0);
     return c2d;
 }



 ON_Surface*
 arb_nurbs_surface(const ON_3dPoint& SW, const ON_3dPoint& SE, const ON_3dPoint& NE, const ON_3dPoint& NW)
 {
     ON_NurbsSurface* pNurbsSurface = new ON_NurbsSurface(3, FALSE, 2, 2, 2, 2);
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
 
 void arb_nurbs_face(ON_Brep* brep, int surf, int orientation, int v0, int v1, int v2, int v3, int e0, 
	 int eo0, int e1, int eo1, int e2, int eo2, int e3, int eo3) {
     ON_BrepFace& face = brep->NewFace(surf);
     const ON_Surface& srf = *brep->m_S[face.m_si];
     ON_BrepLoop& loop = brep->NewLoop(ON_BrepLoop::outer, face);
     ON_Curve* c2;
     int c2i, ei = 0, bRev3d = 0;
     ON_2dPoint q;
     ON_Surface::ISO iso = ON_Surface::not_iso;
     for (int side = 0; side < 4; side++) {
	 c2 = arb_nurbs_trimming_curve(srf, side);
	 c2i = brep->m_C2.Count();
	 brep->m_C2.Append(c2);
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
   	 ON_BrepTrim& trim = brep->NewTrim(brep->m_E[ei], bRev3d, loop, c2i);
	 trim.m_iso = iso;
	 trim.m_type = ON_BrepTrim::mated;
	 trim.m_tolerance[0] = 0.0;
	 trim.m_tolerance[1] = 0.0;
     }
     face.m_bRev = (orientation == -1);
 }


/**
 *			R T _ A R B 8 _ B R E P
 */
extern "C" void
rt_arb8_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    mat_t	R;
    mat_t	S;
    struct rt_arb_internal	*arb8ip;

    *b = NULL; 
    *b = ON_Brep::New();

    ON_Brep* brep = *b;

    RT_CK_DB_INTERNAL(ip);
    arb8ip = (struct rt_arb_internal *)ip->idb_ptr;
    RT_ARB_CK_MAGIC(arb8ip);

    ON_3dPoint point[8] = {
    	ON_3dPoint(arb8ip->pt[D]),
    	ON_3dPoint(arb8ip->pt[C]),
        ON_3dPoint(arb8ip->pt[G]),
    	ON_3dPoint(arb8ip->pt[H]),
    	ON_3dPoint(arb8ip->pt[B]),
    	ON_3dPoint(arb8ip->pt[A]), 
    	ON_3dPoint(arb8ip->pt[E]),
    	ON_3dPoint(arb8ip->pt[F])
    };

    for (int i = 0; i < 8; i++) {
	 ON_BrepVertex& v = brep->NewVertex(point[i]);
	 v.m_tolerance = 0.0;
    }
    
    brep->m_C3.Append(arb_nurbs_edge_curve(point[A], point[B])); // AB
    brep->m_C3.Append(arb_nurbs_edge_curve(point[B], point[C])); // BC
    brep->m_C3.Append(arb_nurbs_edge_curve(point[C], point[D])); // CD
    brep->m_C3.Append(arb_nurbs_edge_curve(point[D], point[A])); // DA
    brep->m_C3.Append(arb_nurbs_edge_curve(point[B], point[E])); // BE
    brep->m_C3.Append(arb_nurbs_edge_curve(point[E], point[H])); // EH
    brep->m_C3.Append(arb_nurbs_edge_curve(point[H], point[C])); // HC
    brep->m_C3.Append(arb_nurbs_edge_curve(point[E], point[F])); // EF
    brep->m_C3.Append(arb_nurbs_edge_curve(point[F], point[G])); // FG
    brep->m_C3.Append(arb_nurbs_edge_curve(point[G], point[H])); // GH
    brep->m_C3.Append(arb_nurbs_edge_curve(point[F], point[A])); // FA
    brep->m_C3.Append(arb_nurbs_edge_curve(point[D], point[G])); // DG

    arb_nurbs_edge(brep, A, B, AB);
    arb_nurbs_edge(brep, B, C, BC);
    arb_nurbs_edge(brep, C, D, CD);
    arb_nurbs_edge(brep, D, A, DA);
    arb_nurbs_edge(brep, B, E, BE);
    arb_nurbs_edge(brep, E, H, EH);
    arb_nurbs_edge(brep, H, C, HC);
    arb_nurbs_edge(brep, E, F, EF);
    arb_nurbs_edge(brep, F, G, FG);
    arb_nurbs_edge(brep, G, H, GH);
    arb_nurbs_edge(brep, F, A, FA);
    arb_nurbs_edge(brep, D, G, DG);    


    brep->m_S.Append(arb_nurbs_surface(point[A], point[B], point[C], point[D])); //ABCD
    brep->m_S.Append(arb_nurbs_surface(point[B], point[E], point[H], point[C])); //BEHC
    brep->m_S.Append(arb_nurbs_surface(point[E], point[F], point[G], point[H])); //EFGH
    brep->m_S.Append(arb_nurbs_surface(point[F], point[A], point[D], point[G])); //FADG
    brep->m_S.Append(arb_nurbs_surface(point[F], point[E], point[B], point[A])); //FEBA
    brep->m_S.Append(arb_nurbs_surface(point[D], point[C], point[H], point[G])); //DCHG
    
    arb_nurbs_face(brep, ABCD, +1, A, B, C, D, AB, +1, BC, +1, CD, +1, DA, +1);
    arb_nurbs_face(brep, BEHC, +1, B, E, H, C, BE, +1, EH, +1, HC, +1, BC, -1);
    arb_nurbs_face(brep, EFGH, +1, E, F, G, H, EF, +1, FG, +1, GH, +1, EH, -1);
    arb_nurbs_face(brep, FADG, +1, F, A, D, G, FA, +1, DA, -1, DG, +1, FG, -1);
    arb_nurbs_face(brep, FEBA, +1, F, E, B, A, EF, -1, BE, -1, AB, -1, FA, -1);
    arb_nurbs_face(brep, DCHG, +1, D, C, H, G, CD, -1, HC, -1, GH, -1, DG, -1);


    
 //   brep->Standardize();
 //   brep->Compact();
}

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

