/*                         P A R S E R . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2023 United States Government as represented by
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
/** @file vrml/parser.h
 *
 * Class definition for parser class
 *
 */

#ifndef PARSER_H
#define PARSER_H

#include "common.h"

#include "node.h"
#include "string_util.h"

#include "vmath.h"

#include <vector>


class PARSER
{
    public:
    std::vector<std::string> userdeftypes; //Store DEF node names
    std::vector<std::string> protodeftypes; //Store PROTO node names
    std::vector<NODE *> protonodelist; //Stores pointer to PROTO nodes
    std::vector<NODE *> defnodelist;  //Stores pointer to DEF nodes
    std::vector<double> scenevert;  //Store all vertices in the scenegraph
    NODE rootnode;   //Parent scene node
    int ntopchild; // Number of direct children to the root node

    void parseScene(char *string);
    NODE *parseNodeString(char *instring, NODE *node);
    NODE *parsekwdef(NODE *node);
    NODE *parsekwuse(char *instring, NODE *node);
    NODE *parseNode(char *instring, NODE *node);
    int getChildNodeList(NODE *rtnode, std::vector<NODE *> &childlist);
    int copyNode(NODE *destnode, NODE *sourcenode);
    void transformChild(NODE *pnode);
    NODE *parseProtoNode(char *instring, NODE *node);
    NODE *findChild(NODE *node, int search);
    void doColor(std::vector<NODE *> &childlist);
    void freeSceneNode(std::vector<NODE *> &childlist);

};

#endif


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
