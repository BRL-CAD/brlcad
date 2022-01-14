/*			 N O D E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
/** @file vrml/node.cpp
 *
 * Routines for node utility and description (node class)
 *
 */


#include "common.h"

#include "node.h"

#include "string_util.h"
#include "node_type.h"
#include "transform_node.h"

#include <vector>
#include <algorithm>
#include <iterator>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "bn.h"
#include "bu.h"


using namespace std;

static const char *fields[] = {
    "",
    "rotation",
    "scaleFactor",
    "translation",
    "set_orientation",
    "set_position",
    "solid",
    "tessellation",
    "scale",
    "radius",
    "point",
    "normal",
    "color",
    "index",
    "position",
    "scaleOrientation",
    "location",
    "colorPerVertex",
    "center",
    "normalPerVertex",
    "segments",
    "texture",
    "colorIndex",
    "geometry",
    "data",
    "string",
    "children",
    "bottomRadius",
    "name",
    "texCoord",
    "texCoordIndex",
    "url",
    "coord",
    "shaders",
    "coordIndex",
    "orientation",
    "appearance",
    "height",
    "size",
    "bottom",
    "side",
    "top",
    "diffuseColor"
};


char *getNextWord(char *nextword);
char *nextWord(char *inputstring, char *nextwd);
void getSFVec3f(float *p);
void getSFVec4f(float *p);
void getInt(int &n);
void getFloat(float &n);
void getCoordIndex(vector<int> &ccoordindex);
void getPoint(vector<float> &cpoint);

//Initialize transform node data
void
NODE::initTransform(NODE *node)
{
    node->scale[0] = 1; node->scale[1] = 1; node->scale[2] = 1;
    node->rotation[0] = 0; node->rotation[1] = 0; node->rotation[2] = 0; node->rotation[3] = 0;
    node->translation[0] = 0; node->translation[1] = 0; node->translation[2] = 0;
}


NODE *
NODE::createNewNode(int nodetype)
{
    NODE *node = NULL;

    switch (nodetype) {
	case NODE_APPEARANCE:
	    node = new NODE;
	    node->nnodetype = NODE_APPEARANCE;
	    node->nodetypename = "Appearance";
	    return node;
	case NODE_BOX:
	    node = new NODE;
	    node->ispoly = 0;
	    node->nnodetype = NODE_BOX;
	    node->nodetypename = "Box";
	    node->size[0] = 2.0f;node->size[1] = 2.0f;node->size[2] = 2.0f;
	    return node;
	case NODE_CONE:
	    node = new NODE;
	    node->ispoly = 0;
	    node->nnodetype = NODE_CONE;
	    node->nodetypename = "Cone";
	    node->height = 2.0f;
	    node->bottomradius = 1.0f;
	    return node;
	case NODE_COORDINATE:
	    node = new NODE;
	    node->nnodetype = NODE_COORDINATE;
	    node->nodetypename = "Coordinate";
	    return node;
	case NODE_CYLINDER:
	    node = new NODE;
	    node->ispoly = 0;
	    node->nnodetype = NODE_CYLINDER;
	    node->nodetypename = "Cylinder";
	    node->radius = node->bottomradius= 1.0f;
	    node->height = 2.0f;
	    return node;
	case NODE_GROUP:
	    node = new NODE;
	    node->nnodetype = NODE_GROUP;
	    node->nodetypename = "Group";
	    return node;
	case NODE_INDEXEDFACESET:
	    node = new NODE;
	    node->ispoly = 1;
	    node->nnodetype = NODE_INDEXEDFACESET;
	    node->nodetypename = "IndexedFaceSet";
	    return node;
	case NODE_INDEXEDLINESET:
	    node = new NODE;
	    node->ispoly = 1;
	    node->nnodetype = NODE_INDEXEDLINESET;
	    node->nodetypename = "IndexedLineSet";
	    return node;
	case NODE_MATERIAL:
	    node = new NODE;
	    node->nnodetype = NODE_MATERIAL;
	    node->nodetypename = "Material";
	    node->diffusecolor[0] = 0.8f;
	    node->diffusecolor[1] = 0.8f;
	    node->diffusecolor[2] = 0.8f;
	    return node;
	case NODE_SHAPE:
	    node = new NODE;
	    node->nnodetype = NODE_SHAPE;
	    node->nodetypename = "Shape";
	    return node;
	case NODE_SPHERE:
	    node = new NODE;
	    node->ispoly = 0;
	    node->nnodetype = NODE_SPHERE;
	    node->nodetypename = "Sphere";
	    node->radius = 1.0f;
	    return node;
	case NODE_TRANSFORM:
	    node = new NODE;
	    node->nnodetype = NODE_TRANSFORM;
	    node->nodetypename = "Transform";
	    memcpy(node->rotmat,bn_mat_identity,sizeof(double)*16);
	    initTransform(node);
	    return node;
	case NODE_PROTO:
	    node = new NODE;
	    node->nnodetype = NODE_PROTO;
	    node->nodetypename = "PROTO";
	    node->isprotodef = 0;
	    return node;
	default :
	    return NULL;
    }

}

