/*                      Z I P P E R . C P P
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
/** @file Zipper.cpp
 *
 * Main logic for the zippering gaps module in the mesh healing module
 *
 */

#include "common.h"

/* interface header */
#include "./Zipper.h"

/* system implementation headers */
#include <stddef.h>
#include <climits>
#include <utility>
#include <vector>

/* local implementation headers */
#include "vmath.h"
#include "./Geometry.h"


void
calcFeaturePair(PolygonalMesh *polymesh, queue_element *node)
{
    DCEL_Edge *closest_edge;
    DCEL_Vertex *vertex = node->vertex;
    /* Vertices of the closest edge */
    DCEL_Vertex *v1, *v2;

    /* Get the closest edge */
    closest_edge = polymesh->findClosestEdge(vertex);
    if(closest_edge == NULL) {
	node->feature_edge = NULL;
	node->feature_vertex = NULL;
	node->dist = INT_MAX;
	return;
    }
    v1 = (polymesh->getVertex(closest_edge->edge_id.first));
    v2 = (polymesh->getVertex(closest_edge->edge_id.second));

    /* Check if orthogonal projection is possible, if yes set the feature_edge, else set the feature_vertex */
    if (isOrthoProjPossible(v1->coordinates, v2->coordinates, vertex->coordinates)) {
	node->feature_edge = closest_edge;
	node->dist = shortestDistToLine(v1->coordinates, v2->coordinates, vertex->coordinates);
	node->feature_vertex = NULL;
    }

    else {
	/* Returns the closest end point */
	node->feature_vertex = polymesh->findCloserVertex(closest_edge, vertex);
	node->dist = shortestDistBwPoints(vertex->coordinates, node->feature_vertex->coordinates);
	node->feature_edge = NULL;
    }
}

std::priority_queue<queue_element, std::vector<queue_element>, compareDist>
initPriorityQueue(PolygonalMesh *polymesh, DCEL_Edge *start, double tolerance)
{
    queue_element node;
    DCEL_Vertex *vertex;
    DCEL_Edge *edge;
    int v_id;

    /* Priority queue ordered based on the error measure */
    std::priority_queue<queue_element, std::vector<queue_element>, compareDist> PQ;

    if(start == NULL)
	return PQ;

    /* Check if the gap is in the shape of a square.
     * If yes pick a vertex and set it's feature vertex to the opposite vertex on the square.
     */
    int no_of_sides = 0;
    DCEL_Edge *trav_edge = start;

    do {
	no_of_sides++;
	trav_edge = trav_edge->next;
    }
    while (trav_edge != start);

    /* If it is a triangular hole, perform vertex contraction on the two closest vertices */
    if (no_of_sides == 3) {

	DCEL_Edge *temp = start, *feature = NULL;
	int min_dist = INT_MAX;

	for(int i = 0; i < 3; i++) {

	    if (isOrthoProjPossible(temp->origin->coordinates, temp->next->origin->coordinates, temp->previous->origin->coordinates)\
		    && shortestDistToLine(temp->origin->coordinates, temp->next->origin->coordinates, temp->previous->origin->coordinates) < min_dist) {
		feature = temp;
		min_dist = shortestDistToLine(temp->origin->coordinates, temp->next->origin->coordinates, temp->previous->origin->coordinates);
	    }
	    temp = temp->next;
	}

	if (!feature)
	    return PQ;

	node.vertex = feature->previous->origin;
	node.feature_edge = feature;
	node.feature_vertex = NULL;
	node.dist = min_dist;

	while (!PQ.empty()) {
	    PQ.pop();
	}

	PQ.push(node);

	return PQ;
    }

    edge = start;
    do
    {
	/* Consider the origin v_id of each edge */
	v_id = edge->origin->vertex_id;

	vertex = polymesh->getVertex(v_id);
	node.vertex = vertex;
	calcFeaturePair(polymesh, &node);

	/* Push the queue element to the priority queue */
	if (node.dist < tolerance)
	    PQ.push(node);

	edge = edge->next;
    }
    while (edge != start);

    if (!PQ.empty()) {

	if (no_of_sides == 4) {
	    double vec1[3];
	    double vec2[3];
	    double vec3[3];

	    /* Check if there are two consecutive right angles */
	    for (int i = 0; i < 3; i++) {
		vec1[i] = start->origin->coordinates[i] - start->next->origin->coordinates[i];
		vec2[i] = start->next->origin->coordinates[i] - start->next->next->origin->coordinates[i];
		vec3[i] = start->next->next->origin->coordinates[i] - start->next->next->next->origin->coordinates[i];
	    }
	    if (orientation(vec1, vec2, vec3) == CW) {

		return PQ;
	    }

	    if (NEAR_0(DOT_PROD(vec1, vec2)) && NEAR_0(DOT_PROD(vec2, vec3))) {
		node.vertex = start->origin;
		node.feature_vertex = start->next->next->origin;
		node.feature_edge = NULL;
		node.dist = shortestDistBwPoints(node.vertex->coordinates, node.feature_vertex->coordinates);

		while (!PQ.empty()) {
		    PQ.pop();
		}

		PQ.push(node);

		return PQ;
	    }
	}
    }

    return PQ;
}

