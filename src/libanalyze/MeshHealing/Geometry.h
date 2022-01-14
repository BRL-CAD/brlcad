/*                      G E O M E T R Y . H
 * BRL-CAD
 *
 * Copyright (c) 2016-2022 United States Government as represented by
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
/** @file Geometry.h
 *
 * Definitions of the geometry functions required by the mesh healing module
 *
 */

#ifndef SRC_LIBANALYZE_MESHHEALING_GEOMETRY_H_
#define SRC_LIBANALYZE_MESHHEALING_GEOMETRY_H_

#include "common.h"

#include <cstdlib>


#define COLL 0
#define CW 1
#define CCW 2
#define DOT_PROD(A, B) \
    A[0] * B[0] \
    + A[1] * B[1] \
    + A[2] * B[2]

#define TOLERANCE (5.3e-11)
#define NEAR_0(val) (((val) > -TOLERANCE) && ((val) < TOLERANCE))

/* Finds the orthogonal projection of a point C on a line segment formed by points A and B */
void findOrthogonalProjection(const double  line_point1[3], const double  line_point2[3], const double  point[3], double  ortho[3]);

/* Returns true if orthogonal projection of C on the line segment AB is possible, false otherwise. */
bool isOrthoProjPossible(const double  line_point1[3], const double  line_point2[3], const double  point[3]);

/* Returns the shortest distance between the two points */
double shortestDistBwPoints(const double  A[3], const double  B[3]);

/* Returns the shortest distance between a point and a line segment */
double shortestDistToLine(const double  line_point1[3], const double  line_point2[3], const double  point[3]);

/* Checks if line segment AB and CD intersect */
bool checkIfIntersectsInterior(const double  line1_point1[3], const double  line1_point2[3], const double  line2_point1[3], const double  line2_point2[3]);

/* Checks if the two lines are made up of collinear points */
bool checkIfCollinear(const double  line1_point1[3], const double  line1_point2[3], const double  line2_point1[3], const double  line2_point2[3]);

/* To check the orientation of the triangle formed by the three points P, Q, and R
 * 0 - Collinear
 * 1 - Clockwise
 * 2 - Counter-clockwise*/
int orientation(const double  P[3], const double  Q[3], const double  R[3]);

/* Calculates the determinant of the 3x3 matrix m */
double calcDet(const double  m[3][3]);

/* Calculates the determinant of the 4x4 matrix m */
double calcDetFour(const double m[4][4]);

/* Checks if the four points are co-planar */
bool coplanar(const double[3], const double[3], const double[3], const double[3]);

/* Returns the 2D coordinates of the 3D point */
void twoDimCoords(const double coordinates[3], double twod_coords[3]);

/* Returns the perimeter of the triangle formed by the vertices A, B, and C */
double perimeter(const double  A[3], const double  B[3], const double  C[3]);

#endif /* SRC_LIBANALYZE_MESHHEALING_GEOMETRY_H_ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
