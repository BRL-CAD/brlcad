/*		T R A N S F O R M _ N O D E. C P P
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
/** @file vrml/transform_node.cpp
 *
 * Class implementation for node transformation class
 *
 */


#include "common.h"

#include "transform_node.h"

#include "node.h"
#include "parser.h"

using namespace std;

//Peforms transforms on transform node children
void
TRANSFORM::transformChild(NODE *pnode)
{
    PARSER parse;
    vector<NODE*> mychildlist;
    unsigned int count;
    double tempvec[3];
    double temprotvec[3];

    parse.getChildNodeList(pnode, mychildlist);

    for (count = 0; count < mychildlist.size(); count++) {

	if (mychildlist[count]->vertics.size()) {

	    for (unsigned int i = 0; i < (mychildlist[count]->vertics.size()); i+=3 ) {
		tempvec[0] = mychildlist[count]->vertics[i];
		tempvec[1] = mychildlist[count]->vertics[i+1];
		tempvec[2] = mychildlist[count]->vertics[i+2];

		VEC3X4MAT(temprotvec, tempvec, pnode->rotmat);  //Mutiply vector by rotation matrix
		//Do scaling and translation
		if (mychildlist[count]->ispoly || (mychildlist[count]->nnodetype == NODE_BOX)) {
		    mychildlist[count]->vertics[i] = (temprotvec[0]*pnode->scale[0])+pnode->translation[0] ;
		    mychildlist[count]->vertics[i+1] = (temprotvec[1]*pnode->scale[1])+pnode->translation[1];
		    mychildlist[count]->vertics[i+2] = (temprotvec[2]*pnode->scale[2])+pnode->translation[2];
		} else {
		    mychildlist[count]->vertics[i] = (temprotvec[0]*pnode->scale[0]);
		    mychildlist[count]->vertics[i+1] = (temprotvec[1]*pnode->scale[1]);
		    mychildlist[count]->vertics[i+2] = (temprotvec[2]*pnode->scale[2]);
		}
	    }

	    //Translating center point for primitives
	    if (!mychildlist[count]->ispoly && (mychildlist[count]->nnodetype != NODE_BOX)) {
		mychildlist[count]->vertics[0] += pnode->translation[0];
		mychildlist[count]->vertics[1] += pnode->translation[1];
		mychildlist[count]->vertics[2] += pnode->translation[2];
	    }

	}
    }
}

//Vrml rotation matrix routine
void
TRANSFORM::matrotate(double *output, double angle, double x, double y, double z)
{
    double cosxy = cos(angle);
    double sinxy = sin(angle);

    output[0] = x*x + (y*y+z*z)*cosxy;
    output[1] = x*y - x*y*cosxy + z*sinxy;
    output[2] = x*z - x*z*cosxy - y*sinxy;
    output[4] = x*y - x*y*cosxy - z*sinxy;
    output[5] = y*y + (x*x+z*z)*cosxy;
    output[6] = z*y - z*y*cosxy + x*sinxy;
    output[8] = z*x - z*x*cosxy + y*sinxy;
    output[9] = z*y - z*y*cosxy - x*sinxy;
    output[10]= z*z + (x*x+y*y)*cosxy;
    output[3] = 0;
    output[7] = 0;
    output[11] = 0;
    output[12] = 0;
    output[13] = 0;
    output[14] = 0;
    output[15] = 1;

}

void
TRANSFORM::transformSceneVert(vector<NODE *> &scenenoderef)
{
    int size = static_cast<int>(scenenoderef.size());
    int i;

    for (i = size - 1; i >= 0; i--) {
	if (scenenoderef[i]->nnodetype == NODE_TRANSFORM) {
	    transformChild(scenenoderef[i]);
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