//Creates a copy of source node as destination node
//Uses by PROTO and DEF node to create instances of their nodes
int
NODE::copyNode(NODE *destnode, NODE *sourcenode)
{
    unsigned int i;
    int ntype;

    if (sourcenode->children.size()) {
	for (i = 0; i < sourcenode->children.size(); i++) {
	    NODE *nptr;
	    NODETYPE tnode;
	    NODE tmpnode;
	    ntype = tnode.findNodeType(sourcenode->children[i]);
	    nptr =  tmpnode.createNewNode(ntype);
	    nptr->copyNodeData(nptr, sourcenode->children[i], ntype);
	    destnode->children.push_back(nptr);
	    copyNode(destnode->children[i], sourcenode->children[i]);
	}
    } else {
	return 1;
    }

    return 0;
}

//Copy data from source to destination node
int
NODE::copyNodeData(NODE *dnode, NODE *snode, int nodetype)
{
    dnode->nnodetype = snode->nnodetype;

    switch (nodetype) {
	case NODE_APPEARANCE:
	    return 1;
	case NODE_BOX:
	    dnode->ispoly = 0;
	    dnode->size[0] = snode->size[0];
	    dnode->size[1] = snode->size[1];
	    dnode->size[2] = snode->size[2];
	    return 1;
	case NODE_CONE:
	    dnode->ispoly = 0;
	    dnode->height = snode->height;
	    dnode->bottomradius = snode->bottomradius;
	    return 1;
	case NODE_COORDINATE:
	    dnode->point = snode->point;
	    return 1;
	case NODE_CYLINDER:
	    dnode->ispoly = 0;
	    dnode->radius = snode->radius;
	    dnode->height = snode->height;
	    return 1;
	case NODE_GROUP:
	    return 1;
	case NODE_INDEXEDFACESET:
	    dnode->coordindex = snode->coordindex;
	    dnode->ispoly = 1;
	    return 1;
	case NODE_INDEXEDLINESET:
	    dnode->coordindex = snode->coordindex;
	    dnode->ispoly = 1;
	    return 1;
	case NODE_MATERIAL:
	    dnode->diffusecolor[0] = snode->diffusecolor[0];
	    dnode->diffusecolor[1] = snode->diffusecolor[1];
	    dnode->diffusecolor[2] = snode->diffusecolor[2];
	    return 1;
	case NODE_SHAPE:
	    return 1;
	case NODE_SPHERE:
	    dnode->ispoly = 0;
	    dnode->radius = snode->radius;
	    return 1;
	case NODE_TRANSFORM:
	    dnode->scale[0] = snode->scale[0];
	    dnode->scale[1] = snode->scale[1];
	    dnode->scale[2] = snode->scale[2];
	    dnode->rotation[0] = snode->rotation[0];
	    dnode->rotation[1] = snode->rotation[1];
	    dnode->rotation[2] = snode->rotation[2];
	    dnode->rotation[3] = snode->rotation[3];
	    dnode->translation[0] = snode->translation[0];
	    dnode->translation[1] = snode->translation[1];
	    dnode->translation[2] = snode->translation[2];
	    memcpy((void *)dnode->rotmat, (void *)snode->rotmat, sizeof(double)*16);
	    return 1;
	case NODE_PROTO:
	    return 1;
	default :
	    return 0;
     }
}

