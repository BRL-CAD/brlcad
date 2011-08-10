/*                 B R E P L I C A T O R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file proc-db/breplicator.cpp
 *
 * Breplicator is a tool for testing the new boundary representation
 * (BREP) primitive type in librt.  It creates primitives via
 * mk_brep() for testing purposes.
 *
 */

#include "common.h"

#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "bn.h"
#include "bu.h"


ON_Brep *
generate_brep(int count, ON_3dPoint *points)
{
    ON_Brep *brep = new ON_Brep();

    /* make an arb8 */

    // VERTICES

    for (int i=0; i<count; i++) {
	brep->NewVertex(points[i], SMALL_FASTF);
    }

    ON_3dPoint p8 = ON_3dPoint(-1.0, 0.0, -1.0);
    ON_3dPoint p9 = ON_3dPoint(2.0, 0.0, -1.0);
    ON_3dPoint p10 = ON_3dPoint(2.0, 0.0, 3.5);
    ON_3dPoint p11 = ON_3dPoint(-1.0, 0.0, 3.5);

    brep->NewVertex(p8, SMALL_FASTF); // 8
    brep->NewVertex(p9, SMALL_FASTF); // 9
    brep->NewVertex(p10, SMALL_FASTF); // 10
    brep->NewVertex(p11, SMALL_FASTF); // 11

    // LEFT SEGMENTS

    // 0
    ON_Curve* segment01 = new ON_LineCurve(points[0], points[1]);
    segment01->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment01);

    // 1
    ON_Curve* segment12 = new ON_LineCurve(points[1], points[2]);
    segment12->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment12);

    // 2
    ON_Curve* segment23 = new ON_LineCurve(points[2], points[3]);
    segment23->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment23);

    // 3
    ON_Curve* segment30 = new ON_LineCurve(points[3], points[0]);
    segment30->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment30);

    // RIGHT SEGMENTS

    // 4
    ON_Curve* segment45 = new ON_LineCurve(points[5], points[4]);
    segment45->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment45);

    // 5
    ON_Curve* segment56 = new ON_LineCurve(points[6], points[5]);
    segment56->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment56);

    // 6
    ON_Curve* segment67 = new ON_LineCurve(points[7], points[6]);
    segment67->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment67);

    // 7
    ON_Curve* segment74 = new ON_LineCurve(points[4], points[7]);
    segment74->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment74);

    // HORIZONTAL SEGMENTS

    // 8
    ON_Curve* segment04 = new ON_LineCurve(points[0], points[4]);
    segment04->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment04);

    // 9
    ON_Curve* segment51 = new ON_LineCurve(points[5], points[1]);
    segment51->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment51);

    // 10
    ON_Curve* segment26 = new ON_LineCurve(points[2], points[6]);
    segment26->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment26);

    // 11
    ON_Curve* segment73 = new ON_LineCurve(points[7], points[3]);
    segment73->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment73);

    /* XXX */

    // 12
    ON_Curve* segment01prime = new ON_LineCurve(p8, p9);
    segment01prime->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment01prime);

    // 13
    ON_Curve* segment12prime = new ON_LineCurve(p9, p10);
    segment12prime->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment12prime);

    // 14
    ON_Curve* segment23prime = new ON_LineCurve(p10, p11);
    segment23prime->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment23prime);

    // 15
    ON_Curve* segment30prime = new ON_LineCurve(p11, p8);
    segment30prime->SetDomain(0.0, 1.0);
    brep->m_C3.Append(segment30prime);

    // SURFACES
