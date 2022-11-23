/*              M E S H C O N V E R S I O N . C P P
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
/** @file MeshConversion.cpp
 *
 * Main logic for the mesh healing portable module conversion
 *
 */

#include "common.h"

/* interface header */
#include "./MeshConversion.h"

/* system implementation headers */
#include <cstddef>
#include <iterator>
#include <limits>

/* local implementation headers */
#include "./Geometry.h"


/* Vertex record function */

void
PolygonalMesh::initIncidentEdge()
{
    unsigned int num_vertices = this->getNumVertices();

    for (unsigned int i = 0; i < num_vertices; ++i) {
	for (unsigned int j = 0; j < edgelist.size(); ++j) {

	    if (vertexlist[i].vertex_id == edgelist[j].origin->vertex_id) {
		vertexlist[i].incident_edge = &(edgelist[j]);
		break;
	    }
	}
    }
}

/* Face record function */

void
PolygonalMesh::initFaces()
{
    unsigned int num_faces = this->getNumFaces();
    DCEL_Face face;
    for (unsigned int i = 0; i <= num_faces; i++) {
	face.face_id = i;
	face.start_edge = NULL;
	facelist.push_back(face);
    }
}

/* Edge record functions */

void
PolygonalMesh::initTwinEdge()
{
    for (unsigned int i = 0; i < edgelist.size(); ++i) {
	for (unsigned int j = 0; j < edgelist.size(); j++) {
	    if (edgelist[i].edge_id.first == edgelist[j].edge_id.second && \
	    		edgelist[j].edge_id.first == edgelist[i].edge_id.second) {

		edgelist[i].twin = &(edgelist[j]);
		edgelist[j].twin = &(edgelist[i]);
		break;
	    }
	}
    }
}

/* Getters and setters */

DCEL_Vertex*
PolygonalMesh::getVertex(int ID)
{
    if (ID < (signed int)vertexlist.size())
	return &vertexlist[ID];
    return NULL;
}

void
PolygonalMesh::setVertexCoordsInRecord(int ID, double coordinates[3])
{
    if (ID < (signed int)vertexlist.size()) {
	vertexlist[ID].coordinates[0] = coordinates[0];
	vertexlist[ID].coordinates[1] = coordinates[1];
	vertexlist[ID].coordinates[2] = coordinates[2];

	setVertexCoords(ID);
    }
}

void
PolygonalMesh::setVertexRecord(DCEL_Vertex *vertex, DCEL_Vertex *vertex_to_be_replaced)
{
    for (unsigned int i = 0; i < edgelist.size(); ++i) {
	/* Edges whose origin is the vertex to be replaced */
	if (edgelist[i].origin->vertex_id == vertex_to_be_replaced->vertex_id) {
	    edgelist[i].origin = getVertex(vertex->vertex_id);
	    edgelist[i].edge_id.first = vertex->vertex_id;
	}

	/* Edges that end at the vertex to be replaced */
	if (edgelist[i].edge_id.second == vertex_to_be_replaced->vertex_id) {
	    edgelist[i].edge_id.second = vertex->vertex_id;
	}
    }
    setVertex(vertex->vertex_id, vertex_to_be_replaced->vertex_id);
}

void
PolygonalMesh::deleteVertexRecord(int ID)
{
    if ((unsigned)ID >= vertexlist.size())
	return;

    /* Setting the references for the origin of edges whose origin comes after the vertex being deleted */
    for (unsigned int i = 0; i < edgelist.size(); i++) {
	if (edgelist[i].origin->vertex_id > ID) {
	    edgelist[i].origin = &vertexlist[edgelist[i].origin->vertex_id - 1];
	}
    }

    /* Delete the vertex record with vertex id=ID. */

    vertexlist.erase(vertexlist.begin() + ID);
    setNumVertices();

    for (unsigned int i = 0; i < edgelist.size(); i++) {
	if (edgelist[i].edge_id.first > ID)
	    edgelist[i].edge_id.first--;

	if (edgelist[i].edge_id.second > ID)
	    edgelist[i].edge_id.second--;
    }

    /* Reassigning vertex IDs to the vertices */
    for (unsigned int i = ID; i < vertexlist.size(); i++) {
	vertexlist[i].vertex_id = i;
    }
}

