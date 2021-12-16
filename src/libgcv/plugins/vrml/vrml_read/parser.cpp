/*			 P A R S E R . C P P
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
/** @file vrml/parser.cpp
 *
 * Routine for class PARSER
 * parsing nodes and storing in scene graph
 *
 */


#include "common.h"

#include "parser.h"

#include <vector>
#include <algorithm>
#include <iterator>
#include <cstring>

#include "bu.h"

#include "string_util.h"
#include "node.h"
#include "node_type.h"
#include "transform_node.h"

using namespace std;

char *getNextWord(char *instring, char *nextword);
char *getNextWord( char *nextword);
bool findKeyWord(char *inputstring, int kw);
void stringcopy(string &str1, char *str2);
int stringcompare(string &str1, char *str2);
int findFieldName(char *instring);


//Main parser which calls sub parsers and stores parent node of
//the scenegraph as children of rootnode
void
PARSER::parseScene(char *string) {
    char nextwd[MAXSTRSIZE];

    while (getNextWord(string, nextwd)) {
	NODE *node = NULL;

	if ((node = parseNodeString(nextwd, node))) {
	    if (node != NULL) {
		rootnode.children.push_back(node);
		ntopchild++;
		continue;
	    }
	}
    }
}

//Calls parsers for different keywords
NODE *
PARSER::parseNodeString(char *instring, NODE *node) {
    if (findKeyWord(instring, KWDEF)) {
	if ((node = parsekwdef(node)))
	return node;
    }

    if (findKeyWord(instring, KWUSE)) {
	if ((node = parsekwuse(instring, node)))
	return node;
    }

    if (findKeyWord(instring, KWPROTO)) {
	char protoname[MAXSTRSIZE];
	int sqbrcount = 0;
	string protostring;

	getNextWord(protoname);
	stringcopy(protostring, protoname);

	while (getNextWord(protoname)) {
	    if (*protoname == '[') {
		sqbrcount++;
	    }

	    if (*protoname == ']') {
		sqbrcount--;
		if (sqbrcount == 0) {
		    break;
		}
	    }
	}

	if ((node = parseNode(instring, node))) {
	    protodeftypes.push_back(protostring);
	    protonodelist.push_back(node);
	    return NULL;
	}
    }

    if ((node = parseProtoNode(instring, node))) {
	return node;
    }


    if ((node = parseNode(instring, node))) {
	return node;
    }

    return NULL;
}

//Parses the DEF keyword
NODE *
PARSER::parsekwdef(NODE * node)
{
    char nextwd[MAXSTRSIZE];
    string defstring;

    getNextWord(nextwd); //get the defnode idname
    stringcopy(defstring, nextwd);// convert defnode idname from type char *to c++ string
    getNextWord(nextwd);

    node = parseNode(nextwd, node);
    if (node != NULL) {
	userdeftypes.push_back(defstring); // save defnode idname in userdeftype vector
	defnodelist.push_back(node); //stores pointer to node found in defnodelist
    }else
	return NULL;

    return node;

}

//Parser for the USE keyword
NODE *
PARSER::parsekwuse(char *instring, NODE *node)
{
    int i, ntype;

    getNextWord(instring);

    for (i = userdeftypes.size() - 1; i >= 0 ; i--) {
	if (stringcompare(userdeftypes[i], instring) == 0) { //Search for node in userdeftype vector
	    NODETYPE tnode;
	    NODE tmpnode;
	    ntype = tnode.findNodeType(defnodelist[i]);
	    node = tmpnode.createNewNode(ntype);  //Create instances of node and copies node data
	    node->copyNodeData(node, defnodelist[i], ntype);
	    node->copyNode(node, defnodelist[i]);
	    return node;
	}
    }

    return NULL;
}

//Final parser, parses node and get data/fields
NODE *
PARSER::parseNode(char *instring, NODE *node)
{

    string nextwd;
    NODETYPE ntype;
    NODE tempnode;
    int nodeid, curly = 0;
    char nextstr[MAXSTRSIZE];

    stringcopy(nextwd, instring);
    nodeid = ntype.findNodeType(nextwd);
    node = parseProtoNode(instring, node);//Check first for PROTO  node

    if (node != NULL) {
	return node; // If node is  PROTO then return pointer to node
    }

    if (!nodeid) return NULL;

    node = tempnode.createNewNode(nodeid);
    getNextWord(nextstr);


    if (bu_strcmp("{", nextstr) == 0) //Ensure open curly bracket after node name
    {
	curly++;
    } else {
	return NULL;
    }

    while (getNextWord(nextstr)) {
	NODE *tnodeptr = NULL;

	if (int n = node->findFieldName(nextstr)) {  //Get field for node
	    node->getField(n, node);
	}

	if ((tnodeptr = parseNodeString(nextstr, tnodeptr))) {  //Parse children node and store in scenegraph
	    if (tnodeptr != NULL) {
		node->children.push_back(tnodeptr);
		continue;
	    }
	}

	if (bu_strcmp("{", nextstr) == 0) {
	    curly++;
	    continue;
	}

	if (bu_strcmp("}", nextstr) == 0) {
	    curly--;
	    if (curly == 0) {
		break;
	    }
	}
    }
    return node;
}

//Get list of children for specified node
int
PARSER::getChildNodeList(NODE *rtnode, vector<NODE*> &childlist)
{
    unsigned int i;

    if (rtnode->children.size()) {
	for (i = 0; i < rtnode->children.size(); i++) {
	    childlist.push_back(rtnode->children[i]);
	    getChildNodeList(rtnode->children[i], childlist);
	}
    } else {
	return 0;
    }

    return 1;
}

//Parser for PROTO node
NODE *
PARSER::parseProtoNode(char *instring, NODE *node)
{
    int i;

    for (i = protodeftypes.size() - 1; i >= 0 ; i--) {  //Searches for node in protodeftypes vector
	if (stringcompare(protodeftypes[i], instring) == 0) {
	    NODETYPE tnode;
	    NODE tmpnode;
	    node = tmpnode.createNewNode(tnode.findNodeType(protonodelist[i]));
	    node->copyNode(node, protonodelist[i]);
	    return node;
	}
    }
    return NULL;
}

NODE *
PARSER::findChild(NODE *node, int search) {
    int size;
    int i;
    NODETYPE ntype;
    vector<NODE*> childlist;

    getChildNodeList(node,childlist);
    size = static_cast<int>(childlist.size());

    for (i = 0; i < size; i++) {
	if (ntype.findNodeType(childlist[i]) == search) {
	    return childlist[i];
	}
    }

    return NULL;
}

void
PARSER::doColor(vector<NODE*> &childlist) {

    int size = static_cast<int>(childlist.size());
    int i;
    int mat = NODE_MATERIAL;
    NODE *matnode;
    unsigned int l;

    for (i = 0; i < size; i++) {
	if (childlist[i]->nnodetype == NODE_SHAPE) {
	    matnode = findChild(childlist[i], mat);
	    if (matnode) {
		for (l = 0; l < childlist[i]->children.size(); l++) {
		    memcpy((void *)childlist[i]->children[l]->diffusecolor, (void *)matnode->diffusecolor, sizeof(float)*3);
		}
	    }
	}
    }
}

void
PARSER::freeSceneNode(vector<NODE *> &childlist) {

    int size = static_cast<int>(childlist.size());
    int i;

    for (i = size-1; i >= 0; i--) {
	delete childlist[i];
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