int
NODE::findFieldName(char *instring)
{
    int i;

	for (i = 1; i < FIELDNAMEMAX ; i++) {
	    if (bu_strcmp(instring, fields[i]) == 0) {
		return i;
	    }
	}

	return 0;
}

int
NODE::getField(int fieldname, NODE *node)
{
    switch (fieldname) {
	case FIELD_rotation:
	    TRANSFORM trans;
	    getSFVec4f(node->rotation);
	    trans.matrotate(rotmat, node->rotation[3], node->rotation[0], node->rotation[1], node->rotation[2]);
	    return 1;
	case FIELD_scaleFactor:
	    return 1;
	case FIELD_translation:
	    getSFVec3f(node->translation);
	    return 1;
	case FIELD_scale:
	    getSFVec3f(node->scale);
	    return 1;
	case FIELD_radius:
	    getFloat(node->radius);
	    return 1;
	case FIELD_point:
	    getPoint(node->point);
	    return 1;
	case FIELD_scaleOrientation:
	    break;
	case FIELD_bottomRadius:
	    getFloat(node->bottomradius);
	    return 1;
	case FIELD_coord:
	    break;
	case FIELD_coordIndex:
	    getCoordIndex(node->coordindex);
	    return 1;
	case FIELD_height:
	    getFloat(node->height);
	    return 1;
	case FIELD_size:
	    getSFVec3f(node->size);
	    return 1;
	case FIELD_bottom:
	    break;
	case FIELD_side:
	    break;
	case FIELD_top:
	    break;
	case FIELD_diffuseColor:
	    getSFVec3f(node->diffusecolor);
	    return 1;
	case FIELD_color:
	    getSFVec3f(node->color);
	    return 1;
	default :
	    break;
	}
     return 0;
}

//Get cone primitive points
void
NODE::getCone(NODE *node)
{
    float h = (node->height);
    float r = node->bottomradius;
    float h2 = (node->height/2);

    node->vertics.push_back(0);
    node->vertics.push_back(0);
    node->vertics.push_back(0);

    node->vertics.push_back(0);
    node->vertics.push_back(0);
    node->vertics.push_back(h);

    node->vertics.push_back(0.001);
    node->vertics.push_back(0);
    node->vertics.push_back(0);

    node->vertics.push_back(0);
    node->vertics.push_back(0.001);
    node->vertics.push_back(0);

    node->vertics.push_back(r);
    node->vertics.push_back(0);
    node->vertics.push_back(0);

    node->vertics.push_back(0);
    node->vertics.push_back(r);
    node->vertics.push_back(0);

    rotXNode(node, 1.57);
    node->vertics[1] +=(h2);

}
//Get box primitive points
void
NODE::getBox(NODE *node) {

    float x = node->size[0]/2;
    float y = node->size[1]/2;
    float z = node->size[2]/2;

    node->vertics.push_back(x);
    node->vertics.push_back(-y);
    node->vertics.push_back(-z);

    node->vertics.push_back(x);
    node->vertics.push_back(y);
    node->vertics.push_back(-z);

    node->vertics.push_back(x);
    node->vertics.push_back(y);
    node->vertics.push_back(z);

    node->vertics.push_back(x);
    node->vertics.push_back(-y);
    node->vertics.push_back(z);

    node->vertics.push_back(-x);
    node->vertics.push_back(-y);
    node->vertics.push_back(-z);

    node->vertics.push_back(-x);
    node->vertics.push_back(y);
    node->vertics.push_back(-z);

    node->vertics.push_back(-x);
    node->vertics.push_back(y);
    node->vertics.push_back(z);

    node->vertics.push_back(-x);
    node->vertics.push_back(-y);
    node->vertics.push_back(z);

}