void
PolygonalMesh::insertVertexOnEdge(DCEL_Vertex *vertex, DCEL_Edge *edge)
{
    /* edge11 and edge12 make up the edge passed as argument. edge21 and edge 22 make up the twin_edge */
    DCEL_Edge *edge11, *edge12, *edge21, *edge22;

    DCEL_Edge *twin_edge;

    edge11 = addEdge(std::make_pair(edge->edge_id.first, vertex->vertex_id), edge->origin, NULL, edge->incident_face, NULL, edge->previous);
    
    edge12 = addEdge(std::make_pair(vertex->vertex_id, edge->edge_id.second), vertex, NULL, edge->incident_face, edge->next, edge11);

    edge11->next = edge12;

    vertex->incident_edge = edge12;

    /* If the edge was the starting edge of its incident face, change it to either one of the two new edges */
    if (edge->incident_face->face_id != UNBOUNDED_FACE) {
	if (edge->incident_face->start_edge->edge_id.first == edge->edge_id.first)
	    edge->incident_face->start_edge = edge11;
    }

    twin_edge = edge->twin;

    edge21 = addEdge(std::make_pair(twin_edge->edge_id.first, vertex->vertex_id), twin_edge->origin, NULL, twin_edge->incident_face, NULL, twin_edge->previous);

    edge22 = addEdge(std::make_pair(vertex->vertex_id, twin_edge->edge_id.second), vertex, NULL, twin_edge->incident_face, twin_edge->next, edge21);

    edge21->next = edge22;


    /* If the edge was the starting edge of its incident face, change it to either one of the two new edges */
    if (twin_edge->incident_face->face_id != UNBOUNDED_FACE) {
	if (twin_edge->incident_face->start_edge->edge_id.first == twin_edge->edge_id.first)
	    twin_edge->incident_face->start_edge = edge21;
    }

    edge11->twin = edge22;
    edge12->twin = edge21;
    edge21->twin = edge12;
    edge22->twin = edge11;

    /* Delete the argument edge and its twin */
    int v1 = edge->edge_id.first;
    int v2 = edge->edge_id.second;
    int face = edge->incident_face->face_id;
    int twin_face = edge->twin->incident_face->face_id;
    deleteEdge(std::make_pair(v1, v2), face);
    deleteEdge(std::make_pair(v2, v1), twin_face);

    /*checkAndSetTwins(vertex);*/
}

void
PolygonalMesh::deleteEdge(std::pair<int, int> ID, int face_id)
{
    int index = getEdgeIndex(ID, face_id);

    int e_id;

    for (unsigned int i = 0; i < vertexlist.size(); i++) {
	e_id = getEdgeIndex(vertexlist[i].incident_edge->edge_id, vertexlist[i].incident_edge->incident_face->face_id);
	if (e_id > index) {
	    vertexlist[i].incident_edge = &edgelist[e_id - 1];
	}

	else if (e_id == index) {
	    for (unsigned int j = 0; j < edgelist.size(); j++) {
		if (vertexlist[i].vertex_id == edgelist[j].edge_id.first && j != (unsigned)index) {
		    vertexlist[i]. incident_edge = &edgelist[j];
		    break;
		}
	    }
	}
    }

    for (unsigned int i = 1; i < facelist.size(); i++) {
	e_id = getEdgeIndex(facelist[i].start_edge->edge_id, i);
	if (e_id > index) {
	    facelist[i].start_edge = &edgelist[e_id - 1];
	}
    }

    for (unsigned int i =0; i < edgelist.size(); i++) {
	e_id = getEdgeIndex(edgelist[i].twin->edge_id, edgelist[i].twin->incident_face->face_id);
	if (e_id > index) {
	    edgelist[i].twin = &edgelist[e_id - 1];
	}

	e_id = getEdgeIndex(edgelist[i].next->edge_id, edgelist[i].next->incident_face->face_id);
	if(e_id > index) {
	    edgelist[i].next = &edgelist[e_id - 1];
	}

	e_id = getEdgeIndex(edgelist[i].previous->edge_id, edgelist[i].previous->incident_face->face_id);
	if ( e_id > index) {
	    edgelist[i].previous = &edgelist[e_id - 1];
	}
    }

    if(index != -1) {
	edgelist.erase(edgelist.begin() + index);
	is_edge_checked.erase(is_edge_checked.begin() + index);
    }
}

