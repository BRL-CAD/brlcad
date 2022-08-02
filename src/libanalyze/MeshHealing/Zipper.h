/*                        Z I P P E R . H
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
/** @file Zipper.h
 *
 * Definitions of the utility functions for the zippering module
 *
 */

#ifndef SRC_LIBANALYZE_MESHHEALING_ZIPPER_H_
#define SRC_LIBANALYZE_MESHHEALING_ZIPPER_H_

#include "common.h"

#include <queue>
#include <vector>

#include "./MeshConversion.h"


struct queue_element {
    DCEL_Vertex *vertex;

    /* Either vertex feature or the edge feature will be set.
     * Feature is the closest edge.
     * If an orthogonal projection of the vertex on the edge is possible, the edge feature will be set.
     * Else, the vertex feature will be set to the closer end point of the edge.
     */
    DCEL_Vertex *feature_vertex;
    DCEL_Edge *feature_edge;

    /* Distance between the vertex and the feature - the error measure*/
    double dist;
};

/* Operator to compare the error measures of two feature pairs */
struct compareDist {
    bool operator()(queue_element& lhs, queue_element& rhs)
    {
	return lhs.dist > rhs.dist;
    }
};

/* Calculate error measures for the vertices on the free edge chain and push the queue_element variable to the priority queue */
std::priority_queue<queue_element, std::vector<queue_element>, compareDist> initPriorityQueue(PolygonalMesh *polymesh, DCEL_Edge *start, double tol);

/* Take the priority queue and zippers all those vertices whose error measures fall within the tolerance for zippering */
void zipperGaps(PolygonalMesh *polymesh, double tolerance, std::priority_queue<queue_element, std::vector<queue_element>, compareDist> PQ);

/* Check if the queue element has a valid feature */
int isValidFeature(PolygonalMesh *polymesh, queue_element *node);

/* Calculates the feature pair of a queue element */
void calcFeaturePair(PolygonalMesh *polymesh, queue_element *node);

/* Checks whether at least one node in the Priority queue has a valid feature pair */
bool checkIfValidPQ(std::priority_queue<queue_element, std::vector<queue_element>, compareDist> PQ);

#endif /* SRC_LIBANALYZE_MESHHEALING_ZIPPER_H_ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
