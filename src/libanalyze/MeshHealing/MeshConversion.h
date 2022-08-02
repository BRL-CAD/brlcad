/*                M E S H C O N V E R S I O N . H
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
/** @file MeshConversion.h
 *
 * Definition of the PolygonalMesh class, and the utility methods for the mesh healing module
 *
 */

#ifndef SRC_LIBANALYZE_MESHHEALING_MESHCONVERSION_H_
#define SRC_LIBANALYZE_MESHHEALING_MESHCONVERSION_H_

#include "common.h"

#include <utility>
#include <vector>

#include "DCEL.h"


#define VERTICES_PER_FACE 3
#define UNBOUNDED_FACE 0


class PolygonalMesh {

protected:

    /* DCEL records */
    std::vector<DCEL_Vertex> vertexlist;
    std::vector<DCEL_Face> facelist;
    std::vector<DCEL_Edge> edgelist;

    /* Virtual functions to update the native structures */

    /* DCEL_Vertex record functions */

    /* Returns number of vertices */
    virtual int getNumVertices() = 0;
    /* Sets the DCEL_Vertex IDs and coordinates for all the vertices */
    virtual void initVertices() = 0;
    /* For every DCEL_Vertex, sets an arbitrary DCEL_Vertex incident on it */
    void initIncidentEdge();

    /* DCEL_Face record functions 
    */

    /* Returns number of faces */
    virtual int getNumFaces() = 0;

    /* Sets a DCEL_Face ID for each DCEL_Face */
    void initFaces();

    /* Sets an arbitrary DCEL_Edge that is a prt of the DCEL_Face, so that that it can be traversed in counter-clockwise order */ 
    virtual void initStartEdge() = 0;

    /* DCEL_Edge record functions */

    /* Sets DCEL_Edge ID and the origin of the DCEL_Edge */
    virtual void initEdges() = 0;

    /* Sets the DCEL_Face to the half-DCEL_Edge's left */
    virtual void initIncidentFace() = 0;

    /* Sets the twin DCEL_Edge of the half-DCEL_Edge */
    void initTwinEdge();

    /* Sets the DCEL_Edge previous to this DCEL_Edge in the incident DCEL_Face */
    virtual void initPrevEdge() = 0;

    /* Sets the DCEL_Edge next to this DCEL_Edge in the incident DCEL_Face */
    virtual void initNextEdge() = 0;

    virtual void setNumVertices() = 0;
    virtual void setNumFaces() = 0;
    virtual void setVertexCoords(int ID) = 0;
    virtual void setVertex(int v1, int v2) = 0;
    virtual void deleteVertex(int ID) = 0;
    virtual void deleteFace(int ID) = 0;
    virtual void addFace() = 0;
    virtual void addVertex(int ID) = 0;

    /* Test function to create bot from scratch
    virtual void createBot();*/

public:
    virtual void setVertices() = 0;
    virtual void setFaces() = 0;

    std::vector<bool> is_edge_checked;

    /* Getters and setters */

    /* Get vertex with vertex id=ID */
    DCEL_Vertex* getVertex(int ID);
    /* Get edge with edge id=ID */
    DCEL_Edge* getEdge(std::pair<int, int> ID);
    /* Returns index of the edge given it's ID pair */
    int getEdgeIndex(std::pair<int, int> ID, int face_id);
    /* Get face with face id=ID */
    DCEL_Face* getFace(int ID);
    /* Set the coordinates of the vertex with vertex id=ID */
    void setVertexCoordsInRecord(int ID, double coordinates[3]);
    /* Replace all references of a vertex to be replaced with the other vertex */
    void setVertexRecord(DCEL_Vertex *vertex, DCEL_Vertex *vertex_to_be_replaced);
    /* Deletes a vertex with vertex id=ID */
    void deleteVertexRecord(int ID);
    /* Insert a vertex on the half edge */
    void insertVertexOnEdge(DCEL_Vertex *vertex, DCEL_Edge *edge);
    /* Deletes edge with edge id=ID */
    void deleteEdge(std::pair<int, int> ID, int face_id);
    /* Split the face through the line joining the given vertex (on an edge) to the opposite vertex */
    void splitFace(int face_id, DCEL_Vertex *vertex_on_edge, DCEL_Vertex *opp_vertex);
    /* Adds new face and sets the starting edge and returns the ID */
    int addFaceRecord(DCEL_Edge *start_edge);
    /* Adds an edge and sets the DCEL edge record attributes */
    DCEL_Edge* addEdge(std::pair<int, int> edge_id, DCEL_Vertex *origin, DCEL_Edge *twin, DCEL_Face *inc_face, DCEL_Edge *next, DCEL_Edge *prev);
    /* Deletes face with given face_id */
    void deleteFaceRecord(int face_id);
    /* Distance to an edge is the perpendicular distance if an orthogonal projection is possible,
     * else it is the distance to the closer end point.
     * Set the feature edge or the vertex accordingly. Set the other one to NULL.
     * The edge if set, it is the half edge with the unbounded face.
     * Line joining the vertex to the edge should not intersect any existing edge/vertex */
    DCEL_Edge* findClosestEdge(DCEL_Vertex *vertex);
    /* Returns the end point of the edge closer to the vertex */
    DCEL_Vertex* findCloserVertex(DCEL_Edge *edge, DCEL_Vertex *vertex);
    /*Adds new vertex with the given coordinates and returns ID*/
    int addVertexRecord(double coordinates[3]);
    /* Checks if the vertex with the given ID is present in the face with the given ID) */
    bool isVertexInFace(int vertex_id, int face_id);
    /* Checks if an edge is a free edge */
    bool isFreeEdge(DCEL_Edge *edge);
    /* Returns the start and end edges of a free-edge chain */
    DCEL_Edge* findFreeEdgeChain();
    /* Returns number of half edges in the DCEL */
    int getNumEdges();
    /* Checks whether the edge opposite to vertex in face intersects the line from vertex to other_vertx */
    bool doesLineIntersectFaceWithVertex(DCEL_Face *face, DCEL_Vertex *vertex, double other_vertex[3]);
    /* Checks whether line from vertex to the ortho_proj intersects the face */
    bool doesLineIntersectFace(DCEL_Face *face, DCEL_Vertex *vertex, double ortho_proj[3]);
    /* Returns the face adjacent to 'face' around the vertex */
    DCEL_Face* getNextFace(DCEL_Face *face, DCEL_Vertex *vertex);
    /* Checks whether the line from vertex to closer_vertex crosses any faces around vertex */
    bool checkEligibleEdge(DCEL_Vertex *vertex, double closer_vertex[3]);
    /* Sets the boundary edge of the mesh, and hence the boundary of the mesh can be traversed through this one edge */
    void squeezeOutEdges(DCEL_Edge *edge, DCEL_Edge *next_edge);
    /* Sets twin reference after vertex/edge contraction */
    void checkAndSetTwins(DCEL_Vertex *vertex);
    /* Checking if one edge lies on the chain of another edge */
    bool isEdgeOnChain(DCEL_Edge *edge_to_be_checked, DCEL_Edge *chain_edge);
    /* Returns edge in the given index in the edge list */
    DCEL_Edge* getEdgeWithIndex(int index);

};

#endif /* SRC_LIBANALYZE_MESHHEALING_MESHCONVERSION_H_ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