void
PolygonalMesh::splitFace(int face_id, DCEL_Vertex *vertex_on_edge, DCEL_Vertex *opp_vertex)
{
    DCEL_Edge *start_edge1 = NULL, *start_edge2 = NULL;
    DCEL_Edge *new_edge1, *new_edge2;
    /* Indices of the newly added faces */
    int face1, face2;

    /* Find the two starting edges of the resultant faces */
    for (unsigned int i = 0; i < edgelist.size(); ++i) {

	if (edgelist[i].incident_face->face_id == face_id) {

	    if (edgelist[i].edge_id.first == vertex_on_edge->vertex_id)
		start_edge1 = &edgelist[i];
	    else if (edgelist[i].edge_id.second== vertex_on_edge->vertex_id)
		start_edge2 = &edgelist[i];
	}

	if (start_edge1 != NULL && start_edge2 != NULL)
	    break;
    }

    /* Don't keep trying if we didn't get our starting edges */
    if (!start_edge1 || !start_edge2) return;

    start_edge1->next->previous = start_edge1;
    start_edge2->previous->next = start_edge2;

    /* Create two half edges from the vertex passed as argument and the vertex opposite to the edge it is present on */
    new_edge1 = addEdge(std::make_pair(opp_vertex->vertex_id, vertex_on_edge->vertex_id), opp_vertex, NULL, NULL, start_edge1, start_edge1->next);
    new_edge2 = addEdge(std::make_pair(vertex_on_edge->vertex_id, opp_vertex->vertex_id), vertex_on_edge, NULL, NULL, start_edge2->previous, start_edge2);

    new_edge1->twin = new_edge2;
    new_edge2->twin = new_edge1;

    face1 = addFaceRecord(start_edge1);
    face2 = addFaceRecord(start_edge2);

    start_edge1->incident_face = &facelist[face1];
    new_edge1->incident_face = &facelist[face1];
    start_edge1->next->incident_face = &facelist[face1];

    start_edge2->incident_face = &facelist[face2];
    new_edge2->incident_face = &facelist[face2];
    start_edge2->previous->incident_face = &facelist[face2];

    deleteFaceRecord(face_id);
}

int
PolygonalMesh::addFaceRecord(DCEL_Edge *start_edge)
{
    int last_face_id = facelist.back().face_id;
    DCEL_Face new_face;
    new_face.face_id = last_face_id + 1;
    new_face.start_edge = start_edge;

    facelist.push_back(new_face);

    /*addFace();*/

    return last_face_id + 1;
}

DCEL_Edge*
PolygonalMesh::addEdge(std::pair<int, int> edge_id, DCEL_Vertex *origin, DCEL_Edge *twin, DCEL_Face *inc_face, DCEL_Edge *next, DCEL_Edge *prev)
{
    DCEL_Edge *new_edge = new DCEL_Edge;

    new_edge->edge_id.first = edge_id.first;
    new_edge->edge_id.second = edge_id.second;

    new_edge->origin = origin;

    new_edge->twin = twin;
    if (twin != NULL)
	twin->twin = new_edge;

    new_edge->incident_face = inc_face;

    new_edge->next = next;
    if (next != NULL)
	new_edge->next->previous = new_edge;

    new_edge->previous = prev;
    if (prev != NULL)
	new_edge->previous->next = new_edge;

    edgelist.push_back(*new_edge);
    is_edge_checked.push_back(false);

    delete new_edge;

    if (twin != NULL)
	twin->twin = &edgelist[edgelist.size() - 1];
    if (next != NULL)
	next->previous = &edgelist[edgelist.size() - 1];
    if (prev != NULL)
	prev->next = &edgelist[edgelist.size() - 1];

    return &edgelist[edgelist.size() - 1];
}

void
PolygonalMesh::deleteFaceRecord(int face_id)
{
    /*deleteFace(face_id - 1);*/
    
    /* Setting incident face references */
    for (unsigned int i = 0; i < edgelist.size(); i++) {
	if (edgelist[i].incident_face->face_id > face_id) {
	    edgelist[i].incident_face = &facelist[edgelist[i].incident_face->face_id - 1];
	}
    }
    facelist.erase(facelist.begin() + face_id);

    /* Reassigning face IDs */
    for (unsigned int i = face_id; i < facelist.size(); i++) {
	facelist[i].face_id = i;
    }
}

