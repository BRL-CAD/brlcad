/*                      S T I T C H . C P P
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
/** @file Stitch.cpp
 *
 * Main logic for the stitching module in the mesh healing module
 *
 */

#include "common.h"

/* interface header */
#include "./Stitch.h"

/* system implementation headers */
#include <stddef.h>
#include <algorithm>
#include <utility>

/* local implementation headers */
#include "./Geometry.h"


void
stitchGaps(PolygonalMesh *polymesh, DCEL_Edge* start_A, DCEL_Edge* start_B, double tolerance)
{
    /* The edges that will be used to traverse both meshes, according to the advancing rule */
    DCEL_Edge *edge_A = start_A, *edge_B = start_B;

    DCEL_Vertex *first_A, *first_B;
    DCEL_Vertex *next_A, *next_B;
    int flag_A = 0, flag_B = 0;

    double perimeter_A, perimeter_B;

    do {

	first_A = polymesh->getVertex(edge_A->edge_id.first);
	first_B = polymesh->getVertex(edge_B->edge_id.second);

	next_A = polymesh->getVertex(edge_A->edge_id.second);
	next_B = polymesh->getVertex(edge_B->edge_id.first);

	perimeter_A = perimeter(first_A->coordinates, next_A->coordinates, first_B->coordinates);
	perimeter_B = perimeter(first_B->coordinates, next_B->coordinates, first_A->coordinates);

	if (perimeter_A < perimeter_B) {
	    /* Add triangle comprising the vertices first_A, next_A, and first_B, and
	     * Advance the edge pointer on chain A
	     */
	    addFace(polymesh, first_A, next_A, first_B, edge_A->previous, edge_A->next, edge_B->previous, edge_B, tolerance);
	    edge_A = edge_A->next;
	    if (edge_A == start_A)
		flag_A = 1;

	} else {
	    /* Add triangle comprising the vertices first_B, next_B, and first_A, and
	     * Advance the edge pointer on chain B
	     */
	    addFace(polymesh, next_B, first_B, first_A, edge_B->previous, edge_A->next, edge_A->previous, edge_A, tolerance);
	    edge_B = edge_B->previous;
	    if (edge_B == start_B)
		flag_B = 1;
	}
    }
    while (!(edge_A == start_A && flag_A) && !(edge_B == start_B && flag_B));

    if (edge_A == start_A) {

	while (edge_B != start_B) {
	    /* Make triangles with the the ending vertex in the A chain
	     * And advance the edge pointer on the B chain till it reaches the end of the chain
	     */
	    /*addFace(polymesh, next_B, first_B, edge_A->origin, edge_A->previous, );*/
	    edge_B = edge_B->next;
	}
    }

    else if (edge_B == start_B) {
	while (edge_A != start_A) {
	    /* Make triangles with the the ending vertex in the A chain
	     * And advance the edge pointer on the B chain till it reaches the end of the chain
	     */

	    edge_A = edge_A->previous;
	}
    }
}

void
addFace(PolygonalMesh *polymesh, DCEL_Vertex* first_vertex_A, DCEL_Vertex* second_vertex_A, DCEL_Vertex* first_vertex_B, \
	DCEL_Edge *prev_A, DCEL_Edge *next_A, DCEL_Edge *prev_B, DCEL_Edge *next_B, double tolerance)
{
    if (std::max(shortestDistBwPoints(first_vertex_A->coordinates, first_vertex_B->coordinates), \
	    shortestDistBwPoints(second_vertex_A->coordinates, first_vertex_B->coordinates)) > tolerance)
	return;

    if (!polymesh->checkEligibleEdge(first_vertex_A, first_vertex_B->coordinates))
	return;
    if (!polymesh->checkEligibleEdge(first_vertex_B, first_vertex_A->coordinates))
	return;
    if (!polymesh->checkEligibleEdge(second_vertex_A, first_vertex_B->coordinates))
	return;
    if (!polymesh->checkEligibleEdge(first_vertex_B, second_vertex_A->coordinates))
	return;

    DCEL_Edge *existing_edge = polymesh->getEdge(std::make_pair(first_vertex_A->vertex_id, second_vertex_A->vertex_id));

    DCEL_Edge *b_edge_btoa = polymesh->getEdge(std::make_pair(first_vertex_B->vertex_id, first_vertex_A->vertex_id));
    DCEL_Edge *ub_edge_atob = NULL;

    /* Adding half-edges from the first vertex of chain A to the vertex on chain B, if they are not already present */
    if (b_edge_btoa == NULL) {
	ub_edge_atob = polymesh->addEdge(std::make_pair(first_vertex_A->vertex_id, first_vertex_B->vertex_id), first_vertex_A, \
		NULL, polymesh->getFace(UNBOUNDED_FACE), prev_A, next_B);
	b_edge_btoa = polymesh->addEdge(std::make_pair(first_vertex_B->vertex_id, first_vertex_A->vertex_id), first_vertex_B, \
		ub_edge_atob, NULL, existing_edge, NULL);
    }

    DCEL_Edge *b_edge_atob = polymesh->getEdge(std::make_pair(second_vertex_A->vertex_id, first_vertex_B->vertex_id));
    DCEL_Edge *ub_edge_btoa = NULL;

    if (b_edge_atob == NULL) {
	ub_edge_btoa = polymesh->addEdge(std::make_pair(first_vertex_B->vertex_id, second_vertex_A->vertex_id), first_vertex_B, \
		NULL, polymesh->getFace(UNBOUNDED_FACE), next_A, prev_B);
	b_edge_atob = polymesh->addEdge(std::make_pair(second_vertex_A->vertex_id, first_vertex_B->vertex_id), second_vertex_A, \
		ub_edge_btoa, NULL, NULL, existing_edge);
    }
    if (ub_edge_atob != NULL)
	ub_edge_atob->twin = b_edge_btoa;

    if (ub_edge_btoa != NULL)
    ub_edge_btoa->twin = b_edge_atob;

    b_edge_atob->next = b_edge_btoa;
    b_edge_btoa->previous = b_edge_atob;

    /* Add face record for the new face */
    int new_face = polymesh->addFaceRecord(b_edge_atob);

    b_edge_atob->incident_face = polymesh->getFace(new_face);
    b_edge_btoa->incident_face = polymesh->getFace(new_face);
    existing_edge->incident_face = polymesh->getFace(new_face);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

