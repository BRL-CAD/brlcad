/*                         G L O B . H
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
/** @file glob.h
 *
 * INTAVAL Target Geometry File to BRL-CAD converter:
 * global declarations
 *
 *  Origin -
 *	TNO (Netherlands)
 *	IABG mbH (Germany)
 */

#ifndef GLOB_INCLUDED
#define GLOB_INCLUDED

#include "common.h"

#include "vmath.h"


const size_t LINELEN       = 128;
const size_t MAX_TRIANGLES = 1000;
const size_t MAX_NPTS      = 3 * MAX_TRIANGLES;

typedef int I_Point_t[3];


struct Form {
    int    id;
    int    compnr;
    int    s_compnr; // surrounding component number
    size_t npts;

    union {
	I_Point_t pt[MAX_NPTS];

	struct {
	    size_t num_vertices;
	    size_t num_faces;
	    int vertices[MAX_NPTS * 3];   // points[3][num_vertices]
	    int faces[MAX_TRIANGLES * 3]; // faces[3][num_faces]
	} bot;
    };

    int tr_vec[3];
    int thickness;
    int width;
    int radius1;
    int radius2;
};


#endif // GLOB_INCLUDED
