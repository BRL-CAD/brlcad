/*       M E S H C O N V E R S I O N _ B R L C A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2016-2021 United States Government as represented by
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
/** @file MeshConversion_brlcad.cpp
 *
 * Main logic for converting a bot to the PolygonalMesh object
 *
 */

#include "common.h"

/* interface header */
#include "./MeshConversion_brlcad.h"

/* system implementation headers */
#include <stddef.h>
#include <utility>
#include <vector>

/* local implementation headers */
#include <bu/defines.h>
#include <bu/malloc.h>
#include <rt/geom.h>
#include <rt/primitives/bot.h>
#include "./DCEL.h"
#include "./Geometry.h"


BrlcadMesh::BrlcadMesh(rt_bot_internal *bot_mesh)
{
    bot = bot_mesh;
    initVertices();
    initFaces();
    initEdges();
    initIncidentFace();
    initIncidentEdge();
    initStartEdge();
    initTwinEdge();
    initPrevEdge();
    //initNextEdge();
}

/* DCEL_Vertex record functions */

int
BrlcadMesh::getNumVertices()
{
    return bot->num_vertices;
}

void
BrlcadMesh::initVertices()
{
    unsigned int num_vertices = this->getNumVertices();
    DCEL_Vertex vertex;

    for (unsigned int i = 0; i < num_vertices; i++) {

    vertex.vertex_id = i;

    vertex.coordinates[0] = bot->vertices[VERTICES_PER_FACE * i];
    vertex.coordinates[1] = bot->vertices[VERTICES_PER_FACE * i + 1];
    vertex.coordinates[2] = bot->vertices[VERTICES_PER_FACE * i + 2];
    vertexlist.push_back(vertex);
    }
}

/* DCEL_Face record functions 
 * Note: The unbounded DCEL_Face will have face_id = 0. Its start_edge will be set to NULL.
 */

int
BrlcadMesh::getNumFaces()
{
    return bot->num_faces;
}

void
BrlcadMesh::initStartEdge()
{
    int vertex1, vertex2, vertex3;
    int v1, v2;
    unsigned int num_faces = this->getNumFaces();
    facelist[UNBOUNDED_FACE].start_edge = NULL;

    for (unsigned int i = 0; i < num_faces; ++i) {
	vertex1 = bot->faces[VERTICES_PER_FACE * i];
	vertex2 = bot->faces[VERTICES_PER_FACE * i + 1];
	vertex3 = bot->faces[VERTICES_PER_FACE * i + 2];

	if (bot->orientation == RT_BOT_CCW)
	    vertex2 = bot->faces[VERTICES_PER_FACE * i + 1];
	else if (bot->orientation == RT_BOT_CW)
	    vertex2 = bot->faces[VERTICES_PER_FACE * i + 2];
	else {
	    /* Determine the CCW orientation and set the vertex2 accordingly */

	    /* If the orientation is clockwise, switch the second and third vertices */
	    if (orientation(getVertex(vertex1)->coordinates, getVertex(vertex2)->coordinates, \
	    		getVertex(vertex3)->coordinates) != CCW)
		vertex2 = vertex3;
	}

	for (unsigned int j = 0; j < edgelist.size(); j++) {
	    v1 = edgelist[j].edge_id.first;
	    v2 = edgelist[j].edge_id.second;
	    
	    if (vertex1 == v1 && vertex2 == v2) {
		/* i+1 due to the presence of the unbounded face record */
		facelist[i + 1].start_edge = &(edgelist[j]);
		break;
	    }
	}
    }
}

/* DCEL_Edge record functions */

void
BrlcadMesh::initEdges()
{
    unsigned int num_faces = this->getNumFaces();
    int v1, v2;
    
    /* To check if the half-edges are already present */
    bool flag;

    for (unsigned int i = 0; i < num_faces; ++i) {

	DCEL_Edge temp;
	temp.edge_id = std::make_pair(-1,-1);
	temp.origin = NULL;
	temp.twin = NULL;
	temp.incident_face = NULL;
	temp.next = NULL;
	temp.previous = NULL;

	for (unsigned int j = 0; j < VERTICES_PER_FACE; ++j){
	    v1 = bot->faces[VERTICES_PER_FACE * i + j];
	    v2 = bot->faces[VERTICES_PER_FACE * i + (j + 1) % VERTICES_PER_FACE];

	    flag = false;

	    /* Checking if the DCEL_Edge from v1 to v2 is already present */
	    for (unsigned int k = 0; k < edgelist.size(); k++) {
		if (v1 == edgelist[k].edge_id.first && v2 == edgelist[k].edge_id.second)
		    flag = true;

		if (flag)
		    break;
	    }

	    if (!flag) {
		/* Setting the DCEL_Edge record for the half record from v1 to v2 */
		temp.origin = &(vertexlist[v1]);
		temp.edge_id = std::make_pair(v1, v2);
		edgelist.push_back(temp);

		/* Setting the DCEL_Edge record for the half record from v2 to v1 */
		temp.origin = &(vertexlist[v2]);
		temp.edge_id = std::make_pair(v2, v1);
		edgelist.push_back(temp);
	    }
	}
    }
}

