/*			 N O D E. H
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file vrml/node.h
 *
 * Class definition for node class
 *
 */

#ifndef NODE_H
#define NODE_H


#include "common.h"

#include "string_util.h"
#include "node_type.h"

#include <vector>

#define PI 3.141592653589793f
#define MAXSTRSIZE 512


#define NODE_APPEARANCE         1
#define NODE_BOX                2
#define NODE_CONE               3
#define NODE_COORDINATE         4
#define NODE_CYLINDER           5
#define NODE_GROUP              6
#define NODE_INDEXEDFACESET     7
#define NODE_INDEXEDLINESET     8
#define NODE_MATERIAL           9
#define NODE_SHAPE              10
#define NODE_SPHERE             11
#define NODE_TRANSFORM          12
#define NODE_PROTO              13

#define FIELD_rotation           1
#define FIELD_scaleFactor        2
#define FIELD_translation        3
#define FIELD_set_orientation    4
#define FIELD_set_position       5
#define FIELD_solid              6
#define FIELD_tessellation       7
#define FIELD_scale              8
#define FIELD_radius             9
#define FIELD_point             10
#define FIELD_normal            11
#define FIELD_color             12
#define FIELD_index             13
#define FIELD_position          14
#define FIELD_scaleOrientation  15
#define FIELD_location          16
#define FIELD_colorPerVertex    17
#define FIELD_center            18
#define FIELD_normalPerVertex   19
#define FIELD_segments          20
#define FIELD_texture           21
#define FIELD_colorIndex        22
#define FIELD_geometry          23
#define FIELD_data              24
#define FIELD_string            25
#define FIELD_children          26
#define FIELD_bottomRadius      27
#define FIELD_name              28
#define FIELD_texCoord          29
#define FIELD_texCoordIndex     30
#define FIELD_url               31
#define FIELD_coord             32
#define FIELD_shaders           33
#define FIELD_coordIndex        34
#define FIELD_orientation       35
#define FIELD_appearance        36
#define FIELD_height            37
#define FIELD_size              38
#define FIELD_bottom            39
#define FIELD_side              40
#define FIELD_top               41
#define FIELD_diffuseColor      42


#define FIELDNAMEMAX            43

using namespace std;

char *getNextWord( char *nextword);
char *nextWord(char *inputstring, char *nextwd);

class NODE
{
public:
    int nnodetype;
    int fieldnumb;
    int nchild;
    int isprotodef;
    int ispoly;
    vector<NODE*> children;
    string nodetypename;
    float color[3];
    float diffusecolor[3];
    float scale[3]; // = {1, 1, 1};
    float rotation[4]; // = {0, 0, 0, 0};
    float translation[3]; // = {0, 0, 0};
    float size[3];
    float radius;
    float bottomradius;
    float height;
    double rotmat[16];
    vector<int> coordindex;
    vector<float> point;
    vector<double> vertics;


    NODE *createNewNode(int nodetype, NODE *node);
    int copyNode(NODE *destnode, NODE *sourcenode);
    int copyNodeData(NODE *dnode, NODE *snode, int nodetype);
    int findFieldName(char *instring);
    int getField(int fieldname, NODE *node);
    void initTransform(NODE *node);
    void getCone(NODE *node);
    void getBox(NODE *node);
    void getCylinder(NODE *node);
    void getSphere (NODE *node);
    void getPolyRep(NODE *node);
    void rotXNode(NODE *node, double rad);
    void doMakePoly(vector<NODE*> &noderef);
};

#endif


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