void
zipperGaps(PolygonalMesh *polymesh, double tolerance, std::priority_queue<queue_element, std::vector<queue_element>, compareDist> PQ)
{
    double *A, *B, *C;
    bool vertex_contract = false;
    int deleted_vertex_id = INT_MAX;

    /*queue_element *curr_element = new queue_element;*/
    queue_element curr_element;

    /* Temporary priority queue */
    std::priority_queue<queue_element, std::vector<queue_element>, compareDist> temp_PQ;

    if (PQ.empty()) {
	return;
    }

    /*int count = 0;*/

    while (!PQ.empty()) {

	curr_element = PQ.top();
	PQ.pop();
	/* Vertex has already been healed */
	if(NEAR_0(curr_element.dist))
	    continue;

	/* Check if the error measure is within the tolerance for zippering.
	 * If not break out of the loop, since all the error measures after this one are not going to be lesser than this one
	 */
	if (curr_element.dist > tolerance)
	    break;

	/* If the feature is a vertex, move both these vertices to the midpoint of the line connecting the two vertices */
	if (curr_element.feature_vertex != NULL) {
	    vertex_contract = true;

	    double coords[3];

	    /* Find the mid-point */
	    coords[0] = (curr_element.vertex->coordinates[0] + curr_element.feature_vertex->coordinates[0]) / 2;
	    coords[1] = (curr_element.vertex->coordinates[1] + curr_element.feature_vertex->coordinates[1]) / 2;
	    coords[2] = (curr_element.vertex->coordinates[2] + curr_element.feature_vertex->coordinates[2]) / 2;

	    /* Changing the coordinates of the vertex to the midpoint of the vertex and its feature pair */
	    polymesh->setVertexCoordsInRecord(curr_element.vertex->vertex_id, coords);

	    /* Replace all references of the feature vertex with that of the vertex on the free edge chain */
	    polymesh->setVertexRecord(curr_element.vertex, curr_element.feature_vertex);

	    /* Delete the feature vertex's record */
	    deleted_vertex_id = curr_element.feature_vertex->vertex_id;

	    polymesh->deleteVertexRecord(curr_element.feature_vertex->vertex_id);

	    if (curr_element.vertex->vertex_id > curr_element.feature_vertex->vertex_id) {
		polymesh->checkAndSetTwins(polymesh->getVertex(curr_element.vertex->vertex_id - 1));
	    } else {
		polymesh->checkAndSetTwins(polymesh->getVertex(curr_element.vertex->vertex_id));
	    }

	}

	/* Else if the feature is an edge, project the vertex orthogonally onto the edge and split the edge and its incident face at the point */
	else if (curr_element.feature_edge != NULL) {

	    double D[3];

	    vertex_contract = false;

	    A = polymesh->getVertex(curr_element.feature_edge->edge_id.first)->coordinates;
	    B = polymesh->getVertex(curr_element.feature_edge->edge_id.second)->coordinates;
	    C = polymesh->getVertex(curr_element.vertex->vertex_id)->coordinates;

	    findOrthogonalProjection(A, B, C, D);

	    polymesh->setVertexCoordsInRecord(curr_element.vertex->vertex_id, D);

	    std::pair<int, int> fe_id;
	    fe_id.first = curr_element.feature_edge->edge_id.first;
	    fe_id.second = curr_element.feature_edge->edge_id.second;

	    int face_id = curr_element.feature_edge->twin->incident_face->face_id;

	    /* Insert a vertex with coordinates D on the feature edge. */
	    polymesh->insertVertexOnEdge(polymesh->getVertex(curr_element.vertex->vertex_id), curr_element.feature_edge);

	    int index = polymesh->getEdgeIndex(std::make_pair(fe_id.second, curr_element.vertex->vertex_id), face_id);

	    curr_element.feature_edge = polymesh->getEdgeWithIndex(index);

	    /* Split the face incident on the half edge of the feature edge with a bounded face incident */
	    polymesh->splitFace(curr_element.feature_edge->incident_face->face_id, \
	    polymesh->getVertex(curr_element.vertex->vertex_id), curr_element.feature_edge->previous->origin);

	    polymesh->checkAndSetTwins(curr_element.vertex);
	}

	if (PQ.empty()) {
	    return;
	}

	/* Testing here
	count++;
	if (count == 6)
	    break;*/

	/* To account for:
	 * 1. Those vertices with invalid elements
	 * 2. Those vertices whose coordinates have been changed, and hence the error measure - recalculate for all measures
	 */
	queue_element *element = new queue_element;
	do {
	    *element = PQ.top();
	    PQ.pop();

	    /* To account for the vertex deletion in the vertex contraction process */
	    if (vertex_contract && element->vertex->vertex_id == deleted_vertex_id) {
		continue;
	    }
	    if(vertex_contract && element->vertex->vertex_id > deleted_vertex_id) {
		element->vertex = polymesh->getVertex(element->vertex->vertex_id - 1);
	    }

	    calcFeaturePair(polymesh, element);

	    if (element->dist < tolerance)
		temp_PQ.push(*element);
	}
	while (!PQ.empty());

	delete element;

	while (!temp_PQ.empty()) {
	    PQ.push(temp_PQ.top());
	    temp_PQ.pop();
	}
    }
}

int
isValidFeature(PolygonalMesh *polymesh, queue_element *node)
{
    if (node->feature_vertex != NULL) {
	if (polymesh->getVertex(node->feature_vertex->vertex_id) == NULL) {
	    return 0;
	}
	return 1;
    }

    else if(node->feature_edge != NULL){
	if (polymesh->getEdge(std::make_pair(node->feature_edge->edge_id.first, \
			node->feature_edge->edge_id.second)) == NULL)
	    return 0;
	return 1;
    }
    return 1;
}

bool
checkIfValidPQ(std::priority_queue<queue_element, std::vector<queue_element>, compareDist> PQ)
{
    while(!PQ.empty()) {
	if (NEAR_EQUAL(PQ.top().dist, INT_MAX, SMALL_FASTF))
	    return true;
	PQ.pop();
    }
    return false;
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
