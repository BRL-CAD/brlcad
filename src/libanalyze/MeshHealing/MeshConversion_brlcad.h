/*         M E S H C O N V E R S I O N _ B R L C A D . H
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
/** @file MeshConversion_brlcad.h
 *
 * Definitions of the functions that let the PolygonalMesh object interact with the bot type
 *
 */

#ifndef SRC_LIBANALYZE_MESHHEALING_MESHCONVERSION_BRLCAD_H_
#define SRC_LIBANALYZE_MESHHEALING_MESHCONVERSION_BRLCAD_H_

#include "./MeshConversion.h"


struct rt_bot_internal;


class BrlcadMesh: public PolygonalMesh {

public:
    BrlcadMesh(rt_bot_internal *bot_mesh);

private:
    rt_bot_internal *bot;

    int getNumVertices();
    void initVertices();
    int getNumFaces();
    void initStartEdge();
    void initEdges();
    void initIncidentFace();
    void initPrevEdge();
    void initNextEdge();

    void setNumVertices();
    void setNumFaces();
    void setVertices();
    void setFaces();
    void setVertexCoords(int ID);
    void setVertex(int v1, int v2);
    void deleteVertex(int ID);
    void deleteFace(int ID);
    void addFace();
    void addVertex(int ID);

    /* Test function
    void createBot();*/
};

#endif /* SRC_LIBANALYZE_MESHHEALING_MESHCONVERSION_BRLCAD_H_ */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