//Get cylinder primitive points
void
NODE::getCylinder(NODE *node)
{
    float h = (node->height);
    float r = node->radius;
    float h2 = (node->height/2);

    node->vertics.push_back(0);
    node->vertics.push_back(0);
    node->vertics.push_back(0);

    node->vertics.push_back(0);
    node->vertics.push_back(0);
    node->vertics.push_back(h);

    node->vertics.push_back(r);
    node->vertics.push_back(0);
    node->vertics.push_back(0);

    node->vertics.push_back(0);
    node->vertics.push_back(r);
    node->vertics.push_back(0);

    rotXNode(node, 1.57);
    node->vertics[1] +=(h2);


}

//Get sphere primitive points
void
NODE::getSphere (NODE *node) {
    float r = node->radius;

    node->vertics.push_back(0);
    node->vertics.push_back(0);
    node->vertics.push_back(0);

    node->vertics.push_back(r);
    node->vertics.push_back(0);
    node->vertics.push_back(0);

    node->vertics.push_back(0);
    node->vertics.push_back(r);
    node->vertics.push_back(0);

    node->vertics.push_back(0);
    node->vertics.push_back(0);
    node->vertics.push_back(r);
}

void
NODE::rotXNode(NODE *node, double rad) {
    double tempvec[3];
    double rotmatt[16] = {0};
    TRANSFORM trans;

    trans.matrotate(rotmatt, rad, 1, 0, 0);
    for (unsigned int i = 0; i < node->vertics.size(); i+=3 ) {
	tempvec[0] = node->vertics[i];
	tempvec[1] = node->vertics[i+1];
	tempvec[2] = node->vertics[i+2];
	VEC3X4MAT(&node->vertics[i], tempvec, rotmatt);  //Mutiply vector by rotation matrix
    }

}


//Stores vertice for polygonal nodes
void
NODE::getPolyRep(NODE *node) {

    int indexsize = static_cast<int>(node->coordindex.size());
    int i;

    for (i = 0; i < indexsize; i++) {
	node->vertics.push_back(node->point[node->coordindex[i]*3]);
	node->vertics.push_back(node->point[node->coordindex[i]*3+1]);
	node->vertics.push_back(node->point[node->coordindex[i]*3+2]);
    }
}

//Gets and stores vertices for nodes pointed to by noderef vector
void
NODE::doMakePoly(vector<NODE*> &noderef)
{

    int ssize = static_cast<int>(noderef.size());
    int i;

    for (i = 0; i < ssize; i++) {
	if (noderef[i]->nnodetype == NODE_CONE) {
	    noderef[i]->getCone(noderef[i]);
	}else if (noderef[i]->nnodetype == NODE_BOX) {
	    noderef[i]->getBox(noderef[i]);
	}else if (noderef[i]->nnodetype == NODE_CYLINDER) {
	    noderef[i]->getCylinder(noderef[i]);
	}else if (noderef[i]->nnodetype == NODE_SPHERE) {
	    noderef[i]->getSphere(noderef[i]);
	}else if (noderef[i]->nnodetype == NODE_INDEXEDLINESET) {
	    noderef[i]->point = noderef[i]->children[0]->point;
	    noderef[i]->getPolyRep(noderef[i]);
	}else if (noderef[i]->nnodetype == NODE_INDEXEDFACESET) {
	    noderef[i]->point = noderef[i]->children[0]->point;
	    noderef[i]->getPolyRep(noderef[i]);
	}
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