DCEL_Edge*
PolygonalMesh::getEdge(std::pair<int, int> ID)
{
    for (unsigned int i = 0; i < edgelist.size(); i++) {
	if (edgelist[i].edge_id.first == ID.first && edgelist[i].edge_id.second == ID.second)
	    return &edgelist[i];
    }
    return NULL;
}

int
PolygonalMesh::addVertexRecord(double coordinates[3])
{
    int last_vertex_id = vertexlist[vertexlist.size() - 1].vertex_id;
    DCEL_Vertex new_vertex = DCEL_VERTEX_NULL;
    new_vertex.vertex_id = last_vertex_id + 1;

    new_vertex.coordinates[0] = coordinates[0];
    new_vertex.coordinates[1] = coordinates[1];
    new_vertex.coordinates[2] = coordinates[2];

    vertexlist.push_back(new_vertex);

    addVertex(new_vertex.vertex_id);

    return last_vertex_id + 1;
}

DCEL_Edge*
PolygonalMesh::findClosestEdge(DCEL_Vertex *vertex)
{
    double min_dist = std::numeric_limits<int>::max();
    double dist;
    double ortho_proj[3];
    DCEL_Edge *edge = NULL;
    DCEL_Vertex *closer_vertex;
    bool is_eligible;
    DCEL_Face *face;

    for (unsigned int i = 0; i < edgelist.size(); ++i) {
    is_eligible = true;
	/* The edge should have the unbounded face incident on it */
	if (edgelist[i].incident_face->face_id != UNBOUNDED_FACE)
	    continue;

	/* The edge should not be incident on the vertex itself */
	if (edgelist[i].edge_id.first == vertex->vertex_id || edgelist[i].edge_id.second == vertex->vertex_id)
	    continue;

	/* Line joining the vertex to the edge should not cross any bounded face */

	/* The feature edge is an edge if an orthogonal projection is possible */
	if(isOrthoProjPossible(getVertex(edgelist[i].edge_id.first)->coordinates, \
			getVertex(edgelist[i].edge_id.second)->coordinates, vertex->coordinates)){
		/* If the face incident on this edge's twin contains the vertex, it will cross a bounded face */
			face = edgelist[i].twin->incident_face;
			if (isVertexInFace(vertex->vertex_id, face->face_id))
			continue;

	    findOrthogonalProjection(getVertex(edgelist[i].edge_id.first)->coordinates, \
	    getVertex(edgelist[i].edge_id.second)->coordinates, vertex->coordinates, ortho_proj);

	    /* Check if the line from the vertex to the edge intersects the face incident on the edge's twin,
	     * or any of the faces around the vertex
	     */
	    if(!checkEligibleEdge(vertex, ortho_proj))
	    	continue;
	    if (doesLineIntersectFace(edgelist[i].twin->incident_face, vertex, ortho_proj))
	    	continue;
	}

	/* The feature is a vertex */
	else {

	    closer_vertex = findCloserVertex(&edgelist[i], vertex);

	    /* If there exists an edge between the closer_vertex and vertex, make the current edge in-eligible */
	    for (unsigned int j = 0; j < edgelist.size(); j++) {
		if (edgelist[j].edge_id.first == vertex->vertex_id && edgelist[j].edge_id.second == closer_vertex->vertex_id) {
		    is_eligible = false;
		    break;
		}
	    }
	    if(!is_eligible)
		continue;

	    /* Check if the line joining vertex to closer_vertex crosses any other bounded face. Precisely:
	     * 1. Any face around the vertex
	     * 2. Any face around the closer vertex
	     */
	    if (!checkEligibleEdge(vertex, closer_vertex->coordinates))
		continue;
	    if(!checkEligibleEdge(closer_vertex, vertex->coordinates))
		continue;

	    for (int j = 0; j < 3; j++) {
		ortho_proj[j] = closer_vertex->coordinates[j];
	    }
	}

	/* The edge is now eligible to be the closest edge */
	dist = shortestDistBwPoints(vertex->coordinates, ortho_proj);

	if (dist < min_dist) {
	    min_dist = dist;
	    edge = &edgelist[i];
	}
    }

    return edge;
}