#if 1
    ON_NurbsSurface* surf0123 = new ON_NurbsSurface(3 /*dimension*/, 0 /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf0123->SetKnot(0, 0, 0.0); surf0123->SetKnot(0, 1, 1.0); surf0123->SetKnot(1, 0, 0.0); surf0123->SetKnot(1, 1, 1.0);
    surf0123->SetCV(0, 0, points[0]);
    surf0123->SetCV(1, 0, points[1]);
    surf0123->SetCV(1, 1, points[2]);
    surf0123->SetCV(0, 1, points[3]);
    brep->m_S.Append(surf0123); /* 0 */
#else
    /* XXX */
    ON_NurbsSurface* surf0123prime = new ON_NurbsSurface(3 /*dimension*/, 0 /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf0123prime->SetKnot(0, 0, 0.0); surf0123prime->SetKnot(0, 1, 1.0); surf0123prime->SetKnot(1, 0, 0.0); surf0123prime->SetKnot(1, 1, 1.0);
    surf0123prime->SetCV(0, 0, p8);
    surf0123prime->SetCV(1, 0, p9);
    surf0123prime->SetCV(1, 1, p10);
    surf0123prime->SetCV(0, 1, p11);
    brep->m_S.Append(surf0123prime); /* 0 */
#endif

    ON_NurbsSurface* surf4765 = new ON_NurbsSurface(3 /*dimension*/, 0 /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf4765->SetKnot(0, 0, 0.0); surf4765->SetKnot(0, 1, 1.0); surf4765->SetKnot(1, 0, 0.0); surf4765->SetKnot(1, 1, 1.0);
    surf4765->SetCV(0, 0, points[4]);
    surf4765->SetCV(1, 0, points[7]);
    surf4765->SetCV(1, 1, points[6]);
    surf4765->SetCV(0, 1, points[5]);
    brep->m_S.Append(surf4765); /* 1 */

    ON_NurbsSurface* surf0451 = new ON_NurbsSurface(3 /*dimension*/, 0 /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf0451->SetKnot(0, 0, 0.0); surf0451->SetKnot(0, 1, 1.0); surf0451->SetKnot(1, 0, 0.0); surf0451->SetKnot(1, 1, 1.0);
    surf0451->SetCV(0, 0, points[0]);
    surf0451->SetCV(1, 0, points[4]);
    surf0451->SetCV(1, 1, points[5]);
    surf0451->SetCV(0, 1, points[1]);
    brep->m_S.Append(surf0451); /* 2 */

    ON_NurbsSurface* surf2673 = new ON_NurbsSurface(3 /*dimension*/, 0 /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf2673->SetKnot(0, 0, 0.0); surf2673->SetKnot(0, 1, 1.0); surf2673->SetKnot(1, 0, 0.0); surf2673->SetKnot(1, 1, 1.0);
    surf2673->SetCV(0, 0, points[2]);
    surf2673->SetCV(1, 0, points[6]);
    surf2673->SetCV(1, 1, points[7]);
    surf2673->SetCV(0, 1, points[3]);
    brep->m_S.Append(surf2673); /* 3 */

    ON_NurbsSurface* surf1562 = new ON_NurbsSurface(3 /*dimension*/, 0 /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf1562->SetKnot(0, 0, 0.0); surf1562->SetKnot(0, 1, 1.0); surf1562->SetKnot(1, 0, 0.0); surf1562->SetKnot(1, 1, 1.0);
    surf1562->SetCV(0, 0, points[1]);
    surf1562->SetCV(1, 0, points[5]);
    surf1562->SetCV(1, 1, points[6]);
    surf1562->SetCV(0, 1, points[2]);
    brep->m_S.Append(surf1562); /* 4 */

    ON_NurbsSurface* surf0374 = new ON_NurbsSurface(3 /*dimension*/, 0 /*nonrational*/, 2 /*u*/, 2 /*v*/, 2 /*#u*/, 2 /*#v*/);
    surf0374->SetKnot(0, 0, 0.0); surf0374->SetKnot(0, 1, 1.0); surf0374->SetKnot(1, 0, 0.0); surf0374->SetKnot(1, 1, 1.0);
    surf0374->SetCV(0, 0, points[0]);
    surf0374->SetCV(1, 0, points[3]);
    surf0374->SetCV(1, 1, points[7]);
    surf0374->SetCV(0, 1, points[4]);
    brep->m_S.Append(surf0374); /* 5 */


    // TRIM CURVES

    ON_Curve* trimcurve01 = new ON_LineCurve(ON_2dPoint(0, 0), ON_2dPoint(1, 0));
    trimcurve01->SetDomain(0.0, 1.0);
    brep->m_C2.Append(trimcurve01); /* 0 */

    ON_Curve* trimcurve12 = new ON_LineCurve(ON_2dPoint(1, 0), ON_2dPoint(1, 1));
    trimcurve12->SetDomain(0.0, 1.0);
    brep->m_C2.Append(trimcurve12); /* 1 */

    ON_Curve* trimcurve23 = new ON_LineCurve(ON_2dPoint(1, 1), ON_2dPoint(0, 1));
    trimcurve23->SetDomain(0.0, 1.0);
    brep->m_C2.Append(trimcurve23); /* 2 */

    ON_Curve* trimcurve30 = new ON_LineCurve(ON_2dPoint(0, 1), ON_2dPoint(0, 0));
    trimcurve30->SetDomain(0.0, 1.0);
    brep->m_C2.Append(trimcurve30); /* 3 */

    // EDGES

    /* C3 curve */
    // left face edges
    brep->NewEdge(brep->m_V[0], brep->m_V[1], 0, NULL, SMALL_FASTF); /* 0 */
    brep->NewEdge(brep->m_V[1], brep->m_V[2], 1, NULL, SMALL_FASTF); /* 1 */
    brep->NewEdge(brep->m_V[2], brep->m_V[3], 2, NULL, SMALL_FASTF); /* 2 */
    brep->NewEdge(brep->m_V[3], brep->m_V[0], 3, NULL, SMALL_FASTF); /* 3 */

    // right face edges
    brep->NewEdge(brep->m_V[5], brep->m_V[4], 4, NULL, SMALL_FASTF); /* 4 */
    brep->NewEdge(brep->m_V[6], brep->m_V[5], 5, NULL, SMALL_FASTF); /* 5 */
    brep->NewEdge(brep->m_V[7], brep->m_V[6], 6, NULL, SMALL_FASTF); /* 6 */
    brep->NewEdge(brep->m_V[4], brep->m_V[7], 7, NULL, SMALL_FASTF); /* 7 */

    // horizontal face edges
    brep->NewEdge(brep->m_V[0], brep->m_V[4], 8, NULL, SMALL_FASTF); /* 8 */
    brep->NewEdge(brep->m_V[5], brep->m_V[1], 9, NULL, SMALL_FASTF); /* 9 */
    brep->NewEdge(brep->m_V[2], brep->m_V[6], 10, NULL, SMALL_FASTF); /* 10 */
    brep->NewEdge(brep->m_V[7], brep->m_V[3], 11, NULL, SMALL_FASTF); /* 11 */

    // XXX
    brep->NewEdge(brep->m_V[8], brep->m_V[9], 12, NULL, SMALL_FASTF); /* 12 */
    brep->NewEdge(brep->m_V[9], brep->m_V[10], 13, NULL, SMALL_FASTF); /* 13 */
    brep->NewEdge(brep->m_V[10], brep->m_V[11], 14, NULL, SMALL_FASTF); /* 14 */
    brep->NewEdge(brep->m_V[11], brep->m_V[8], 15, NULL, SMALL_FASTF); /* 15 */

    // FACES

#if 1
    ON_BrepFace& face0123 = brep->NewFace(0);
    ON_BrepLoop& loop0123 = brep->NewLoop(ON_BrepLoop::outer, face0123); /* 0 */
    ON_BrepTrim& trim01 = brep->NewTrim(brep->m_E[0], false, loop0123, 0 /* trim */); /* m_T[0] */
    trim01.m_iso = ON_Surface::S_iso;
    trim01.m_type = ON_BrepTrim::mated;
    trim01.m_tolerance[0] = SMALL_FASTF;
    trim01.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim12 = brep->NewTrim(brep->m_E[1], false, loop0123, 1 /* trim */); /* 1 */
    trim12.m_iso = ON_Surface::E_iso;
    trim12.m_type = ON_BrepTrim::mated;
    trim12.m_tolerance[0] = SMALL_FASTF;
    trim12.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim23 = brep->NewTrim(brep->m_E[2], false, loop0123, 2 /* trim */); /* 2 */
    trim23.m_iso = ON_Surface::N_iso;
    trim23.m_type = ON_BrepTrim::mated;
    trim23.m_tolerance[0] = SMALL_FASTF;
    trim23.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim30 = brep->NewTrim(brep->m_E[3], false, loop0123, 3 /* trim */); /* 3 */
    trim30.m_iso = ON_Surface::W_iso;
    trim30.m_type = ON_BrepTrim::mated;
    trim30.m_tolerance[0] = SMALL_FASTF;
    trim30.m_tolerance[1] = SMALL_FASTF;
#else
    ON_BrepFace& face0123 = brep->NewFace(0 /* surfaceID */);
    ON_BrepLoop& loop0123 = brep->NewLoop(ON_BrepLoop::outer, face0123); /* 0 */
    ON_BrepTrim& trim01 = brep->NewTrim(brep->m_E[0], false, loop0123, 0 /* trim */); /* 0 */
    trim01.m_iso = ON_Surface::S_iso;
    trim01.m_type = ON_BrepTrim::boundary;
    trim01.m_tolerance[0] = SMALL_FASTF;
    trim01.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim12 = brep->NewTrim(brep->m_E[1], false, loop0123, 1 /* trim */); /* 1 */
    trim12.m_iso = ON_Surface::E_iso;
    trim12.m_type = ON_BrepTrim::boundary;
    trim12.m_tolerance[0] = SMALL_FASTF;
    trim12.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim23 = brep->NewTrim(brep->m_E[2], false, loop0123, 2 /* trim */); /* 2 */
    trim23.m_iso = ON_Surface::N_iso;
    trim23.m_type = ON_BrepTrim::boundary;
    trim23.m_tolerance[0] = SMALL_FASTF;
    trim23.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim30 = brep->NewTrim(brep->m_E[3], false, loop0123, 3 /* trim */); /* 3 */
    trim30.m_iso = ON_Surface::W_iso;
    trim30.m_type = ON_BrepTrim::boundary;
    trim30.m_tolerance[0] = SMALL_FASTF;
    trim30.m_tolerance[1] = SMALL_FASTF;
#endif

#if 1
    ON_BrepFace& face4765 = brep->NewFace(1 /* surfaceID */);
    ON_BrepLoop& loop4765 = brep->NewLoop(ON_BrepLoop::outer, face4765); /* 1 */
    ON_BrepTrim& trim47 = brep->NewTrim(brep->m_E[7], false, loop4765, 0 /* trim */); /* 4 */
    trim47.m_iso = ON_Surface::S_iso;
    trim47.m_type = ON_BrepTrim::mated;
    trim47.m_tolerance[0] = SMALL_FASTF;
    trim47.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim76 = brep->NewTrim(brep->m_E[6], false, loop4765, 1 /* trim */); /* 5 */
    trim76.m_iso = ON_Surface::E_iso;
    trim76.m_type = ON_BrepTrim::mated;
    trim76.m_tolerance[0] = SMALL_FASTF;
    trim76.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim65 = brep->NewTrim(brep->m_E[5], false, loop4765, 2 /* trim */); /* 6 */
    trim65.m_iso = ON_Surface::N_iso;
    trim65.m_type = ON_BrepTrim::mated;
    trim65.m_tolerance[0] = SMALL_FASTF;
    trim65.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim54 = brep->NewTrim(brep->m_E[4], false, loop4765, 3 /* trim */); /* 7 */
    trim54.m_iso = ON_Surface::W_iso;
    trim54.m_type = ON_BrepTrim::mated;
    trim54.m_tolerance[0] = SMALL_FASTF;
    trim54.m_tolerance[1] = SMALL_FASTF;

    ON_BrepFace& face0451 = brep->NewFace(2);
    ON_BrepLoop& loop0451 = brep->NewLoop(ON_BrepLoop::outer, face0451); /* 2 */
    ON_BrepTrim& trim04 = brep->NewTrim(brep->m_E[8], false, loop0451, 0 /* trim */); /* 8 */
    trim04.m_iso = ON_Surface::S_iso;
    trim04.m_type = ON_BrepTrim::mated;
    trim04.m_tolerance[0] = SMALL_FASTF;
    trim04.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim45 = brep->NewTrim(brep->m_E[4], true, loop0451, 1 /* trim */); /* 9 */
    trim45.m_iso = ON_Surface::E_iso;
    trim45.m_type = ON_BrepTrim::mated;
    trim45.m_tolerance[0] = SMALL_FASTF;
    trim45.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim51 = brep->NewTrim(brep->m_E[9], false, loop0451, 2 /* trim */); /* 10 */
    trim51.m_iso = ON_Surface::N_iso;
    trim51.m_type = ON_BrepTrim::mated;
    trim51.m_tolerance[0] = SMALL_FASTF;
    trim51.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim10 = brep->NewTrim(brep->m_E[0], true, loop0451, 3 /* trim */); /* 11 */
    trim10.m_iso = ON_Surface::W_iso;
    trim10.m_type = ON_BrepTrim::mated;
    trim10.m_tolerance[0] = SMALL_FASTF;
    trim10.m_tolerance[1] = SMALL_FASTF;

    ON_BrepFace& face2673 = brep->NewFace(3);
    ON_BrepLoop& loop2673 = brep->NewLoop(ON_BrepLoop::outer, face2673); /* 3 */
    ON_BrepTrim& trim26 = brep->NewTrim(brep->m_E[10], false, loop2673, 0 /* trim */); /* 12 */
    trim26.m_iso = ON_Surface::S_iso;
    trim26.m_type = ON_BrepTrim::mated;
    trim26.m_tolerance[0] = SMALL_FASTF;
    trim26.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim67 = brep->NewTrim(brep->m_E[6], true, loop2673, 1 /* trim */); /* 13 */
    trim67.m_iso = ON_Surface::E_iso;
    trim67.m_type = ON_BrepTrim::mated;
    trim67.m_tolerance[0] = SMALL_FASTF;
    trim67.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim73 = brep->NewTrim(brep->m_E[11], false, loop2673, 2 /* trim */); /* 14 */
    trim73.m_iso = ON_Surface::N_iso;
    trim73.m_type = ON_BrepTrim::mated;
    trim73.m_tolerance[0] = SMALL_FASTF;
    trim73.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim32 = brep->NewTrim(brep->m_E[2], true, loop2673, 3 /* trim */); /* 15 */
    trim32.m_iso = ON_Surface::W_iso;
    trim32.m_type = ON_BrepTrim::mated;
    trim32.m_tolerance[0] = SMALL_FASTF;
    trim32.m_tolerance[1] = SMALL_FASTF;

    ON_BrepFace& face1562 = brep->NewFace(4);
    ON_BrepLoop& loop1562 = brep->NewLoop(ON_BrepLoop::outer, face1562); /* 4 */
    ON_BrepTrim& trim15 = brep->NewTrim(brep->m_E[9], true, loop1562, 0 /* trim */); /* 16 */
    trim15.m_iso = ON_Surface::S_iso;
    trim15.m_type = ON_BrepTrim::mated;
    trim15.m_tolerance[0] = SMALL_FASTF;
    trim15.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim56 = brep->NewTrim(brep->m_E[5], true, loop1562, 1 /* trim */); /* 17 */
    trim56.m_iso = ON_Surface::E_iso;
    trim56.m_type = ON_BrepTrim::mated;
    trim56.m_tolerance[0] = SMALL_FASTF;
    trim56.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim62 = brep->NewTrim(brep->m_E[10], true, loop1562, 2 /* trim */); /* 18 */
    trim62.m_iso = ON_Surface::N_iso;
    trim62.m_type = ON_BrepTrim::mated;
    trim62.m_tolerance[0] = SMALL_FASTF;
    trim62.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim21 = brep->NewTrim(brep->m_E[1], true, loop1562, 3 /* trim */); /* 19 */
    trim21.m_iso = ON_Surface::W_iso;
    trim21.m_type = ON_BrepTrim::mated;
    trim21.m_tolerance[0] = SMALL_FASTF;
    trim21.m_tolerance[1] = SMALL_FASTF;

    ON_BrepFace& face0374 = brep->NewFace(5);
    ON_BrepLoop& loop0374 = brep->NewLoop(ON_BrepLoop::outer, face0374); /* 5 */
    ON_BrepTrim& trim03 = brep->NewTrim(brep->m_E[3], true, loop0374, 0 /* trim */); /* 20 */
    trim03.m_iso = ON_Surface::S_iso;
    trim03.m_type = ON_BrepTrim::mated;
    trim03.m_tolerance[0] = SMALL_FASTF;
    trim03.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim37 = brep->NewTrim(brep->m_E[11], true, loop0374, 1 /* trim */); /* 21 */
    trim37.m_iso = ON_Surface::E_iso;
    trim37.m_type = ON_BrepTrim::mated;
    trim37.m_tolerance[0] = SMALL_FASTF;
    trim37.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim74 = brep->NewTrim(brep->m_E[7], true, loop0374, 2 /* trim */); /* 22 */
    trim74.m_iso = ON_Surface::N_iso;
    trim74.m_type = ON_BrepTrim::mated;
    trim74.m_tolerance[0] = SMALL_FASTF;
    trim74.m_tolerance[1] = SMALL_FASTF;
    ON_BrepTrim& trim40 = brep->NewTrim(brep->m_E[8], true, loop0374, 3 /* trim */); /* 23 */
    trim40.m_iso = ON_Surface::W_iso;
    trim40.m_type = ON_BrepTrim::mated;
    trim40.m_tolerance[0] = SMALL_FASTF;
    trim40.m_tolerance[1] = SMALL_FASTF;
#endif

    return brep;
}


int
main(int argc, char *argv[])
{
    struct rt_wdb *wdbp = NULL;
    const char *name = "brep";
    ON_Brep *brep = NULL;
    int ret;

    if (argc > 0)
	bu_log("Usage: %s\n", argv[0]);

    bu_log("Breplicating...please wait...\n");

    ON_3dPoint points[8] = {
	/* left */
	ON_3dPoint(0.0, 0.0, 0.0), // 0
	ON_3dPoint(1.0, 0.0, 0.0), // 1
	ON_3dPoint(1.0, 0.0, 2.5), // 2
	ON_3dPoint(0.0, 0.0, 2.5), // 3
	/* right */
	ON_3dPoint(0.0, 1.0, 0.0), // 4
	ON_3dPoint(1.0, 1.0, 0.0), // 5
	ON_3dPoint(1.0, 1.0, 2.5), // 6
	ON_3dPoint(0.0, 1.0, 2.5), // 7
    };

    brep = generate_brep(8, points);
    if (!brep) {
	bu_exit(1, "ERROR: We don't have a BREP\n");
    }

    ON_TextLog log(stdout);

    brep->Dump(log);

    if (!brep->IsValid(&log)) {
	delete brep;
	bu_exit(1, "ERROR: We don't have a valid BREP\n");
    }

    brep->Dump(log);

    wdbp = wdb_fopen("breplicator.g");
    if (!wdbp) {
	delete brep;
	bu_exit(2, "ERROR: Unable to open breplicator.g\n");
    }
    mk_id(wdbp, "Breplicator test geometry");

    bu_log("Creating the BREP as BRL-CAD geometry\n");
    ret = mk_brep(wdbp, name, brep);
    if (ret) {
	delete brep;
	wdb_close(wdbp);
	bu_exit(3, "ERROR: Unable to export %s\n", name);
    }

    bu_log("Done.\n");

    delete brep;
    wdb_close(wdbp);

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
