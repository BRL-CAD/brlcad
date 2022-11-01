/*                    G E O M E T R Y . C P P
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
/** @file Geometry.cpp
 *
 * Implementations of all the geometry functions required by the mesh healing module
 *
 */

#include "common.h"

/* interface header */
#include "./Geometry.h"

/* system implementation headers */
#include <cmath>


/* Calculates the determinant of a 3x3 matrix */
double
calcDet(const double m[3][3])
{
    double d;

    d = m[0][0] * (m[1][1]*m[2][2] - m[1][2] * m[2][1]);
    d -= m[0][1] * (m[1][0]*m[2][2] - m[1][2] * m[2][0]);
    d += m[0][2] * (m[1][0]*m[2][1] - m[1][1] * m[2][0]);

    return d;
}

void
findOrthogonalProjection(const double line_point1[3], const double line_point2[3], const double point[3], double ortho[3])
{
    /* Parameter in the line equation of AB */
    double t;

    double BA[3];
    double CA[3];

    for (int i = 0; i < 3; ++i) {
	BA[i] = line_point2[i] - line_point1[i];
	CA[i] = point[i] - line_point1[i];
    }

    /* Finding parameter t for a point D on line AB such that AB and DC are perpendicular to each other using the formula:
     * t = dot(B-A, C-A)/dot(B-A, B-A) */
    double num = DOT_PROD(BA, CA);
    double den = DOT_PROD(BA, BA);

    t = num / den;

    /* t=0 and t=1 correspond to the end points */
    if (t < 0)
	t = 0;
    else if (t > 1)
	t = 1;

    for (int i = 0; i < 3; ++i) {
	ortho[i] = line_point1[i] + t * BA[i];
    }
}

bool
isOrthoProjPossible(const double line_point1[3], const double line_point2[3], const double point[3])
{
    /* Parameter in the line equation of AB */
    double t;

    double BA[3];
    double CA[3];

    for (int i = 0; i < 3; ++i) {
	BA[i] = line_point2[i] - line_point1[i];
	CA[i] = point[i] - line_point1[i];
    }

    /* Finding parameter t for a point D on line AB such that AB and DC are perpendicular to each other using this formula:
     * t = dot(B-A, C-A)/dot(B-A, B-A)
     */
    double num = DOT_PROD(BA, CA);
    double den = DOT_PROD(BA, BA);

    t = num / den;

    if (t > 0 && t < 1)
	return true;

    return false;
}

double
shortestDistBwPoints(const double A[3], const double B[3])
{
    double dist = 0;
    for (int i = 0; i < 3; i++) {
	dist += (A[i] - B[i]) * (A[i] - B[i]);
    }
    dist = std::sqrt(dist);

    return dist;
}

double
shortestDistToLine(const double line_point1[3], const double line_point2[3], const double point[3])
{
    double dist;
    double ortho[3];
    findOrthogonalProjection(line_point1, line_point2, point, ortho);

    dist = shortestDistBwPoints(point, ortho);

    return dist;
}

bool
checkIfIntersectsInterior(const double line1_point1[3], const double line1_point2[3], const double line2_point1[3], const double line2_point2[3])
{
    int o1, o2, o3, o4;
    o1 = orientation(line1_point1, line1_point2, line2_point1);
    o2 = orientation(line1_point1, line1_point2, line2_point2);
    o3 = orientation(line2_point1, line2_point2, line1_point1);
    o4 = orientation(line2_point1, line2_point2, line1_point2);

    if (o1 != o2 && o3 != o4 && o1 != COLL && o2 != COLL && o3 != COLL && o4 != COLL)
	return true;

    return false;
}

bool
checkIfCollinear(const double line1_point1[3], const double line1_point2[3], const double line2_point1[3], const double line2_point2[3])
{
    int o1, o2, o3, o4;
    o1 = orientation(line1_point1, line1_point2, line2_point1);
    o2 = orientation(line1_point1, line1_point2, line2_point2);
    o3 = orientation(line2_point1, line2_point2, line1_point1);
    o4 = orientation(line2_point1, line2_point2, line1_point2);

    if(o1 == COLL && o2 == COLL && o3 == COLL && o4 == COLL)
	return true;
    return false;
}

int
orientation(const double A[3], const double B[3], const double C[3])
{
    double m[3][3];
    double det;

    for (int i = 0; i < 3; i++) {
	m[0][i] = A[i];
	m[1][i] = B[i];
	m[2][i] = C[i];
    }

    if(NEAR_0(m[0][2]) && NEAR_0(m[1][2]) && NEAR_0(m[2][2])) {
	m[0][2] = 1;
	m[1][2] = 1;
	m[2][2] = 1;
    }

/*
    for (int i = 0; i < 3; i++) {
	m[0][i] = 1;
	m[1][i] = B[i] - A[i];
	m[2][i] = C[i] - A[i];
    }
*/
    det = calcDet(m);

    if(NEAR_0(det))
	return COLL;
    else if (det > 0)
	return CCW;
    else
	return CW;
}

void
twoDimCoords(const double coordinates[3], double twod_coords[3])
{
	twod_coords[0] = coordinates[0];
	twod_coords[1] = coordinates[1];
	twod_coords[2] = 0;
}

bool
coplanar(const double A[3], const double B[3], const double C[3], const double D[3])
{
    double m[4][4];

    for(int i = 0; i < 3; i++) {
	m[0][i] = A[i];
	m[1][i] = B[i];
	m[2][i] = C[i];
	m[3][i] = D[i];
    }

    for(int i = 0; i < 4; i++) {
	m[i][3] = 1;
    }

    return NEAR_0(calcDetFour(m)) ? true : false;
}

double
perimeter(const double A[3], const double B[3], const double C[3])
{
    double dist;

    dist = shortestDistBwPoints(A, B);
    dist += shortestDistBwPoints(B, C);
    dist += shortestDistBwPoints(C, A);

    return dist;
}

double
calcDetFour(const double m[4][4])
{
    double det = 0;
    double temp[3][3];
    int col;

    for(int i = 0; i < 4; i++) {
	for(int j = 1; j < 4; j++) {
	    col = 0;

	    for(int k = 0; k < 4; k++) {
		if(k == i)
		    continue;

		temp[j - 1][col] = m[j][k];
		col++;
	    }
	}

	if(i % 2 == 0)
	    det += m[0][i]*calcDet(temp);
	else
	    det -= m[0][i]*calcDet(temp);
    }

    return det;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