DCEL_Vertex*
PolygonalMesh::findCloserVertex(DCEL_Edge *edge, DCEL_Vertex *vertex)
{
    double dist1, dist2;
    DCEL_Vertex *vertex1 = getVertex(edge->edge_id.first);
    DCEL_Vertex *vertex2 = getVertex(edge->edge_id.second);

    dist1 = shortestDistBwPoints(vertex->coordinates, vertex1->coordinates);
    dist2 = shortestDistBwPoints(vertex->coordinates, vertex2->coordinates);

    if(dist1 < dist2)
	return vertex1;
    return vertex2;
}

DCEL_Face*
PolygonalMesh::getFace(int ID)
{
    return &facelist[ID];
}

bool
PolygonalMesh::isVertexInFace(int vertex_id, int face_id)
{
    DCEL_Edge *edge = (getFace(face_id))->start_edge;
    DCEL_Edge *trav_edge = edge;

    if (face_id == UNBOUNDED_FACE)
	return false;

    while (true) {
	if (trav_edge->edge_id.first == vertex_id)
	    return true;
	trav_edge = trav_edge->next;
	if (trav_edge == edge)
	    break;
    }
    return false;
}

bool
PolygonalMesh::isFreeEdge(DCEL_Edge* edge)
{
    if (edge->incident_face->face_id == UNBOUNDED_FACE)
	return true;

    return false;
}

int
PolygonalMesh::getEdgeIndex(std::pair<int, int> ID, int face_id)
{
    for (unsigned int i = 0; i < edgelist.size(); ++i) {
	if (edgelist[i].edge_id.first == ID.first && edgelist[i].edge_id.second == ID.second && \
	edgelist[i].incident_face->face_id == face_id) {
	    return i;
	}
    }
    return -1;
}

int
PolygonalMesh::getNumEdges()
{
    return edgelist.size();
}

DCEL_Edge*
PolygonalMesh::findFreeEdgeChain()
{
    DCEL_Edge *edge, *trav_edge;

    for (unsigned int i = 0; i < edgelist.size(); ++i) {
	if (!isFreeEdge(&edgelist[i]) || is_edge_checked[i])
	    continue;

	edge = &edgelist[i];
	trav_edge = edge;

	while (isFreeEdge(trav_edge)) {
	    is_edge_checked[getEdgeIndex(trav_edge->edge_id, trav_edge->incident_face->face_id)] = true;
	    trav_edge = trav_edge->next;
	    if (trav_edge == edge) {
		return trav_edge;
	    }
	}
    }
    return NULL;
}

bool
PolygonalMesh::doesLineIntersectFaceWithVertex(DCEL_Face *face, DCEL_Vertex *vertex, double other_vertex[3])
{
    if(face->face_id == UNBOUNDED_FACE)
	return false;

    DCEL_Edge *edge_to_be_checked = face->start_edge;

    /* Vertices of the end points of the opposite edge */
    DCEL_Vertex *v1, *v2;

    bool flag = false;

    for (int i = 0; i < VERTICES_PER_FACE; i++) {
	if (vertex->vertex_id != edge_to_be_checked->edge_id.first && vertex->vertex_id != edge_to_be_checked->edge_id.second) {
	    flag = true;
	    break;
	}
	edge_to_be_checked = edge_to_be_checked->next;
    }

    if(!flag) {

	return false;
    }

    v1 = getVertex(edge_to_be_checked->edge_id.first);
    v2 = getVertex(edge_to_be_checked->edge_id.second);

    if (checkIfIntersectsInterior(vertex->coordinates, other_vertex, v1->coordinates, v2->coordinates) && \
	    coplanar(vertex->coordinates, other_vertex, v1->coordinates, v2->coordinates))
    {
	return true;
    }

    /* Check if it is collinear with the other two edges */
    edge_to_be_checked = edge_to_be_checked->next;
    v1 = getVertex(edge_to_be_checked->edge_id.first);
    v2 = getVertex(edge_to_be_checked->edge_id.second);

    if (checkIfCollinear(vertex->coordinates, other_vertex, v1->coordinates, v2->coordinates) && \
	    coplanar(vertex->coordinates, other_vertex, v1->coordinates, v2->coordinates))
    {
	return true;
    }

    edge_to_be_checked = edge_to_be_checked->next;

    v1 = getVertex(edge_to_be_checked->edge_id.first);
    v2 = getVertex(edge_to_be_checked->edge_id.second);

    bool result = checkIfCollinear(vertex->coordinates, other_vertex, v1->coordinates, v2->coordinates);

    result = result && coplanar(vertex->coordinates, other_vertex, v1->coordinates, v2->coordinates);

    return result;
}