void
BrlcadMesh::initIncidentFace()
{

    int vertex1, vertex2;
    unsigned int num_faces = this->getNumFaces();
    int v1, v2, v3, temp;
    bool face_set ;

    int *a = new int[2];
    int *b = new int[3];

    for (unsigned int i = 0; i < edgelist.size(); ++i) {
	face_set = false;

	vertex1 = edgelist[i].edge_id.first;
	vertex2 = edgelist[i].edge_id.second;

	a[0] = vertex1;
	a[1] = vertex2;

	for (unsigned int j = 0; j < num_faces; j++) {
	    v1 = bot->faces[VERTICES_PER_FACE * j];
	    v2 = bot->faces[VERTICES_PER_FACE * j + 1];
	    v3 = bot->faces[VERTICES_PER_FACE * j + 2];

	    if (bot->orientation == RT_BOT_CCW) {
		v2 = bot->faces[VERTICES_PER_FACE * i + 1];
		v3 = bot->faces[VERTICES_PER_FACE * i + 2];
	    }
	    else if (bot->orientation == RT_BOT_CW) {
		v2 = bot->faces[VERTICES_PER_FACE * i + 2];
		v3 = bot->faces[VERTICES_PER_FACE * i + 1];
	    }
	    else {

		/* If the orientation is clockwise, switch the second and third vertices */
		if(orientation(getVertex(v1)->coordinates, getVertex(v2)->coordinates, getVertex(v3)->coordinates) != CCW) {
		    temp = v2;
		    v2 = v3;
		    v3 = temp;
		}
	    }

	    b[0] = v1;
	    b[1] = v2;
	    b[2] = v3;

	    /* Check if a (vertex1 and vertex2) is present in the same order as b (the triangle made of v1, v2, v3) */
	    if (rt_bot_same_orientation(a, b)) {
		edgelist[i].incident_face = &(facelist[j + 1]);
		face_set = true;
		break;
	    }		
	}
	/* If no DCEL_Face has been set till here, set the unbounded DCEL_Face to be the incident DCEL_Face */
	if (!face_set) {
	    edgelist[i].incident_face = &(facelist[UNBOUNDED_FACE]);
	}
    }
    delete[] a;
    delete[] b;
}

void
BrlcadMesh::initPrevEdge()
{
    int inc_face, third_vertex;
    int vertex1, vertex2;

    for (unsigned int i = 0; i < edgelist.size(); i++) {

	/* inc_face has the face_id of the incident face for the current edge */
	inc_face = edgelist[i].incident_face->face_id;

	/* The vertex_id of the vertex list comprising the current half edge */
	vertex1 = edgelist[i].edge_id.first;
	vertex2 = edgelist[i].edge_id.second;

	/* If the face is the unbounded face, check in the edge list for the vertex with vertex1 as its end vertex,
	 * and has the unbounded face incident on it. */
	if(inc_face == UNBOUNDED_FACE) {
	    for(unsigned int j = 0; j < edgelist.size(); j++) {
		if(edgelist[j].edge_id.second == vertex1 && edgelist[j].incident_face->face_id == UNBOUNDED_FACE) {
		    edgelist[i].previous = &edgelist[j];
		    edgelist[j].next = &edgelist[i];
		    break;
		}
	    }
	}

	else {
	    for (unsigned int j = 0; j < VERTICES_PER_FACE; j++) {

		/*Finding the third vertex, that is one that is not vertex1 or vertex2 */
		third_vertex = bot->faces[VERTICES_PER_FACE * (inc_face - 1) + j];

		if (third_vertex != vertex1 && third_vertex != vertex2) {
		    /* Finding the edge_id of the edge from third_vertex to vertex1 */

		    DCEL_Edge *edge = getEdge(std::make_pair(third_vertex, vertex1));
		    edgelist[i].previous = edge;
		    edge->next = &edgelist[i];

		    break;
		}
	    }
	}
    }
}

void
BrlcadMesh::initNextEdge()
{

    int inc_face, third_vertex;
    int vertex1, vertex2;

    for (unsigned int i = 0; i < edgelist.size(); i++) {

	/* inc_face has the face_id of the incident face for the current edge */
	inc_face = edgelist[i].incident_face->face_id;

	/* The vertex_id of the vertices comprising the current half edge */
	vertex1 = edgelist[i].edge_id.first;
	vertex2 = edgelist[i].edge_id.second;

	/* If the face is the unbounded face, check in the edge list for the vertex with vertex 1 as its end vertex,
	 * and has the unbounded face incident on it. */
	if(inc_face == UNBOUNDED_FACE) {
	    for(unsigned int j = 0; j < edgelist.size(); j++) {
		if(edgelist[j].edge_id.first == vertex2 && edgelist[j].incident_face->face_id == UNBOUNDED_FACE) {
		    edgelist[i].next = &edgelist[j];
		    break;
		}
	    }
	} else {

	    third_vertex = -1;
	    for (unsigned int j = 0; j < VERTICES_PER_FACE; j++) {

		/*Finding the third vertex, that is one that is not vertex1 or vertex2 */
		if (bot->faces[VERTICES_PER_FACE * (inc_face - 1) + j] != vertex1 && bot->faces[VERTICES_PER_FACE * (inc_face - 1) + j] != vertex2) {
		    third_vertex = bot->faces[VERTICES_PER_FACE * (inc_face - 1) + j];
		    break;
		}
	    }

	    /* Finding the edge_id of the edge from vertex2 to third_vertex */
	    if (third_vertex != -1)
		edgelist[i].next = getEdge(std::make_pair(vertex2, third_vertex));
	}
    }
}

