/*                          D C E L . H
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
/** @file DCEL.h
 *
 * Structure definitions of the vertex, edge, and face records for the DCEL
 *
 */

#ifndef SRC_LIBANALYZE_MESHHEALING_DCEL_H_
#define SRC_LIBANALYZE_MESHHEALING_DCEL_H_

#include "common.h"

#include <utility>

struct DCEL_Edge;

/* DCEL_Vertex record of a doubly-connected DCEL_Edge list includes:
    1. DCEL_Vertex ID
    2. Coordinates
    3. An arbitrary incident DCEL_Edge - one with the particular DCEL_Vertex as its origin 
*/
struct DCEL_Vertex {
    int vertex_id;
    double coordinates[3];
    DCEL_Edge* incident_edge;
};
#define DCEL_VERTEX_NULL {0, {0.0, 0.0, 0.0}, NULL}

/* DCEL_Face record of a doubly-connected DCEL_Edge list includes:
    1. DCEL_Face ID,
    2. ID of any DCEL_Edge that can be used to traverse the DCEL_Face in counter-clockwise order
 */
struct DCEL_Face {
    int face_id;
    DCEL_Edge* start_edge;
};

/* The half-DCEL_Edge record of a doubly-connected DCEL_Edge list includes:
    1. DCEL_Edge ID
    2. ID of the origin DCEL_Vertex
    3. ID of the twin DCEL_Edge
    4. ID of the incident DCEL_Face - DCEL_Face to its left (since we orient the faces in counter-clockwise order)
    5. ID of the previous DCEL_Edge in the incident DCEL_Face
    6. ID of the next DCEL_Edge in the incident DCEL_Face
 */
struct DCEL_Edge {
    std::pair<int, int> edge_id;
    DCEL_Vertex* origin;
    DCEL_Edge* twin;
    DCEL_Face* incident_face;
    DCEL_Edge* next;
    DCEL_Edge* previous;
};

#endif /* SRC_LIBANALYZE_MESHHEALING_DCEL_H_ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