DCEL_Face*
PolygonalMesh::getNextFace(DCEL_Face *face, DCEL_Vertex *vertex)
{
    if(face->face_id == UNBOUNDED_FACE) {
	for (unsigned int i = 0; i < edgelist.size(); i++) {
	    if(edgelist[i].edge_id.first == vertex->vertex_id && edgelist[i].incident_face->face_id == UNBOUNDED_FACE)
		return edgelist[i].twin->incident_face;
	}
    }
    DCEL_Edge *edge = face->start_edge;
    DCEL_Edge *trav_edge = edge;
    do {
	if (trav_edge->edge_id.first == vertex->vertex_id) {
	    return trav_edge->twin->incident_face;
	}
	trav_edge = trav_edge->next;
    }
    while(trav_edge != edge);
    return NULL;
}

bool
PolygonalMesh::checkEligibleEdge(DCEL_Vertex *vertex, double closer_vertex[3])
{
    DCEL_Face *face, *trav_face;
    bool is_elgible = true;
    face = vertex->incident_edge->incident_face;
    trav_face = face;

    DCEL_Edge *edge;
    do {
	if (doesLineIntersectFaceWithVertex(trav_face, vertex, closer_vertex)) {
	    is_elgible = false;
	    break;
	}

	/* Check if orientation of the face is changing - i.e is the face getting turned over */
	if (trav_face->face_id != UNBOUNDED_FACE) {

	    edge = trav_face->start_edge;
	    while(edge->edge_id.first != vertex->vertex_id) {
		edge = edge->next;
	    }

	    if (orientation(closer_vertex, edge->next->origin->coordinates, edge->next->next->origin->coordinates) == CW) {
		is_elgible = false;
		break;
	    }
	}

	trav_face = getNextFace(trav_face, vertex);
    }
    while(trav_face != face);

    return is_elgible;
}

bool
PolygonalMesh::isEdgeOnChain(DCEL_Edge *edge_to_be_checked, DCEL_Edge *chain_edge)
{
    if(edge_to_be_checked == NULL)
	return false;
    DCEL_Edge *trav_edge = chain_edge;
    do {
	if(edge_to_be_checked == trav_edge)
	    return true;
	trav_edge = trav_edge->next;
    }
    while(trav_edge != chain_edge);
    return false;
}

void
PolygonalMesh::squeezeOutEdges(DCEL_Edge* edge, DCEL_Edge *next_edge)
{
    if (edge->incident_face->face_id != UNBOUNDED_FACE)
	return;

    edge->previous->next = next_edge->next;
    next_edge->next->previous = edge->previous;

    /* Delete the edge records */
    int v1 = next_edge->edge_id.first, v2 = next_edge->edge_id.second, face = next_edge->incident_face->face_id;

    deleteEdge(std::make_pair(edge->edge_id.first, edge->edge_id.second), edge->incident_face->face_id);
    deleteEdge(std::make_pair(v1, v2), face);

    /* If this edge is the incident edge of any vertex, change the reference to some other edge whose origin is this edge's origin */
    for (unsigned int i = 0; i < vertexlist.size(); i++) {
	if (vertexlist[i].vertex_id != vertexlist[i].incident_edge->edge_id.first) {
	    for (unsigned int j = 0; j < edgelist.size(); j++) {
		if (edgelist[j].edge_id.first == vertexlist[i].vertex_id && edgelist[j].incident_face->face_id != UNBOUNDED_FACE) {
		    vertexlist[i].incident_edge = &edgelist[j];
		    break;
		}
	    }
	}
    }
}