void
BrlcadMesh::setNumVertices()
{
    bot->num_vertices = vertexlist.size();
}

void
BrlcadMesh::setNumFaces()
{
    bot->num_faces = facelist.size();
}

void
BrlcadMesh::setVertices()
{
    bu_free(bot->vertices, "vertices");
    bot->vertices = (fastf_t*)bu_malloc(sizeof(fastf_t) * vertexlist.size() * 3, "alloc vertices");
    bot->num_vertices = vertexlist.size();

    for (unsigned int i = 0; i < vertexlist.size(); i++) {
	for (int k = 0; k < 3; k++)
	    bot->vertices[3 * i + k] = vertexlist[i].coordinates[k];
    }
}

void
BrlcadMesh::setFaces()
{
    DCEL_Edge *edge;

    bot->faces = (int*)bu_realloc(bot->faces, 3* (facelist.size() - 1) * sizeof(int), "realloc faces");
    bot->num_faces = facelist.size() - 1;

    /* Unbounded face is the first record, so skip */
    for (unsigned int i = 1; i < facelist.size(); ++i) {

	edge = facelist[i].start_edge;
	for (int j = 0; j < VERTICES_PER_FACE; ++j) {

	    bot->faces[VERTICES_PER_FACE * (i - 1) + j] = edge->edge_id.first;
	    edge = edge->next;
	}
    }
}

void
BrlcadMesh::setVertexCoords(int ID)
{
    for (int i = 0; i < 3; i++) {
	bot->vertices[3 * ID + i] = vertexlist[ID].coordinates[i];
    }
}

void BrlcadMesh::setVertex(int vertex, int vertex_to_be_replaced)
{
    for (int i = 0; i < getNumFaces(); ++i) {
	for (int j = 0; j < VERTICES_PER_FACE; ++j) {
	    if (bot->faces[VERTICES_PER_FACE * i + j] == vertex_to_be_replaced)
		bot->faces[VERTICES_PER_FACE * i + j] = vertex;
	}
    }
}

void
BrlcadMesh::deleteVertex(int ID)
{
    for (int i = ID ; i < getNumVertices() - 1; i++) {
	for (int j = 0; j < 3; j++) {
	    bot->vertices[3 * i + j] = bot->vertices[3 * (i + 1) + j];
	}
    }

    for (unsigned int i = 0; i < bot->num_faces; i++) {
	for (int j = 0; j < VERTICES_PER_FACE; j++) {
	    if (bot->faces[3* i + j] > ID) {
		bot->faces[3* i + j]--;
	    }
	}
    }

    bot->num_vertices -= 1;
    /*bot->vertices = (fastf_t*)bu_realloc(bot->vertices, 3 * getNumVertices() * sizeof(fastf_t), "realloc vertices");*/
}

void
BrlcadMesh::deleteFace(int ID)
{
    for (int i = ID ; i < (getNumFaces() - 1); i++) {
	for (int j = 0; j < VERTICES_PER_FACE; j++) {
	    bot->faces[3 * i + j] = bot->faces[3 * (i + 1) + j];
	}
    }
    bot->num_faces -= 1;

    /*bot->faces = (int*)bu_realloc(bot->faces, 3 * getNumFaces() * sizeof(int), "realloc vertices");*/
}

void
BrlcadMesh::addFace()
{
    int *new_faces = (int*)bu_realloc(bot->faces, 3 * (getNumFaces() + 1) * sizeof(int), "realloc faces");
    bot->faces = new_faces;
    int ID = facelist.size() - 1;
    DCEL_Edge *trav_edge = facelist[ID].start_edge;

    for (int i = 0; i < VERTICES_PER_FACE; i++) {
	bot->faces[3 * (ID - 1) + i] = trav_edge->edge_id.first;
	trav_edge = trav_edge->next;
    }

    bot->num_faces += 1;
}

void
BrlcadMesh::addVertex(int ID)
{
    fastf_t *new_vertices = (fastf_t*)bu_realloc(bot->vertices, 3 * (getNumVertices() + 1) * sizeof(fastf_t), "realloc vertices");

    bot->vertices = new_vertices;

    for (int i = 0; i < 3; i++) {
	bot->vertices[3 * ID + i] = vertexlist[ID].coordinates[i];
    }
    bot->num_vertices += 1;
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