bool
PolygonalMesh::doesLineIntersectFace(DCEL_Face* face, DCEL_Vertex* vertex, double ortho_proj[3])
{
    if(face->face_id == UNBOUNDED_FACE)
	return false;

    DCEL_Edge *edge_to_be_checked = face->start_edge;
    DCEL_Edge *trav_edge = edge_to_be_checked;

    /* Vertices of the end points of the opposite edge */
    DCEL_Vertex *v1, *v2;

    do {
	v1 = getVertex(trav_edge->edge_id.first);
	v2 = getVertex(trav_edge->edge_id.second);

	if (checkIfIntersectsInterior(vertex->coordinates, ortho_proj, v1->coordinates, v2->coordinates) \
		&& coplanar(vertex->coordinates, ortho_proj, v1->coordinates, v2->coordinates))	{
	    return true;
	}
	trav_edge = trav_edge->next;
    }
    while(trav_edge != edge_to_be_checked);

    return false;
}

void
PolygonalMesh::checkAndSetTwins(DCEL_Vertex* vertex)
{
    DCEL_Edge *edge = vertex->incident_edge;
    DCEL_Edge *trav_edge = edge;
    DCEL_Edge *other_edge;
    bool flag1 = false, flag2 = false;
    std::pair<int, int> edge1_id, edge2_id;

    DCEL_Edge *edge1, *edge2;
    DCEL_Edge *fedge1 = NULL, *fedge2 = NULL;

    do {
	trav_edge = trav_edge->twin->next;
    }
    while (trav_edge->incident_face->face_id != UNBOUNDED_FACE && trav_edge != edge);

    if(trav_edge == edge && trav_edge->incident_face->face_id != UNBOUNDED_FACE)
	return;

    edge1 = trav_edge;
    edge1_id.first = edge1->edge_id.first;
    edge1_id.second = edge1->edge_id.second;

    edge2 = edge1->previous;
    edge2_id.first = edge2->edge_id.first;
    edge2_id.second = edge2->edge_id.second;

    if (edge1->next->edge_id.second == edge1->edge_id.first)
	flag1 = true;

    if (edge2->previous->edge_id.first == edge2->edge_id.second)
	flag2 = true;

    /* Look for twins, for the twins of these edges */
    for (unsigned int i = 0; i < edgelist.size(); i++) {

	if (!flag1 && !flag2) {

	    if (edgelist[i].incident_face->face_id == UNBOUNDED_FACE && edgelist[i].edge_id.first == vertex->vertex_id) {
		if (fedge1 != NULL) {
		    fedge2 = &edgelist[i];

		    DCEL_Edge *temp = fedge1->previous;

		    fedge1->previous = fedge2->previous;
		    fedge2->previous->next= fedge1;

		    fedge2->previous = temp;
		    temp->next = fedge2;

		    break;
		}

		else {
		    fedge1 = &edgelist[i];
		}
	    }
	}

	if (flag1) {
	    if (edgelist[i].edge_id.first == edge1->edge_id.first && edgelist[i].edge_id.second == edge1->edge_id.second &&\
	    edgelist[i].incident_face->face_id != UNBOUNDED_FACE) {

		edge1->twin->twin = &edgelist[i];
		edgelist[i].twin = edge1->twin;
		other_edge = edge1->next;
		squeezeOutEdges(edge1, other_edge);
		edge2 = &edgelist[getEdgeIndex(edge2_id, UNBOUNDED_FACE)];
		flag1 = false;

		if (!flag1 && !flag2)
		    break;
	    }
	}

	if (flag2) {
	    if (edgelist[i].edge_id.first == edge2->edge_id.first && edgelist[i].edge_id.second == edge2->edge_id.second &&\
		edgelist[i].incident_face->face_id != UNBOUNDED_FACE) {

		    edge2->twin->twin = &edgelist[i];
		    edgelist[i].twin = edge2->twin;
		    other_edge = edge2->previous;
		    squeezeOutEdges(other_edge, edge2);
		    edge1 = &edgelist[getEdgeIndex(edge1_id, UNBOUNDED_FACE)];
		    flag2 = false;

		    if (!flag1 && !flag2)
			break;
	    }
	}
    }
}

DCEL_Edge*
PolygonalMesh::getEdgeWithIndex(int index)
{
    if ((unsigned)index < edgelist.size())
	return &edgelist[index];
    return NULL;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

